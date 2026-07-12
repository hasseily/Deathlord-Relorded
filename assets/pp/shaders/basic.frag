#ifdef GL_ES
	#define COMPAT_PRECISION mediump
	precision mediump float;
#else
	#define COMPAT_PRECISION
#endif

in vec2 vTexCoords;
out vec4 FragColor;

uniform COMPAT_PRECISION int iFrameCount;
uniform sampler2D A2TextureCurrent;
uniform sampler2D PreviousFrame;
uniform bool bHalveFrameRate;
uniform COMPAT_PRECISION int POSTPROCESSING_LEVEL;
uniform COMPAT_PRECISION float GhostingPercent;
uniform COMPAT_PRECISION vec2 TextureSize;

// Utility functions to convert from/to sRGB/linear space
vec3 toLinear(vec3 srgbColor) {
	return pow(srgbColor, vec3(2.2));
}
vec3 toGamma(vec3 linearColor) {
	return pow(linearColor, vec3(1.0 / 2.2));
}

// This function merges 2 generated frames: if it's an even frame nothing
// happens, but an odd frame mixes in the previous even frame at 50%
// In main.cpp if we halve the frame rate, every even frame is skipped
// The background buffer isn't flipped and we overwrite it. The end result
// is that the frame rate is halved, and we only display even+odd during
// odd frames
vec4 HalveFrameRate(vec2 coords, vec4 currentColor)
{
	if ((iFrameCount & 1) == 1) {
		vec4 previousColor = texture(PreviousFrame, coords);

		// Convert both colors to linear space
		vec3 linearCurrent = toLinear(currentColor.rgb);
		vec3 linearPrevious = toLinear(previousColor.rgb);

		// Perform the mix in linear space
		vec3 linearMix = (linearCurrent + linearPrevious) * 0.5;

		// Convert the mixed color back to sRGB space
		vec3 gammaMix = toGamma(linearMix);
		return vec4(gammaMix, 1.0);
	}
	return currentColor;
}

void main()
{
	FragColor = texture(A2TextureCurrent, vTexCoords);
	if (bHalveFrameRate)
		FragColor = HalveFrameRate(vTexCoords, FragColor);

	// Apply simple horizontal scanline if required
	if (POSTPROCESSING_LEVEL == 1) {
		ivec2 texSize = textureSize(A2TextureCurrent, 0);
		FragColor.rgb = FragColor.rgb * (1.0 - mod(floor(vTexCoords.y * float(TextureSize.y)), 2.0));
		return;
	}
}
