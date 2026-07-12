#ifdef GL_ES
	#define COMPAT_PRECISION mediump
	precision mediump float;
#else
	#define COMPAT_PRECISION
#endif

// Shader for text overlays

in vec2 vTexCoords;
in vec4 vColor;
uniform sampler2D uTex;	// font atlas
out vec4 FragColor;
void main() {
	if (vTexCoords.x < -0.001) {		// the log overlay
		FragColor = vColor;
	} else {
		float a = texture(uTex, vTexCoords).r;
		FragColor = vec4(vColor.rgb, vColor.a * a);
	}
}
