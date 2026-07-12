#ifdef GL_ES
#define COMPAT_PRECISION mediump
precision mediump float;
#else
#define COMPAT_PRECISION
#endif

// Basic vertex shader with vertex color and a transform matrix

layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 texCoords;
layout (location = 2) in vec4 aColor;
out vec2 vTexCoords;
out vec4 vColor;

uniform mat4 uTransform;

void main()
{
	vTexCoords = texCoords;
	vColor = aColor;
    gl_Position = uTransform * vec4(aPos, 0.0, 1.0);
}
