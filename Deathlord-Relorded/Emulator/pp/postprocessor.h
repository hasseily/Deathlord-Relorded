#pragma once
#ifndef POSTPROCESSOR_H
#define POSTPROCESSOR_H

/*
	Singleton postprocessing class that has a number of shaders to choose from.
	It simply takes as input a texture and passes it through the selected shader.
	It also has an ImGUI interface to modify the shader variables.
*/

#include "shader.h"
#include <vector>
#include "nlohmann/json.hpp"

namespace sa2 {
	class PostProcessor
	{
	public:
		struct ImageAsset {
			void AssignByFilename(const char* filename);

			// image assets are full 32-bit bitmap files, uploaded from PNG
			uint32_t image_xcount = 0;	// width and height of asset in pixels
			uint32_t image_ycount = 0;
			uint32_t tex_id = UINT_MAX;	// Texture ID on the GPU that holds the image data
		};

		// public singleton code
		static PostProcessor* GetInstance()
		{
			if (NULL == s_instance)
				s_instance = new PostProcessor();
			return s_instance;
		}
		~PostProcessor();

		uint32_t GetTextureId() { return texture_id; };
		void Render(int outWidth, int outHeight, uint32_t inputTexId, uint32_t inputTexWidth, uint32_t inputTexHeight);
		void RenderImGuiWindow();

		void SaveState(std::string filePath);
		void LoadState(std::string filePath);

		void LoadTexture(unsigned char* data, int width, int height, int nrComponents, uint32_t textureID);

		// Tells the caller to skip flipping the buffers if we are halving the frame rate
		// This actually skips even frames if ShouldFrameBeSkipped() is called after the frame
		// is created
		const bool ShouldFrameBeSkipped() { return (bHalveFramerate && (frame_count & 1) == 1); };
		const bool IsFrameRateHalved() { return bHalveFramerate; };

		const bool IsActive() { return (bIsActive); };
		void SetActive(bool isActive);

		// public properties
		std::vector<Shader>v_ppshaders;
		bool bImguiWindowIsOpen = false;

	private:
		void Initialize();
		int PopulateBezelFiles(std::vector<std::string>& bezelFiles, const std::string& selectedBezelFile);
		void SelectShader();
		void RegenerateFBOs();
		void ResetToDefaults();

		nlohmann::json SerializeState();
		void DeserializeState(const nlohmann::json& jsonState);

		void LoadSelectedBezel();
		
		// Singleton pattern
		static PostProcessor* s_instance;
		PostProcessor()
		{
			Initialize();
		}
		
		// Attributes
		uint32_t quadVAO = UINT_MAX;
		uint32_t quadVBO = UINT_MAX;
		uint32_t FBO = UINT_MAX;		// Framebuffer that holds the texture of the current
		uint32_t texture_id = UINT_MAX;	// The current frame as a texture

		uint32_t FBO_prevFrame = UINT_MAX;		// Framebuffer that holds the texture of the previous frame
		uint32_t prevFrame_texture_id = UINT_MAX;	// The previous frame as a texture
		
		int maxTexSize = 0;	// maximum texture size, depends on GL implementation

		bool bIsActive = false;

		Shader shaderProgram;		// PP shader program
		Shader shaderProgramBezel;	// Bezel shader program
		
		// The bezel image may have 2 layers: The bezel itself, and the glass which
		// comes on top. The bezel alpha (0<a<1) determines the reflection amount
		// The glass alpha is just standard alpha.
		ImageAsset bezelImageAsset;
		ImageAsset bezelGlassImageAsset;
		
		int requestedWidth = 0, requestedHeight = 0;	// requested size
		int viewportWidth = 0, viewportHeight = 0;		// actual size
		int quadWidth = 0, quadHeight = 0;
		int texWidth = 0, texHeight = 0;
		int prev_texWidth = INT_MAX, prev_texHeight = INT_MAX;
		uint32_t inTextureId = UINT_MAX;	// cached input texture id to check if it's changed

		// If sizes change, regenerate FBO
		bool bShouldRegenFBO = false;
		
		// The transform matrix for the A2 texture quad
		glm::mat4 mTransform = glm::mat4(1.0f);
		
		// Transformed A2 render quad
		// the rectangle (in pixels) where the A2 texture is rendered on screen
		// based on the transformations requested by the user. This is only necessary
		// to copy the data into the previous frame texture, as the transformation is
		// otherwise happening in the vertex shader
		SDL_Rect tA2Quad;
		
		bool bCRTFillWindow = false;
		
		int frame_count = 0;	// Frame count for interlacing, it may not be aligned with A2Video frames
		char preset_name_buffer[28];	// Preset's name
		int max_integer_scale = 1;	// Maximum possible integer scale given screen size
		int integer_scale = 1;		// Base integer scale used
		bool bAutoScale = true;		// Automatically scale to max scale?
		bool bHalveFramerate = false;	// Mixes every pair of frames, to avoid page flip flicker
#define _PP_NO_BEZEL_FILENAME "NONE"
		std::string selectedBezelFile = _PP_NO_BEZEL_FILENAME;
		int currentBezelIndex = 0;
		glm::vec2 bezelSize = glm::vec2(1.0f, 1.0f);
		glm::vec2 bezelCenter = glm::vec2(0.0f, 0.0f);
		
		// Shader parameter variables
		bool p_b_useOKlab = true;
		bool p_b_smoothCorner = false;
		bool p_b_slot = false;
		bool p_b_phosphorGlow = false;
		float p_f_barrelDistortion = 0.0f;
		float p_f_bgr = 0.0f;
		float p_f_black = 0.0f;
		float p_f_brDep = 0.2f;
		float p_f_brightness = 1.0f;
		float p_f_contrast = 1.0f;
		float p_f_convB = 0.0f;
		float p_f_convG = 0.0f;
		float p_f_convR = 0.0f;
		float p_f_corner = 0.0f;
		float p_f_cStr = 0.0f;
		float p_f_hue = 0.0f;
		float p_f_hueGB = 0.0f;
		float p_f_hueRB = 0.0f;
		float p_f_hueRG = 0.0f;
		float p_f_maskHigh = 0.75f;
		float p_f_maskLow = 0.3f;
		float p_f_maskSize = 1.0f;
		float p_f_saturation = 1.0f;
		float p_f_scanlineWeight = 1.0f;
		float p_f_scanSpeed = 0.0f;
		float p_f_filmGrain = 0.0f;
		float p_f_interlace = 0.f;
		float p_f_slotW = 3.0f;
		float p_f_vignetteWeight = 0.0f;
		int p_i_cSpace = 0;
		int p_i_maskType = 0;
		int p_i_postprocessingLevel = 0;
		int p_i_scanlineType = 0;
		float p_f_ghostingPercent = 0;	// Percentage of ghosting of previous frame. 0 means no ghosting
		float p_f_phosphorBlur = 0.0f;	// blur modifier
		glm::vec2 p_v_warp = glm::vec2(0.0f, 0.0f);	// curvature
		glm::vec2 p_v_center = glm::vec2(0.0f, 0.0f);
		glm::vec2 p_v_zoom = glm::vec2(1.0f, 1.0f);
		
		// bezel shader variables
		bool p_b_outlineQuad = false;
		float p_f_bezelReflection = 0.0f;
		float p_f_reflectionBlur = 0.0f;
		float p_f_glassThickness = 1.0f;
		glm::vec2 p_v_reflectionScale = glm::vec2(1.0f, 1.0f);
		glm::vec2 p_v_reflectionTranslation = glm::vec2(0.0f, 0.0f);
		
		// imgui vars
		bool bImGuiLockWarp = false;
		bool bImGuiLockZoom = false;
	};
} // namespace sa2

#endif	// POSTPROCESSOR_H
