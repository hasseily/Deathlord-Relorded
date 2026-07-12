#ifndef SHADER_H
#define SHADER_H

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <variant>
#include <SDL3/SDL.h>
#include <unordered_map>
#include <vector>
#include "glm/glm.hpp"

/*
	@brief:
	Utility class to create, build and run a shader.
		- After creating the shader, call Build()
		- You can set static uniforms that don't change every frame by calling shader.Use()
		  and then SetUniform(), and optionally shader.Release()
		- Frame-dynamic uniforms are called the same way, but inside the render loop

	All uniform locations are cached for better performance. They're generally cached upon
	first use, but you can force pre-caching by calling CacheUniform() for each uniform
	after building and activating the shader.

	If you want even more control over the uniforms, you can always do something like:
	Cache the uniform yourself:
		u_ticks = glGetUniformLocation(shader.ID, "ticks");
	Assign the uniform in the render loop:
		glUniform1i(u_ticks, SDL_GetTicks());

 */

// This static lib uses GLAD. The below is to allow the main exec to use what
// it wants for OpenGL

// Generic proc type (same calling conv as SDL_GL_GetProcAddress)
using PP_GL_GetProcAddr = void* (*)(const char*);

// Call this once after the app creates the SDL GL context,
// ON THE SAME THREAD that will use GL.
bool PP_InitGL(PP_GL_GetProcAddr getProc);

// SDL3 — SDL_GetBasePath() can't be called before SDL_Init, so the old
// global-init definition would fault. Use a Meyer's-singleton getter
// instead and have the call sites do `APP_BASE_PATH().append(...)`.
namespace {
	inline const std::string& APP_BASE_PATH() {
		static const std::string s = [] {
			const char* p = SDL_GetBasePath();
			return p ? std::string(p) : std::string("./");
		}();
		return s;
	}
}

namespace sa2 {
	using UniformValue = std::variant<
		bool,int,uint32_t,float,
		glm::vec2,glm::ivec2,glm::uvec2,
		glm::vec3,glm::vec4,
		glm::mat2,glm::mat3,glm::mat4
	>;
	
	class Shader
	{
	public:
		unsigned int ID = 0;
		bool isReady = false;    // Shader is useless until you call build()
		bool isInUse = false;
		
		// Build from vertex and fragment shaders (could be combined using VERTEX and FRAGMENT #define)
		void Build(const char* vertexPath, const char* fragmentPath);
		void _Compile(const std::string* pvertexCode, const std::string* pfragmentCode);
		
		// de/activate the shader
		void Use();
		void Release();
		
		const std::string GetVertexPath() { return s_vertexPath; }
		const std::string GetFragmentPath() { return s_fragmentPath; }
		
		void CacheUniform(std::string const& name);
		void SetUniform(std::string const& name, UniformValue const& v);

		std::string get_gl_version();

	private:
		std::string s_vertexPath;
		std::string s_fragmentPath;
		std::string s_glsl_version;
		std::unordered_map<std::string,int> uniformCache;
		std::vector<std::string> _uniformNames;
		std::vector<int>       _uniformLocs;

		// utility function for checking shader compilation/linking errors.
		void CheckCompileErrors(uint32_t shader, std::string type);
		void set_gl_version();
	};
} // namespace sa2
#endif
