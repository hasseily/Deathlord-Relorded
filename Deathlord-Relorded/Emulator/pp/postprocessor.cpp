#include "postprocessor.h"
#include "imgui.h"
#include "imgui_internal.h"		// for PushItemFlag
#include "extras/ImGuiFileDialog.h"
// For save/restore of presets
#include <fstream>
#include <sstream>

#include "glad/glad.h"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define _SHADER_VERTEX_BASIC "shaders/basic.vert"
#define _SHADER_VERTEX_BASIC_TRANSFORM "shaders/basic_with_transform.vert"
#define _SHADER_FRAGMENT_BASIC "shaders/basic.frag"

#define _TEXUNIT_PP_PREVIOUS GL_TEXTURE5		// The previous frame as a texture
#define _TEXUNIT_PP_INPUT GL_TEXTURE6			// A2 Video input texunit the PP will use to generate the final output
#define _TEXUNIT_PP_BEZEL GL_TEXTURE7			// The bezel in postprocessing
#define _TEXUNIT_PP_BEZEL_GLASS GL_TEXTURE8		// The bezel glass in postprocessing

/* @brief
 The PostProcessor will take any texture that's in slot _PP_INPUT_TEXTURE_UNIT and apply the
 postprocessing shader on it. It always dynamically calculates the texture's size and properly
 scales it up in integer steps (or down in fractional steps).

 The PostProcessor shader has 3 modes of operation:
 - Geometry transformations and bezels
 - Add a simple scanline mode that makes every other scanline black (unused for AppleWin that has its own)
 - A full shader mode with a kitchensink of features

 To make it more optimal for low end devices that may not approve of the full shader,
 the transformation mode uses a basic transformation shader instead of the full one,
 although the full shader can handle transformations as well.

 Bezels:
 =======

 Bezels have their own shader applied in a subsequent pass.
 Bezels have a specific format: Each bezel file should be a PNG with transparency. The bezel will
 be applied as a layer on top of the Apple 2 video, so the transparency needs to be available to view the
 Apple 2 video.

 Any transparency > 0 but < 1 (partial transparency) will be considered to be an area used for reflections.
 Reflections are created by displaying in partially transparent areas the out-of-ranged mirrored texture data
 of the Apple 2 video texture. When the user properly scales it and blurs it, the reflection is good and cheap.

 The lower the alpha value, the less translation the shader will make, and the closer to the bezel the light
 will be "reflected".

 Additionally, if the bezel file has a similarly named file that ends in .glass, the bezel shader will overlay
 the .glass texture as a final pass.
 */

namespace sa2 {
	// below because "The declaration of a static data member in its class definition is not a definition"
	PostProcessor* PostProcessor::s_instance;

	//////////////////////////////////////////////////////////////////////////
	// Basic singleton methods
	//////////////////////////////////////////////////////////////////////////

	void PostProcessor::Initialize()
	{
		if (quadVAO == UINT_MAX)
		{
			// We're going to generate a static quad that the vertex shader will transform
			// based on the requested size changes. This way is more efficient than either
			// always creating a new static quad for every request, or using GL_DYNAMIC_DRAW
			GLfloat quadVertices[] = {
				// Positions         // Texture Coords
				-1.0f,  -1.0f,        0.0f, 0.0f,   // Top-left
				1.0f, 1.0f,        1.0f, 1.0f,   // Bottom-right
				1.0f,  -1.0f,        1.0f, 0.0f,   // Top-right

				-1.0f, -1.0f,        0.0f, 0.0f,   // Top-left
				-1.0f,  1.0f,        0.0f, 1.0f,   // Bottom-left
				1.0f,  1.0f,        1.0f, 1.0f    // Bottom-right
			};

			glGenVertexArrays(1, &quadVAO);
			glGenBuffers(1, &quadVBO);

			glBindVertexArray(quadVAO);
			glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
			glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

			// Set up vertex attribute pointers
			// Attribute 0: position (2 floats)
			glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (void*)0);
			glEnableVertexAttribArray(0);
			// Attribute 1: texture coordinates (2 floats)
			glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (void*)(2 * sizeof(GLfloat)));
			glEnableVertexAttribArray(1);

			// Unbind for cleanliness
			glBindBuffer(GL_ARRAY_BUFFER, 0);
			glBindVertexArray(0);
			GLenum glerr;
			if ((glerr = glGetError()) != GL_NO_ERROR) {
				std::cerr << "OpenGL error PP Initialize: " << glerr << std::endl;
			}
		}

		glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTexSize);

		// PP shader list
		v_ppshaders.clear();
		Shader shader_basic = Shader();
		shader_basic.Build(_SHADER_VERTEX_BASIC_TRANSFORM, _SHADER_FRAGMENT_BASIC);
		v_ppshaders.push_back(shader_basic);
		Shader shader_pp = Shader();
		shader_pp.Build("shaders/a2video_postprocess.glsl", "shaders/a2video_postprocess.glsl");
		v_ppshaders.push_back(shader_pp);
		memset(preset_name_buffer, 0, sizeof(preset_name_buffer));

		//Bezel shader
		shaderProgramBezel = Shader();
		shaderProgramBezel.Build("shaders/overlay_bezel.glsl", "shaders/overlay_bezel.glsl");

		viewportWidth = requestedWidth = 560;
		viewportHeight = requestedHeight = 384;
		RegenerateFBOs();
		ResetToDefaults();
	}

	PostProcessor::~PostProcessor()
	{
		if (quadVAO != UINT_MAX)
		{
			// Cleanup
			glDeleteVertexArrays(1, &quadVAO);
			glDeleteBuffers(1, &quadVBO);
		}
		quadVAO = UINT_MAX;
		quadVBO = UINT_MAX;

		if (FBO != UINT_MAX)
		{
			glDeleteFramebuffers(1, &FBO);
			glDeleteTextures(1, &texture_id);
		}
		FBO = UINT_MAX;

		if (FBO_prevFrame != UINT_MAX)
		{
			glDeleteFramebuffers(1, &FBO_prevFrame);
			glDeleteTextures(1, &prevFrame_texture_id);
		}
		FBO_prevFrame = UINT_MAX;
	}

	//////////////////////////////////////////////////////////////////////////
	// Utility methods
	//////////////////////////////////////////////////////////////////////////
	///
	void HelpMarker(const char *desc)
	{
		ImGui::SameLine();
		ImGui::TextDisabled("(?)");
		if (ImGui::IsItemHovered())
		{
			ImGui::BeginTooltip();
			//ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
			ImGui::TextEx(desc);
			//ImGui::PopTextWrapPos();
			ImGui::EndTooltip();
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// Main methods
	//////////////////////////////////////////////////////////////////////////

	void PostProcessor::ImageAsset::AssignByFilename(const char* filename) {
		int width;
		int height;
		int channels;
		stbi_set_flip_vertically_on_load(true);
		unsigned char* data = stbi_load(filename, &width, &height, &channels, 4);
		stbi_set_flip_vertically_on_load(false);
		if (data == NULL) {
			std::cerr << "ERROR: STBI load failure: " << stbi_failure_reason() << std::endl;
			std::cerr << "Tried loading: " << filename << std::endl;
			image_xcount = 0;
			image_ycount = 0;
			return;
		}
		if (tex_id != UINT_MAX)
		{
			PostProcessor::GetInstance()->LoadTexture(data, width, height, channels, tex_id);
			stbi_image_free(data);
		}
		else {
			std::cerr << "ERROR: Could not bind texture, all slots filled!" << std::endl;
			return;
		}
		GLenum glerr;
		if ((glerr = glGetError()) != GL_NO_ERROR) {
			std::cerr << "ImageAsset::AssignByFilename error: " << glerr << std::endl;
		}
		image_xcount = width;
		image_ycount = height;
	}

	void PostProcessor::LoadTexture(unsigned char* data, int width, int height, int nrComponents, GLuint textureID)
	{
		GLenum glerr;
		GLenum format = GL_RGBA8;
		if (nrComponents == 1)
			format = GL_RED;

		glBindTexture(GL_TEXTURE_2D, textureID);
		if ((glerr = glGetError()) != GL_NO_ERROR) {
			std::cerr << "OpenGL LoadTexture glBindTexture error: " << glerr << std::endl;
		}
		glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
		if ((glerr = glGetError()) != GL_NO_ERROR) {
			std::cerr << "OpenGL LoadTexture glTexImage2D error: " << glerr << std::endl;
		}

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		if ((glerr = glGetError()) != GL_NO_ERROR) {
			std::cerr << "OpenGL LoadTexture glTexParameteri error: " << glerr << std::endl;
		}
	}

	nlohmann::json PostProcessor::SerializeState()
	{
		nlohmann::json jsonState = {
			{"bIsActive", bIsActive},
			{"preset_name", preset_name_buffer},
			{"bezelName", selectedBezelFile},
			{"bezelWidth", bezelSize.x},
			{"bezelHeight", bezelSize.y},
			{"bezelCenterX", bezelCenter.x},
			{"bezelCenterY", bezelCenter.y},
			{"p_f_bezelReflection", p_f_bezelReflection},
			{"p_f_reflectionBlur", p_f_reflectionBlur},
			{"p_i_postprocessingLevel", p_i_postprocessingLevel},
			{"bCRTFillWindow", bCRTFillWindow},
			{"integer_scale", integer_scale},
			{"bAutoScale", bAutoScale},
			{"bCRTFillWindow", bCRTFillWindow},
			{"p_f_corner", p_f_corner},
			{"p_b_smoothCorner", p_b_smoothCorner},
			{"p_b_useOKlab", p_b_useOKlab},
			{"p_f_interlace", p_f_interlace},
			{"p_b_slot", p_b_slot},
			{"p_f_bgr", p_f_bgr},
			{"p_f_black", p_f_black},
			{"p_f_brDep", p_f_brDep},
			{"p_f_brightness", p_f_brightness},
			{"p_f_contrast", p_f_contrast},
			{"p_i_cSpace", p_i_cSpace},
			{"p_f_cStr", p_f_cStr},
			{"p_f_convB", p_f_convB},
			{"p_f_convG", p_f_convG},
			{"p_f_convR", p_f_convR},
			{"p_i_maskType", p_i_maskType},
			{"p_f_maskHigh", p_f_maskHigh},
			{"p_f_maskLow", p_f_maskLow},
			{"p_f_maskSize", p_f_maskSize},
			{"p_f_hue", p_f_hue},
			{"p_f_hueGB", p_f_hueGB},
			{"p_f_hueRB", p_f_hueRB},
			{"p_f_hueRG", p_f_hueRG},
			{"p_f_saturation", p_f_saturation},
			{"p_f_scanlineWeight", p_f_scanlineWeight},
			{"p_f_scanSpeed", p_f_scanSpeed},
			{"p_f_flimGrain", p_f_filmGrain},
			{"p_i_scanlineType", p_i_scanlineType},
			{"p_f_slotW", p_f_slotW},
			{"p_f_vignetteWeight", p_f_vignetteWeight},
			{"p_f_barrelDistortion", p_f_barrelDistortion},
			{"p_f_ghostingPercent", p_f_ghostingPercent},
			{"p_f_phosphorBlur", p_f_phosphorBlur},
			{"p_b_phosphorGlow", p_b_phosphorGlow},
			{"p_v_warpX", p_v_warp.x},
			{"p_v_warpY", p_v_warp.y},
			{"p_v_centerX", p_v_center.x},
			{"p_v_centerY", p_v_center.y},
			{"p_v_zoomX", p_v_zoom.x},
			{"p_v_zoomY", p_v_zoom.y},
			{"p_v_reflectionScaleX", p_v_reflectionScale.x},
			{"p_v_reflectionScaleY", p_v_reflectionScale.y},
			{"p_v_reflectionTranslationX", p_v_reflectionTranslation.x},
			{"p_v_reflectionTranslationY", p_v_reflectionTranslation.y},
			{"p_f_glassThickness", p_f_glassThickness},
		};
		return jsonState;
	}

	void PostProcessor::DeserializeState(const nlohmann::json &jsonState)
	{
		std::strncpy(preset_name_buffer, jsonState.value("preset_name", preset_name_buffer).c_str(), sizeof(preset_name_buffer) - 1);
		selectedBezelFile = jsonState.value("bezelName", selectedBezelFile);
		if (selectedBezelFile != _PP_NO_BEZEL_FILENAME)
		{
			LoadSelectedBezel();
			if (bezelImageAsset.image_xcount == 0)
				selectedBezelFile = _PP_NO_BEZEL_FILENAME;
		}
		bIsActive = jsonState.value("bIsActive", bIsActive);
		bezelSize.x = jsonState.value("bezelWidth", bezelSize.x);
		bezelSize.y = jsonState.value("bezelHeight", bezelSize.y);
		bezelCenter.x = jsonState.value("bezelCenterX", bezelCenter.x);
		bezelCenter.y = jsonState.value("bezelCenterY", bezelCenter.y);
		p_f_bezelReflection = jsonState.value("p_f_bezelReflection", p_f_bezelReflection);
		p_f_reflectionBlur = jsonState.value("p_f_reflectionBlur", p_f_reflectionBlur);
		p_i_postprocessingLevel = jsonState.value("p_i_postprocessingLevel", p_i_postprocessingLevel);
		bCRTFillWindow = jsonState.value("bCRTFillWindow", bCRTFillWindow);
		integer_scale = jsonState.value("integer_scale", integer_scale);
		bAutoScale = jsonState.value("bAutoScale", bAutoScale);
		p_f_corner = jsonState.value("p_f_corner", p_f_corner);
		p_b_smoothCorner = jsonState.value("p_b_smoothCorner", p_b_smoothCorner);
		p_b_useOKlab = jsonState.value("p_b_useOKlab", p_b_useOKlab);
		p_f_interlace = jsonState.value("p_f_interlace", p_f_interlace);
		p_b_slot = jsonState.value("p_b_slot", p_b_slot);
		p_f_bgr = jsonState.value("p_f_bgr", p_f_bgr);
		p_f_black = jsonState.value("p_f_black", p_f_black);
		p_f_brDep = jsonState.value("p_f_brDep", p_f_brDep);
		p_f_brightness = jsonState.value("p_f_brightness", p_f_brightness);
		p_f_contrast = jsonState.value("p_f_contrast", p_f_contrast);
		p_i_cSpace = jsonState.value("p_i_cSpace", p_i_cSpace);
		p_f_cStr = jsonState.value("p_f_cStr", p_f_cStr);
		p_f_convB = jsonState.value("p_f_convB", p_f_convB);
		p_f_convG = jsonState.value("p_f_convG", p_f_convG);
		p_f_convR = jsonState.value("p_f_convR", p_f_convR);
		p_i_maskType = jsonState.value("p_i_maskType", p_i_maskType);
		p_f_maskHigh = jsonState.value("p_f_maskHigh", p_f_maskHigh);
		p_f_maskLow = jsonState.value("p_f_maskLow", p_f_maskLow);
		p_f_maskSize = jsonState.value("p_f_maskSize", p_f_maskSize);
		p_f_hue = jsonState.value("p_f_hue", p_f_hue);
		p_f_hueGB = jsonState.value("p_f_hueGB", p_f_hueGB);
		p_f_hueRB = jsonState.value("p_f_hueRB", p_f_hueRB);
		p_f_hueRG = jsonState.value("p_f_hueRG", p_f_hueRG);
		p_f_saturation = jsonState.value("p_f_saturation", p_f_saturation);
		p_f_scanlineWeight = jsonState.value("p_f_scanlineWeight", p_f_scanlineWeight);
		p_f_scanSpeed = jsonState.value("p_f_scanSpeed", p_f_scanSpeed);
		p_f_filmGrain = jsonState.value("p_f_filmGrain", p_f_filmGrain);
		p_i_scanlineType = jsonState.value("p_i_scanlineType", p_i_scanlineType);
		p_f_slotW = jsonState.value("p_f_slotW", p_f_slotW);
		p_f_vignetteWeight = jsonState.value("p_f_vignetteWeight", p_f_vignetteWeight);
		p_f_barrelDistortion = jsonState.value("p_f_barrelDistortion", p_f_barrelDistortion);
		p_f_ghostingPercent = jsonState.value("p_f_ghostingPercent", p_f_ghostingPercent);
		p_f_phosphorBlur = jsonState.value("p_f_phosphorBlur", p_f_phosphorBlur);
		p_b_phosphorGlow = jsonState.value("p_b_phosphorGlow", p_b_phosphorGlow);
		p_v_warp.x = jsonState.value("p_v_warpX", p_v_warp.x);
		p_v_warp.y = jsonState.value("p_v_warpY", p_v_warp.y);
		p_v_center.x = jsonState.value("p_v_centerX", p_v_center.x);
		p_v_center.y = jsonState.value("p_v_centerY", p_v_center.y);
		p_v_zoom.x = jsonState.value("p_v_zoomX", p_v_zoom.x);
		p_v_zoom.y = jsonState.value("p_v_zoomY", p_v_zoom.y);
		p_v_reflectionScale.x = jsonState.value("p_v_reflectionScaleX", p_v_reflectionScale.x);
		p_v_reflectionScale.y = jsonState.value("p_v_reflectionScaleY", p_v_reflectionScale.y);
		p_v_reflectionTranslation.x = jsonState.value("p_v_reflectionTranslationX", p_v_reflectionTranslation.x);
		p_v_reflectionTranslation.y = jsonState.value("p_v_reflectionTranslationY", p_v_reflectionTranslation.y);
		p_f_glassThickness = jsonState.value("p_f_glassThickness", p_f_glassThickness);

	}

	void PostProcessor::SaveState(std::string filePath) {
		nlohmann::json jsonState = SerializeState();
		std::ofstream file(filePath);
		if (file.is_open()) {
			file << jsonState.dump(4); // Pretty print JSON
			file.close();
		}
	}

	void PostProcessor::LoadState(std::string filePath) {
		std::ifstream file(filePath);
		nlohmann::json jsonState;
		if (file.is_open()) {
			file >> jsonState;
			DeserializeState(jsonState);
		}
	}

	void PostProcessor::LoadSelectedBezel()
	{
		std::string _pathStr = std::string(APP_BASE_PATH()).append("assets/bezels/" + selectedBezelFile);
		glActiveTexture(_TEXUNIT_PP_BEZEL);
		if (bezelImageAsset.tex_id == UINT_MAX)
		{
			glGenTextures(1, &bezelImageAsset.tex_id);
			glBindTexture(GL_TEXTURE_2D, bezelImageAsset.tex_id);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		}
		bezelImageAsset.AssignByFilename(_pathStr.c_str());

		// check if there's a .glass filename as well
		auto pos = _pathStr.find_last_of('.');	// there's always a dot
		_pathStr.replace(pos, 1, ".glass.");
		std::filesystem::path bezelGlassPath = _pathStr;
		if (std::filesystem::is_regular_file(bezelGlassPath)) {
			glActiveTexture(_TEXUNIT_PP_BEZEL_GLASS);
			if (bezelGlassImageAsset.tex_id == UINT_MAX)
			{
				glGenTextures(1, &bezelGlassImageAsset.tex_id);
				glBindTexture(GL_TEXTURE_2D, bezelGlassImageAsset.tex_id);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			}
			bezelGlassImageAsset.AssignByFilename(bezelGlassPath.string().c_str());
		}
		else {
			bezelGlassImageAsset.image_xcount = 0;
			bezelGlassImageAsset.image_ycount = 0;
		}

		glActiveTexture(GL_TEXTURE0);
	}

	int PostProcessor::PopulateBezelFiles(std::vector<std::string>& _bezelFiles, const std::string& _selectedBezelFile) {
		int _selIdx = 0;
		_bezelFiles.clear();
		_bezelFiles.push_back(_PP_NO_BEZEL_FILENAME);  // Default for no bezel

		// populate bezel files
		for (const auto& entry : std::filesystem::directory_iterator(std::string(APP_BASE_PATH()).append("assets/bezels/"))) {
			if (entry.is_regular_file() && (entry.path().extension() == ".png" || entry.path().extension() == ".jpg")) {
				// omit xxx.glass.png or yyy.glass.jpg, the glass layer
				if (entry.path().stem().extension() == ".glass")
					continue;

				_bezelFiles.push_back(entry.path().filename().string());
				if (_selectedBezelFile == entry.path().filename().string()) {
					_selIdx = (int)_bezelFiles.size() - 1;
				}
			}
		}
		return _selIdx;
	}

	void PostProcessor::SelectShader()
	{
		// Choose the shader
		switch (p_i_postprocessingLevel)
		{
			case 0: // transform passthrough shader
			case 1: // transform passthrough shader with optional scanlines
				shaderProgram = v_ppshaders.at(0);
				shaderProgram.Use();
				break;
			case 2:	// CRT shader
				shaderProgram = v_ppshaders.at(1);
				shaderProgram.Use();
				// size info
				shaderProgram.SetUniform("InputSize", glm::vec2(texWidth, texHeight));
				shaderProgram.SetUniform("OutputSize", glm::vec2(quadWidth, quadHeight));

				shaderProgram.SetUniform("PreviousFrame", _TEXUNIT_PP_PREVIOUS - GL_TEXTURE0);

				// shader specific
				shaderProgram.SetUniform("GhostingPercent", p_f_ghostingPercent);
				shaderProgram.SetUniform("BlurSize", p_f_phosphorBlur);
				shaderProgram.SetUniform("bBlurGlow", p_b_phosphorGlow);
				shaderProgram.SetUniform("bCORNER_SMOOTH", p_b_smoothCorner);
				shaderProgram.SetUniform("bUseOKlab", p_b_useOKlab);
				shaderProgram.SetUniform("bSLOT", p_b_slot);
				shaderProgram.SetUniform("BARRELDISTORTION", p_f_barrelDistortion);
				shaderProgram.SetUniform("BGR", p_f_bgr);
				shaderProgram.SetUniform("BLACK", p_f_black);
				shaderProgram.SetUniform("BR_DEP", p_f_brDep);
				shaderProgram.SetUniform("BRIGHTNESS", p_f_brightness);
				shaderProgram.SetUniform("CONTRAST", p_f_contrast);
				shaderProgram.SetUniform("C_STR", p_f_cStr);
				shaderProgram.SetUniform("CONV_B", p_f_convB);
				shaderProgram.SetUniform("CONV_G", p_f_convG);
				shaderProgram.SetUniform("CONV_R", p_f_convR);
				shaderProgram.SetUniform("CORNER", p_f_corner / 10000);
				shaderProgram.SetUniform("MASKH", p_f_maskHigh);
				shaderProgram.SetUniform("MASKL", p_f_maskLow);
				shaderProgram.SetUniform("MSIZE", p_f_maskSize);
				shaderProgram.SetUniform("HUE", p_f_hue);
				shaderProgram.SetUniform("GB", p_f_hueGB);
				shaderProgram.SetUniform("RB", p_f_hueRB);
				shaderProgram.SetUniform("RG", p_f_hueRG);
				shaderProgram.SetUniform("SATURATION", p_f_saturation);
				shaderProgram.SetUniform("SCANLINE_WEIGHT", p_f_scanlineWeight);
				shaderProgram.SetUniform("SCAN_SPEED", p_f_scanSpeed);
				shaderProgram.SetUniform("FILM_GRAIN", p_f_filmGrain);
				shaderProgram.SetUniform("SLOTW", p_f_slotW);
				shaderProgram.SetUniform("VIGNETTE_WEIGHT", p_f_vignetteWeight);
				shaderProgram.SetUniform("INTERLACE_WEIGHT", p_f_interlace);
				shaderProgram.SetUniform("iCOLOR_SPACE", p_i_cSpace);
				shaderProgram.SetUniform("iM_TYPE", p_i_maskType);
				shaderProgram.SetUniform("iSCANLINE_TYPE", p_i_scanlineType);
				shaderProgram.SetUniform("vWARP", p_v_warp);
				break;
		}
		// common — every shader (basic + CRT) samples the //e input on
		// _TEXUNIT_PP_INPUT, so set the uniform regardless of level.
		// Without this the basic-passthrough shader at level 0/1 sees
		// its sampler default to texture unit 0 and renders all black.
		shaderProgram.SetUniform("A2TextureCurrent", _TEXUNIT_PP_INPUT - GL_TEXTURE0);
		shaderProgram.SetUniform("POSTPROCESSING_LEVEL", p_i_postprocessingLevel);
	}

	void PostProcessor::RegenerateFBOs()
	{
		if (FBO == UINT_MAX)
		{
			glGenFramebuffers(1, &FBO);
			glGenTextures(1, &texture_id);
		}
		if (FBO_prevFrame == UINT_MAX)
		{
			glGenFramebuffers(1, &FBO_prevFrame);
			glGenTextures(1, &prevFrame_texture_id);
		}

		// Compute the pixel boundaries of the quad from its normalized coordinates.
		// The conversion from normalized device coordinates (range [-1,1]) to pixel coordinates is:
		//    pixel = (ndc * 0.5 + 0.5) * viewportDimension

		glm::vec4 quadCorners[4] = {
			glm::vec4(-1.0f, -1.0f, 0.0f, 1.0f),
			glm::vec4(1.0f, -1.0f, 0.0f, 1.0f),
			glm::vec4(1.0f,  1.0f, 0.0f, 1.0f),
			glm::vec4(-1.0f,  1.0f, 0.0f, 1.0f)
		};
		glm::vec4 transformed[4];
		for (int i = 0; i < 4; ++i)
		{
			transformed[i] = mTransform * quadCorners[i];
			if (transformed[i].w != 0.0f)
				transformed[i] /= transformed[i].w;
		}

		float nquadLeft = std::min({ transformed[0].x, transformed[1].x, transformed[2].x, transformed[3].x });
		float nquadRight = std::max({ transformed[0].x, transformed[1].x, transformed[2].x, transformed[3].x });
		float nquadBottom = std::max({ transformed[0].y, transformed[1].y, transformed[2].y, transformed[3].y });
		float nquadTop = std::min({ transformed[0].y, transformed[1].y, transformed[2].y, transformed[3].y });

		// rounding is critical, to properly align the previous frame texture
		tA2Quad.x = std::round((nquadLeft * 0.5 + 0.5) * viewportWidth);
		tA2Quad.y = std::round((nquadTop * 0.5 + 0.5) * viewportHeight);
		tA2Quad.w = std::round((nquadRight * 0.5 + 0.5) * viewportWidth - tA2Quad.x);
		tA2Quad.h = std::round((nquadBottom * 0.5 + 0.5) * viewportHeight - tA2Quad.y);

		glBindFramebuffer(GL_FRAMEBUFFER, FBO);
		glBindTexture(GL_TEXTURE_2D, texture_id);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, viewportWidth, viewportHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture_id, 0);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		glBindFramebuffer(GL_FRAMEBUFFER, FBO_prevFrame);
		glBindTexture(GL_TEXTURE_2D, prevFrame_texture_id);
		// Also here use GL_NEAREST to get rid of tiny rounding errors that will compound
		// dramatically at high ghosting values. Proper rounding and GL_NEAREST fix this.
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, tA2Quad.w, tA2Quad.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, prevFrame_texture_id, 0);
		glBindFramebuffer(GL_FRAMEBUFFER, 0); // Unbind FBO

		glEnable(GL_BLEND);
		glClearColor(0.f,0.f,0.f,1.f);

		// Straight-alpha (stb_image gives straight RGBA)
		// Without this blend function the bezel won't have transparency
		glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
							GL_ONE,       GL_ONE_MINUS_SRC_ALPHA);

		glFlush();

		GLuint glerr;
		if ((glerr = glGetError()) != GL_NO_ERROR) {
			std::cerr << "OpenGL error PP RegeneratePreviousTexture: " << glerr << std::endl;
		}
	}

	void PostProcessor::Render(int outWidth, int outHeight, uint32_t inputTexId, uint32_t inputTexWidth, uint32_t inputTexHeight)
	{
		GLenum glerr;

		if (bezelImageAsset.tex_id == UINT_MAX)
		{
			std::vector<std::string> _bezelFiles;
			currentBezelIndex = PopulateBezelFiles(_bezelFiles, selectedBezelFile);
			if (currentBezelIndex > 0)
				LoadSelectedBezel();
		}

		texWidth = inputTexWidth;
		texHeight = inputTexHeight;

		if ((texWidth == 0) || (texHeight == 0)) {
			return;
		}

		if ((outWidth <= 0) || (outHeight <= 0)) {
			return;
		}

		if ((requestedWidth != outWidth) || (requestedHeight != outHeight)
			|| (FBO == UINT_MAX) || (FBO_prevFrame == UINT_MAX))
		{
			requestedWidth = outWidth;
			requestedHeight = outHeight;
			bShouldRegenFBO = true;
		}

		viewportWidth = outWidth;
		viewportHeight = outHeight;

		GLint last_viewport[4]; glGetIntegerv(GL_VIEWPORT, last_viewport);
		// Don't let the viewport have odd values. It creates artifacts when scaling
		if (viewportWidth % 2 == 1)
			viewportWidth -= 1;
		if (viewportHeight % 2 == 1)
			viewportHeight -= 1;
		glViewport(0, 0, viewportWidth, viewportHeight);

		// How much can we scale the output quad?
		// Always scale up in integers numbers, but auto scale down is float
		float _scale = static_cast<float>(viewportWidth) / static_cast<float>(texWidth);
		_scale = std::min(_scale, static_cast<float>(viewportHeight) / static_cast<float>(texHeight));
		if (_scale > 1.0f)
			_scale = std::floor(_scale);
		else
			_scale = 1.0f;
		// make sure the scale doesn't extend beyond the GL max texture size
		max_integer_scale = std::min(maxTexSize / texHeight, maxTexSize / texWidth);

		if (!bAutoScale)
		{
			integer_scale = std::min(integer_scale, max_integer_scale);
			_scale = static_cast<float>(integer_scale);
		}
		else {
			integer_scale = std::min(static_cast<int>(_scale), max_integer_scale);
		}

		// Determine the quad's origin
		quadWidth = static_cast<int>(_scale * texWidth);
		quadHeight = static_cast<int>(_scale * texHeight);

		// Now determine the quad transformations as necessary, to send to the vertex shader
		glm::mat4 _transform = glm::mat4(1.0f);
		if (bCRTFillWindow)
		{
			// For full-window
			quadWidth = viewportWidth;
			quadHeight = viewportHeight;
		}
		// Compute scale factors based on desired quad dimensions.
		float scaleX = (quadWidth / static_cast<float>(viewportWidth));
		float scaleY = (quadHeight / static_cast<float>(viewportHeight));
		_transform = glm::scale(_transform, glm::vec3(scaleX*p_v_zoom.x, scaleY*p_v_zoom.y, 1.0f));
		_transform = glm::translate(_transform, glm::vec3(p_v_center.x/100.f, p_v_center.y/100.f, 0.0f));
		if (_transform != mTransform)
		{
			mTransform = _transform;
			bShouldRegenFBO = true;
		}

		// Always re-bind the texture we're given and generate the mimaps
		glActiveTexture(_TEXUNIT_PP_INPUT);
		inTextureId = inputTexId;
		glBindTexture(GL_TEXTURE_2D, inTextureId);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glGenerateMipmap(GL_TEXTURE_2D);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
		glActiveTexture(GL_TEXTURE0);

		// Always bind the previous frame texture to its dedicated texture unit
		glActiveTexture(_TEXUNIT_PP_PREVIOUS);
		glBindTexture(GL_TEXTURE_2D, prevFrame_texture_id);
		glActiveTexture(GL_TEXTURE0);

		if ((glerr = glGetError()) != GL_NO_ERROR) {
			std::cerr << "OpenGL error PP Input Tex Params: " << glerr << std::endl;
		}

		glBindFramebuffer(GL_FRAMEBUFFER, FBO);
		GLenum _fb = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if (_fb != GL_FRAMEBUFFER_COMPLETE) {
			fprintf(stderr, "FBO not complete at Render start: 0x%X\n", _fb);
		}

		glClear(GL_COLOR_BUFFER_BIT);

		if (bImguiWindowIsOpen || (!shaderProgram.isReady)
			|| (prev_texWidth != texWidth) || (prev_texHeight != texHeight))
		{
			// only update the shader parameters in certain cases
			// as it may be very costly for rPi and slow CPUs
			this->SelectShader();
			prev_texWidth = texWidth;
			prev_texHeight = texHeight;
			if ((glerr = glGetError()) != GL_NO_ERROR) {
				std::cerr << "OpenGL error PP shaderProgram select: " << glerr << std::endl;
			}
		}
		else
		{
			shaderProgram.Use();
			if ((glerr = glGetError()) != GL_NO_ERROR) {
				std::cerr << "OpenGL error PP shaderProgram use: " << glerr << std::endl;
			}
		}

		// Used for all PP shaders
		shaderProgram.SetUniform("uTransform", mTransform);		// in the vertex shader
		shaderProgram.SetUniform("iFrameCount", frame_count);
		shaderProgram.SetUniform("bHalveFrameRate", bHalveFramerate);
		shaderProgram.SetUniform("TextureSize", glm::vec2(texWidth, texHeight));
		// Only used for the full PP shader
		if (p_i_postprocessingLevel == 2) {
			shaderProgram.SetUniform("OutputSize", glm::vec2(quadWidth, quadHeight));
			shaderProgram.SetUniform("ScanlineCount", inputTexHeight);
		}

		// Bind the quad VAO and draw the quad (static VBO already set up)
		glBindVertexArray(quadVAO);
		glDrawArrays(GL_TRIANGLES, 0, 6);

		if ((glerr = glGetError()) != GL_NO_ERROR) {
			std::cerr << "OpenGL error PP glDrawArrays: " << glerr << std::endl;
		}

		/////////////////////////// BEGIN PREVIOUS FRAME TEXTURE ///////////////////////////

		// DO NOT COPY INTO THE PREVIOUS FRAME TEXTURE UNLESS IT IS REQUIRED
		// THIS _DRAMATICALLY_ REDUCES THE FPS ON A RASPBERRY PI
		if ((p_f_ghostingPercent > 0.0000001f && p_i_postprocessingLevel == 2) || bHalveFramerate)
		{

			// Now copy the screen texture to prevFrame_texture_id, to use it for the next frame
			glBindFramebuffer(GL_READ_FRAMEBUFFER, FBO);
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, FBO_prevFrame);
			glBlitFramebuffer(tA2Quad.x, tA2Quad.y, tA2Quad.w + tA2Quad.x, tA2Quad.h + tA2Quad.y,	// source rectangle (quad region)
							  0, 0, tA2Quad.w, tA2Quad.h,											// destination rectangle
							  GL_COLOR_BUFFER_BIT, GL_NEAREST);
			glBindFramebuffer(GL_FRAMEBUFFER, FBO);

			if ((glerr = glGetError()) != GL_NO_ERROR) {
				std::cerr << "OpenGL error PP glBlitFramebuffer: " << glerr << std::endl;
			}
		}

		//////////////////////////// END OF PREVIOUS FRAME TEXTURE ///////////////////////////

		// Now Build and Draw the Bezel if necessary
		if (selectedBezelFile != _PP_NO_BEZEL_FILENAME)
		{
			shaderProgramBezel.Use();
			glm::mat4 transformBezel = glm::mat4(1.0f);
			transformBezel = glm::scale(transformBezel, glm::vec3(bezelSize.x, bezelSize.y, 1.0f));
			transformBezel = glm::translate(transformBezel, glm::vec3(bezelCenter.x / 100.f, bezelCenter.y / 100.f, 0.0f));
			shaderProgramBezel.SetUniform("uTransform", transformBezel);		// in the vertex shader
			shaderProgramBezel.SetUniform("uMainTex", _TEXUNIT_PP_BEZEL - GL_TEXTURE0);
			shaderProgramBezel.SetUniform("uA2Tex", _TEXUNIT_PP_INPUT - GL_TEXTURE0);
			shaderProgramBezel.SetUniform("uReflectionAmount", p_f_bezelReflection);
			shaderProgramBezel.SetUniform("uReflectionBlur", p_f_reflectionBlur);
			shaderProgramBezel.SetUniform("uReflectionScale", p_v_reflectionScale);
			shaderProgramBezel.SetUniform("uReflectionTranslation", p_v_reflectionTranslation);
			shaderProgramBezel.SetUniform("uOutlineQuad", p_b_outlineQuad);
			if (bezelGlassImageAsset.image_xcount > 0) {
				shaderProgramBezel.SetUniform("uGlassTex", _TEXUNIT_PP_BEZEL_GLASS - GL_TEXTURE0);
				shaderProgramBezel.SetUniform("uGlassThickness", p_f_glassThickness);
			}
			else {
				shaderProgramBezel.SetUniform("uGlassTex", 0);
				shaderProgramBezel.SetUniform("uGlassThickness", 0.f);
			}


			glActiveTexture(_TEXUNIT_PP_INPUT);
			glBindTexture(GL_TEXTURE_2D, inTextureId);
			if (p_f_bezelReflection > 0.001)
			{
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
			} else {
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			}
			glActiveTexture(GL_TEXTURE0);

			glBindVertexArray(quadVAO);
			glDrawArrays(GL_TRIANGLES, 0, 6);

			if ((glerr = glGetError()) != GL_NO_ERROR) {
				std::cerr << "OpenGL error PP Bezel glDrawArrays: " << glerr << std::endl;
			}
		}

		glBindVertexArray(0);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(last_viewport[0], last_viewport[1], (GLsizei)last_viewport[2], (GLsizei)last_viewport[3]);
		glActiveTexture(GL_TEXTURE0);
		if ((glerr = glGetError()) != GL_NO_ERROR) {
			std::cerr << "OpenGL error PP End: " << glerr << std::endl;
		}
		++frame_count;

		// Regenerate the FBO after the frame is generated
		// to avoid flicker when the user adjusts the geometry in realtime
		if (bShouldRegenFBO)
		{
			bShouldRegenFBO = false;
			RegenerateFBOs();
		}
	}

	void PostProcessor::ResetToDefaults()
	{
		memset(preset_name_buffer, 0, sizeof(preset_name_buffer));

		max_integer_scale = 1;
		integer_scale = 1;		// Base integer scale used
		bAutoScale = true;		// Automatically scale to max scale?
		bHalveFramerate = false;	// Mixes every pair of frames, to avoid page flip flicker
		bCRTFillWindow = false;

		selectedBezelFile = _PP_NO_BEZEL_FILENAME;
		currentBezelIndex = 0;
		bezelSize = glm::vec2(1.0f, 1.0f);
		bezelCenter = glm::vec2(0.f, 0.f);

		p_b_smoothCorner = false;
		p_b_useOKlab = true;
		p_b_slot = false;
		p_f_barrelDistortion = 0.0f;
		p_f_bgr = 0.0f;
		p_f_black = 0.0f;
		p_f_brDep = 0.2f;
		p_f_brightness = 1.0f;
		p_f_contrast = 1.0f;
		p_f_convB = 0.0f;
		p_f_convG = 0.0f;
		p_f_convR = 0.0f;
		p_f_corner = 0.0f;
		p_f_cStr = 0.0f;
		p_f_hue = 0.0f;
		p_f_hueGB = 0.0f;
		p_f_hueRB = 0.0f;
		p_f_hueRG = 0.0f;
		p_f_maskHigh = 0.75f;
		p_f_maskLow = 0.3f;
		p_f_maskSize = 1.0f;
		p_f_saturation = 1.0f;
		p_f_scanlineWeight = 1.0f;
		p_f_scanSpeed = 0.0f;
		p_f_filmGrain = 0.0f;
		p_f_interlace = 0.f;
		p_f_slotW = 3.0f;
		p_f_vignetteWeight = 0.0f;
		p_i_cSpace = 0;
		p_i_maskType = 0;
		p_i_postprocessingLevel = 0;
		p_i_scanlineType = 0;
		p_f_ghostingPercent = 0;
		p_f_phosphorBlur = 0.0f;
		p_b_phosphorGlow = 0.0f;
		p_v_warp = glm::vec2(0.0f, 0.0f);
		p_v_center = glm::vec2(0.0f, 0.0f);
		p_v_zoom = glm::vec2(1.0f, 1.0f);

		// bezel shader variables
		p_b_outlineQuad = false;
		p_f_bezelReflection = 0.0f;
		p_f_reflectionBlur = 0.0f;
		p_v_reflectionScale = glm::vec2(1.0f, 1.0f);
		p_v_reflectionTranslation = glm::vec2(0.0f, 0.0f);
		p_f_glassThickness = 1.0f;

		// imgui vars
		bImGuiLockWarp = false;
		bImGuiLockZoom = false;
	}

	void PostProcessor::RenderImGuiWindow()
	{
		if (bImguiWindowIsOpen)
		{
			ImGui::SetNextWindowSizeConstraints(ImVec2(450, 400), ImVec2(FLT_MAX, FLT_MAX));
			ImGui::Begin("Post Processing CRT Shader", &bImguiWindowIsOpen);
			ImGui::Checkbox("Enable Post Processing", &bIsActive);
			if (!bIsActive) {
				ImGui::End();
				return;
			}

			if (ImGui::Button("Reset to defaults"))
			{
				ResetToDefaults();
			}

			// Handle presets
			ImGui::Text("[ PRESETS ]");
			IGFD::FileDialogConfig config;
			config.path = std::string(APP_BASE_PATH()).append("./presets/");
			static ImGuiFileDialog instance_presets;
			if (ImGui::Button("Load")) {
				if (instance_presets.IsOpened())
					instance_presets.Close();
				ImGui::SetNextWindowSize(ImVec2(800, 400));
				instance_presets.OpenDialog("LoadStateDlg", "Choose File to Load", ".json", config);
			}
			ImGui::SameLine(); ImGui::Spacing(); ImGui::SameLine();	// Start Name
			if (std::strlen(preset_name_buffer) == 0) {
				std::strncpy(preset_name_buffer, "Untitled", sizeof(preset_name_buffer) - 1);
			}
			ImGui::PushItemWidth(200);
			ImGui::InputText("Name", preset_name_buffer, sizeof(preset_name_buffer));
			ImGui::PopItemWidth();
			ImGui::SameLine(); ImGui::Spacing(); ImGui::SameLine();	// End Name
			if (ImGui::Button("Save")) {
				if (instance_presets.IsOpened())
					instance_presets.Close();
				ImGui::SetNextWindowSize(ImVec2(800, 400));
				instance_presets.OpenDialog("SaveStateDlg", "Choose Save Location", ".json", config);
			}

			if (instance_presets.Display("LoadStateDlg")) {
				if (instance_presets.IsOk()) {
					std::string filePath = instance_presets.GetFilePathName();
					LoadState(filePath);
				}
				instance_presets.Close();
			}

			if (instance_presets.Display("SaveStateDlg")) {
				if (instance_presets.IsOk()) {
					std::string filePath = instance_presets.GetFilePathName();
					SaveState(filePath);
				}
				instance_presets.Close();
			}

			ImGui::PushItemWidth(200);

			// PP Type
			ImGui::Separator();
			ImGui::Text("[ POSTPROCESSING LEVEL ]");
			ImGui::RadioButton("Transforms and Overlays##PPLEVEL", &p_i_postprocessingLevel, 0); ImGui::SameLine();
			HelpMarker("Geometry transformations and bezels.\nLow performance impact.");
			ImGui::RadioButton("Full CRT##PPLEVEL", &p_i_postprocessingLevel, 2);
			HelpMarker("All CRT shader capabilities.\nPotentially high performance impact.");
			ImGui::Separator();
			ImGui::Text("[ BASE INTEGER SCALE ]");
			ImGui::Checkbox("Auto", &bAutoScale);
			HelpMarker("Automatically selects the largest output possible, with pixel perfect scaling");
			if (bAutoScale)
				ImGui::BeginDisabled();
			ImGui::SliderInt("Integer Scale", &integer_scale, 1, max_integer_scale, "%d");
			if (bAutoScale)
				ImGui::EndDisabled();

			ImGui::Separator();

			ImGui::Text("[ GEOMETRY & OVERLAY]");
			// GEOMETRY
			if (ImGui::Checkbox("Fill Window", &bCRTFillWindow))
			{
				p_v_zoom.x = 1.0f;
				p_v_zoom.y = 1.0f;
				p_v_center.x = 0;
				p_v_center.y = 0;

			}
			if (bImGuiLockZoom)
			{
				float uniform = p_v_zoom.x;
				p_v_zoom.y = p_v_zoom.x;
				if (ImGui::DragFloat("Image Zoom", &uniform, 0.001f, 0.001f, 5.0f, "%.3f"))
					p_v_zoom = glm::vec2(uniform);
			}
			else
				ImGui::DragFloat2("Image Zoom", reinterpret_cast<float*>(&p_v_zoom), 0.001f, 0.001f, 5.0f, "%.3f");
			ImGui::SameLine(); ImGui::Spacing();
			ImGui::SameLine(); ImGui::Checkbox("Uniform##Zoom", &bImGuiLockZoom);

			ImGui::DragFloat2("Image Center", reinterpret_cast<float*>(&p_v_center), 0.1f, -100.0f, 100.0f, "%.2f");

			// OVERLAY
			currentBezelIndex = 0;
			std::vector<std::string> _bezelFiles;
			currentBezelIndex = PopulateBezelFiles(_bezelFiles, selectedBezelFile);

			std::vector<const char*> bezelFileCStrs;
			for (const auto& file : _bezelFiles) {
				bezelFileCStrs.push_back(file.c_str());
			}
			if (ImGui::Combo("Image", &currentBezelIndex, bezelFileCStrs.data(), static_cast<int>(bezelFileCStrs.size())))
			{
				selectedBezelFile = _bezelFiles[currentBezelIndex];
				if (currentBezelIndex > 0)
					LoadSelectedBezel();
			}
			ImGui::DragFloat2("Overlay Zoom", reinterpret_cast<float*>(&bezelSize), 0.001f, 0.f, 300.f, "%.2f");
			ImGui::DragFloat2("Overlay Center", reinterpret_cast<float*>(&bezelCenter), 1.f, -300.f, 300.f, "%.2f");
			if (bezelGlassImageAsset.image_xcount > 0) {
				ImGui::SliderFloat("Glass Thickness", &p_f_glassThickness, 0.f, 2.f, "%.2f");
			}
			p_b_outlineQuad = false;
			ImGui::SliderFloat("Bezel Reflection", &p_f_bezelReflection, 0.f, 0.5f, "%.3f");
			HelpMarker("WARNING: Tricky to get right. Read on for more info: \n\
In order to make a very fast fake reflection technique, we mirror the Apple 2\n\
texture with blur, and superpose it onto the overlay only where the bezel has\n\
some transparency (>0, <1). Since each overlay is different, and your choice\n\
of screen, resolution or window size is unique, you're going to have to manually\n\
tweak the size and position of the mirrored texture to conform to your idea of\n\
a reflection for your bezel. It takes work but can provide a really valuable\n\
FX that makes the whole screen 'pop'. Tweak the scale and center when at 0 blur\n\
and strong reflection, then dial blur up and reflection down.");
			ImGui::SliderFloat("Reflection Blur", &p_f_reflectionBlur, 0.0f, 10.f, "%.3f");
			if (ImGui::DragFloat2("Reflection Scale", reinterpret_cast<float*>(&p_v_reflectionScale),
								  0.001f, 0.001f, 5.0f, "%.3f"))
				p_b_outlineQuad = true;
			if (ImGui::DragFloat2("Reflection Center", reinterpret_cast<float*>(&p_v_reflectionTranslation),
								  0.001f, -4.f, 4.f, "%.3f"))
				p_b_outlineQuad = true;

			ImGui::Separator();

			ImGui::Text("[ FRAME MERGING ]");
			ImGui::Checkbox("Merge Frame Pairs", &bHalveFramerate);
			HelpMarker("WARNING: SIGNIFICANT FPS IMPACT!\n\
Merges every pair of even and odd frames.\n\
Effectively halves the frame rate\n\
but removes any flickering associated\n\
with page flipping images");
			if (p_i_postprocessingLevel == 2) {
				ImGui::Separator();
				// Scanline and Interlacing
				ImGui::Text("[ SCANLINE TYPE ]");
				ImGui::RadioButton("None##SCANLINETYPE", &p_i_scanlineType, 0);
				ImGui::SameLine();
				if (ImGui::RadioButton("Simple##SCANLINETYPE", &p_i_scanlineType, 1))
				{
					p_f_scanlineWeight = 0.3f;
				}
				ImGui::SameLine();
				if (ImGui::RadioButton("Complex##SCANLINETYPE", &p_i_scanlineType, 2))
				{
					p_f_scanlineWeight = 1.0f;
				}
				HelpMarker("WARNING: Uncheck \"50% Scanlines\" from the Video Settings (F8)\n\
if using these scanlines\n\
You should generally tweak color settings (further down)\n\
when using the complex scanline type");
				if (p_i_scanlineType >= 2)
				{
					ImGui::SliderFloat("Scanline Weight", &p_f_scanlineWeight, 0.0f, 2.0f, "%.2f");
					ImGui::SliderFloat("Scanline Speed", &p_f_scanSpeed, 0.0f, 2.0f, "%.2f");
					ImGui::SliderFloat("Film Grain", &p_f_filmGrain, 0.0f, 1.0f, "%.2f");
					ImGui::SliderFloat("Vignette Weight", &p_f_vignetteWeight, 0.0f, 5.0f, "%.2f");
					HelpMarker("Darker sides of the scanlines, works better when there's distortion");
					ImGui::SliderFloat("Interlacing", &p_f_interlace, 0.0f, 2.0f, "%.2f");
					HelpMarker("If you really want to feel the pain of bad refresh rates");
				}

				ImGui::Separator();

				// Blurring and Ghosting
				ImGui::Text("[ BLUR & GHOSTING ]");
				ImGui::SliderFloat("Phosphor Blur", &p_f_phosphorBlur, 0.0, 2.0, "%.2f");
				HelpMarker("Some screen blur");
				ImGui::SameLine();ImGui::Checkbox("Glow", &p_b_phosphorGlow);
				HelpMarker("Some screen blur");

				// We'll use a normalized slider value in [0,1]
				static float _ghostingSV = 100.0f * (1.0f - pow(1.0f - p_f_ghostingPercent / 100.0f, 0.5f));
				if (p_f_ghostingPercent < 0.001)
					_ghostingSV = 0.0f;
				if (ImGui::SliderFloat("Ghosting Amount", &_ghostingSV, 0.0f, 100.0f, "%.0f"))
				{
					// Map sliderValue to ghosting percentage
					// The mapping (1 - (1-x)^2) gives finer control near 100.
					p_f_ghostingPercent = 100.0f - 100.0f * powf(1.0f - _ghostingSV/100.f, 2.0f);
				}
				HelpMarker("WARNING: SIGNIFICANT FPS IMPACT! \n\
Mix in a bit of ghosting to smooth animations. \n\
Overdo it to emulate the Apple /// monitor!\n\
Works best at low frame rates, below 60 FPS.\n\
Needs more ghosting for fast frame rates.");

				ImGui::Separator();

				// Mask Settings
				ImGui::Text("[ MASK SETTINGS ]");
				ImGui::RadioButton("None##Mask", &p_i_maskType, 0); ImGui::SameLine();
				ImGui::RadioButton("CGWG##Mask", &p_i_maskType, 1); ImGui::SameLine();
				ImGui::RadioButton("RGB##Mask", &p_i_maskType, 2);
				ImGui::DragFloat("Mask Size", &p_f_maskSize, 0.001f, 0.001f, 20.0f, "%.3f");
				ImGui::Checkbox("Slot Mask On/Off", &p_b_slot);
				ImGui::SliderFloat("Slot Mask Width", &p_f_slotW, 2.0f, 3.0f, "%.1f");
				ImGui::SliderFloat("Subpixels BGR/RGB", &p_f_bgr, 0.0f, 1.0f, "%.1f");
				ImGui::SliderFloat("Mask Brightness Dark", &p_f_maskLow, 0.0f, 1.0f, "%.2f");
				ImGui::SliderFloat("Mask Brightness Bright", &p_f_maskHigh, 0.0f, 1.0f, "%.2f");
				ImGui::Separator();

				// Geometry Settings
				ImGui::Text("[ ADVANCED GEOMETRY ]");
				if (bImGuiLockWarp)
				{
					float uniform = p_v_warp.x;
					p_v_warp.y = p_v_warp.x;
					if (ImGui::DragFloat("Curvature", &uniform, 0.001f, -0.5f, 0.5f, "%.3f"))
						p_v_warp = glm::vec2(uniform);
				}
				else
					ImGui::DragFloat2("Curvature", reinterpret_cast<float*>(&p_v_warp), 0.001f, -0.5f, 0.5f, "%.3f");
				ImGui::SameLine(); ImGui::Spacing();
				ImGui::SameLine(); ImGui::Checkbox("Uniform##Curvature", &bImGuiLockWarp);

				ImGui::SliderFloat("Barrel Distortion", &p_f_barrelDistortion, -0.30f, 5.00f, "%.2f");
				ImGui::SliderFloat("Corners Cut", &p_f_corner, 0.f, 100.f, "%.3f");
				ImGui::SameLine(); ImGui::Spacing();
				ImGui::SameLine(); ImGui::Checkbox("Smooth", &p_b_smoothCorner);
				ImGui::Separator();

				// Color Settings
				ImGui::Text("[ COLOR SETTINGS ]");
				ImGui::Checkbox("Use OKlab instead of linear RGB", &p_b_useOKlab);
				HelpMarker("OKlab is a smoother color space than linear RGB.");
				ImGui::DragFloat("Brightness", &p_f_brightness, 0.01f, 0.0f, 100.0f, "%.2f");
				ImGui::DragFloat("Contrast", &p_f_contrast, 0.01f, 0.0f, 100.0f, "%.2f");
				ImGui::DragFloat("Black Level", &p_f_black, 0.01f, -1.0f, 1.00f, "%.2f");
				ImGui::DragFloat("Saturation", &p_f_saturation, 0.01f, 0.0f, 100.0f, "%.2f");
				if (p_b_useOKlab) {
					ImGui::DragFloat("Hue", &p_f_hue, 0.001f, -M_PI, M_PI, "%.2f");
				} else {
					ImGui::DragFloat("Green <-to-> Red Hue", &p_f_hueRG, 0.01f, -2.50f, 2.50f, "%.2f");
					ImGui::DragFloat("Blue <-to-> Red Hue", &p_f_hueRB, 0.01f, -2.50f, 2.50f, "%.2f");
					ImGui::DragFloat("Blue <-to-> Green Hue", &p_f_hueGB, 0.01f, -2.50f, 2.50f, "%.2f");
				}
				ImGui::SliderFloat("Scan/Mask Brightness Dependence", &p_f_brDep, 0.0f, 0.5f, "%.3f");
				ImGui::SliderInt("Color Space: sRGB,PAL,NTSC-U,NTSC-J", &p_i_cSpace, 0, 3, "%1d");
				ImGui::Separator();

				// Convergence Settings
				ImGui::Text("[ CONVERGENCE SETTINGS ]");
				ImGui::SliderFloat("Convergence Overall Strength", &p_f_cStr, 0.0f, 0.5f, "%.2f");
				HelpMarker("WARNING: Some FPS impact.");
				ImGui::SliderFloat("Convergence Red X-Axis", &p_f_convR, -3.0f, 3.0f, "%.2f");
				ImGui::SliderFloat("Convergence Green X-axis", &p_f_convG, -3.0f, 3.0f, "%.2f");
				ImGui::SliderFloat("Convergence Blue X-Axis", &p_f_convB, -3.0f, 3.0f, "%.2f");
			}

			ImGui::PopItemWidth();
			ImGui::End();
		}
	}

	void PostProcessor::SetActive(bool isActive) {
		bIsActive = isActive;
	}

} // namespace sa2
