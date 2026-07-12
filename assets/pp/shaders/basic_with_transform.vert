#ifdef GL_ES
#define COMPAT_PRECISION mediump
precision mediump float;
#else
#define COMPAT_PRECISION
#endif

// Basic vertex shader that accepts a transform matrix

layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 texCoords;
out vec2 vTexCoords;

uniform mat4 uTransform;

void main()
{
	vTexCoords = texCoords;
    gl_Position = uTransform * vec4(aPos, 0.0, 1.0);
}
