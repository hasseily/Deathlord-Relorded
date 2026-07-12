/*
By Rikkles.

This shader is a frankenstein and uses parts from:
crt-Cyclon by DariusG
crt-Geom (alternate scanlines)
Mattias CRT
Quillez (main filter)
Dogway's inverse Gamma

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2 of the License, or (at your option)
any later version.

*/

#define pi 3.1415926535897932384626433

#ifdef GL_ES
	#define COMPAT_PRECISION mediump
	precision mediump float;
#else
	#define COMPAT_PRECISION
#endif

///////////////////////////////////////// VERTEX SHADER /////////////////////////////////////////
#if defined(VERTEX)

in vec2 aPos;
in vec2 TexCoord;
//in vec4 COLOR;
out vec2 TexCoords; // Pass texture coordinates to fragment shader
out vec2 scale;
out vec2 ps;

uniform COMPAT_PRECISION int iFrameCount;
uniform COMPAT_PRECISION vec2 OutputSize;
uniform COMPAT_PRECISION vec2 TextureSize;
uniform COMPAT_PRECISION vec2 InputSize;

uniform mat4 uTransform;

void main()
{
	gl_Position = uTransform * vec4(aPos, 0.0, 1.0);
	TexCoords = TexCoord;
	scale = OutputSize.xy/TextureSize.xy;
	ps = 1.0/TextureSize.xy;
}

///////////////////////////////////////// FRAGMENT SHADER /////////////////////////////////////////
#elif defined(FRAGMENT)

uniform COMPAT_PRECISION int iFrameCount;
uniform COMPAT_PRECISION vec2 OutputSize;
uniform COMPAT_PRECISION vec2 TextureSize;
uniform COMPAT_PRECISION vec2 InputSize;
uniform COMPAT_PRECISION uint ScanlineCount;
uniform sampler2D A2TextureCurrent;
uniform sampler2D PreviousFrame;
uniform bool bHalveFrameRate;
in vec2 TexCoords;
in vec2 scale;
in vec2 ps;
out vec4 FragColor;

uniform COMPAT_PRECISION int POSTPROCESSING_LEVEL;

uniform COMPAT_PRECISION float GhostingPercent;
uniform COMPAT_PRECISION float BlurSize;
uniform bool bBlurGlow;

uniform bool bUseOKlab;
uniform bool bCORNER_SMOOTH;
uniform bool bPOTATO;
uniform bool bSLOT;
uniform bool bVIGNETTE;
uniform COMPAT_PRECISION float BARRELDISTORTION;
uniform COMPAT_PRECISION float BGR;
uniform COMPAT_PRECISION float BLACK;
uniform COMPAT_PRECISION float BR_DEP;
uniform COMPAT_PRECISION float BRIGHTNESS;
uniform COMPAT_PRECISION float CONTRAST;
uniform COMPAT_PRECISION float C_STR;
uniform COMPAT_PRECISION float CONV_B;
uniform COMPAT_PRECISION float CONV_G;
uniform COMPAT_PRECISION float CONV_R;
uniform COMPAT_PRECISION float CORNER;
uniform COMPAT_PRECISION float MASKH;
uniform COMPAT_PRECISION float MASKL;
uniform COMPAT_PRECISION float MSIZE;
uniform COMPAT_PRECISION float RB;
uniform COMPAT_PRECISION float RG;
uniform COMPAT_PRECISION float GB;
uniform COMPAT_PRECISION float HUE;
uniform COMPAT_PRECISION float SATURATION;
uniform COMPAT_PRECISION float SCANLINE_WEIGHT;
uniform COMPAT_PRECISION float SCAN_SPEED;
uniform COMPAT_PRECISION float FILM_GRAIN;
uniform COMPAT_PRECISION float INTERLACE_WEIGHT;
uniform COMPAT_PRECISION float SLOTW;
uniform COMPAT_PRECISION float VIGNETTE_WEIGHT;
uniform COMPAT_PRECISION int iCOLOR_SPACE;
uniform COMPAT_PRECISION int iM_TYPE;
uniform COMPAT_PRECISION int iSCANLINE_TYPE;
uniform COMPAT_PRECISION vec2 vWARP;
uniform COMPAT_PRECISION vec2 vCENTER;
uniform COMPAT_PRECISION vec2 vZOOM;


#define fTime (float(iFrameCount) / 60.0)
#define iResolution OutputSize.xy
#define fragCoord gl_FragCoord.xy

#define GAMMA 2.2

// Color mapping
// Basically:
// - switch from sRGB to linear RGB whenever you sample a texture
// - potentially move to OKLab for color changes
// - switch back from linear RGB to sRGB for the final FragColor

vec3 s2l(vec3 c) { // sRGB to linear
	return mix(c/12.92, pow((max(c,0.0)+0.055)/1.055,vec3(2.4)), step(vec3(0.04045),c));
}

vec3 l2s(vec3 c) { // linear to sRGB
	return mix(c*12.92, 1.055*pow(max(c,0.0),vec3(1./2.4))-0.055, step(vec3(0.0031308),c));
}

// sign-preserving cbrt for GLSL
float cbrt1(float x) { return sign(x) * pow(abs(x), 1.0/3.0); }
vec3  cbrt3(vec3  v) { return vec3(cbrt1(v.x), cbrt1(v.y), cbrt1(v.z)); }

vec3 l2oklab(vec3 rgb) { // linear to OkLab
	if (!bUseOKlab)
		return rgb;
	const mat3 rgb2lms = mat3(
							  +0.4122214708, +0.2119034982, +0.0883024619,
							  +0.5363325363, +0.6806995451, +0.2817188376,
							  +0.0514459929, +0.1073969566, +0.6299787005);
	const mat3 lms2lab = mat3(
							  +0.2104542553, +1.9779984951, +0.0259040371,
							  +0.7936177850, -2.4285922050, +0.7827717662,
							  -0.0040720468, +0.4505937099, -0.8086757660);
	vec3 lms = rgb2lms * rgb;
	return lms2lab * pow(lms, vec3(1.0/3.0));
}

vec3 oklab2l(vec3 lab) { // OkLab to linear
	if (!bUseOKlab)
		return lab;
	const mat3 lab2lms = mat3(
							  +1.0000000000, +1.0000000000, +1.0000000000,
							  +0.3963377774, -0.1055613458, -0.0894841775,
							  +0.2158037573, -0.0638541728, -1.2914855480);
	const mat3 lms2rgb = mat3(
							  +4.0767416621, -1.2684380046, 0.0041960863,
							  -3.3077115913, +2.6097574011, -0.7034186147,
							  +0.2309699292, -0.3413193965, +1.7076147010);
	vec3 lms = lab2lms * lab;
	return lms2rgb * (lms*lms*lms);
}

// This function merges 2 generated frames: if it's an even frame nothing
// happens, but an odd frame mixes in the previous even frame at 50%
// In main.cpp if we halve the frame rate, every even frame is skipped
// The background buffer isn't flipped and we overwrite it. The end result
// is that the frame rate is halved, and we only display even+odd during
// odd frames
// NOTE: currentColor must be in linear or oklab space already
vec4 HalveFrameRate(vec2 coords, vec4 currentColor)
{
	if ((iFrameCount & 1) == 1) {
		vec3 previousColor = s2l(texture(PreviousFrame, coords).rgb);

		// Perform the mix in linear/oklab space
		vec3 linearMix = (currentColor.rgb + previousColor) * 0.5;

		return vec4(linearMix, 1.0);
	}
	return currentColor;
}

// Creates a ghosting effect by mixing in the previous frame
// NOTE: currentColor must be in linear space
vec4 GenerateGhosting(vec2 coords, vec4 currentColor)
{
	float ghosting = GhostingPercent/100.0;
	vec4 blended = vec4(0.0,0.0,0.0,0.0);
	vec4 previousColor = texture(PreviousFrame, coords);
	previousColor.rgb = s2l(previousColor.rgb);
	// Calculate the intensity levels of both frames
	if (bUseOKlab) {
		previousColor.rgb = l2oklab(previousColor.rgb);
		currentColor.rgb = l2oklab(currentColor.rgb);
		if (currentColor.x > previousColor.x)	// move at a fast fixed speed towards higher intensity
			blended = mix(currentColor, previousColor, 0.01);
		else {	// going to lower intensity
				// As we get closer to the color, at some point we need to accelerate the move.
				// Otherwise at higher ghosting values the color will never be reached
				// (especially visible when fading to black).
				// Here as the colors get closer to each other, we use more of their distance
				// to blend instead of the ghosting value. The closer they are, the faster they merge.
			float cdist = length (previousColor - currentColor);
			float t = smoothstep(0.03, 0.05, cdist);
			ghosting = mix(cdist, ghosting, t);
			blended = mix(currentColor, previousColor, ghosting);
		}
		blended.rgb = oklab2l(blended.rgb);
	} else {	// linear rgb code for the faster (but less precise) way
		const vec3 lumWeights = vec3(0.2126, 0.7152, 0.0722);	// for linear rgb only!
		float currentIntensity = dot(currentColor.rgb, lumWeights);
		float previousIntensity = dot(previousColor.rgb, lumWeights);
		if (currentIntensity > previousIntensity)	// move at a fast fixed speed towards higher intensity
			blended = mix(currentColor, previousColor, 0.01);
		else {
			if ((previousIntensity - currentIntensity) < (ghosting*0.035))
				// As we get closer to the color (the higher the ghosting, the higher the cutoff),
				// at some point we need to accelerate the move. Otherwise at higher ghosting values
				// the color will never be reached (especially visible when fading to black)
				blended = mix(currentColor, previousColor, ghosting / 2.0);
			else
				blended = mix(currentColor, previousColor, ghosting);
		}
	}
	return blended;
}

// Use the LODs for the phosphor blur, which is much faster than sampling neighbor points
// NOTE: expects and returns color in linear rgb space
vec4 PhosphorBlur(sampler2D tex, vec2 uv, vec2 resolution, float blurAmount) {
	vec4 color = textureLod(tex, uv, blurAmount * 4.0);
	color.rgb = s2l(color.rgb);

	// To get a glowing style, we overlay the regular texture data at 30%
	if (bBlurGlow)
		color.rgb = mix(color.rgb, s2l(texture(tex, uv).rgb), 0.3);
	return clamp(color, 0.0, 1.0);
}

vec3 Mask(vec2 pos, float CGWG) {
	if (iM_TYPE == 0)
		return vec3(1.0);
	vec3 mask = vec3(CGWG);
	if (iM_TYPE == 1){
		if (bPOTATO) {
			return vec3( (1.0-CGWG)*sin(pos.x*pi)+CGWG);
		} else {
			float m = fract(pos.x*0.5);
			if (m < 0.5)
				mask.rb = vec2(1.0);
			else
				mask.g = 1.0;
			return mask;
		}
	}

	if (iM_TYPE == 2) {
		if (bPOTATO) {
			return vec3( (1.0-CGWG)*sin(pos.x*pi*0.6667)+CGWG) ;
		} else {
			float m = fract(pos.x*0.3333);
			if (m<0.3333)
				mask.rgb = (BGR == 0.0) ? vec3(mask.r, mask.g, 1.0) : vec3(1.0, mask.g, mask.b);
			else if (m<0.6666)
				mask.g = 1.0;
			else
				mask.rgb = (BGR == 0.0) ? vec3(1.0, mask.g, mask.b) : vec3(mask.r, mask.g, 1.0);
			return mask;
		}
	}
	return vec3(1.0);
}

float roundCorners(vec2 p, vec2 b, float r)
{
	return length(max(abs(p)-b+r,0.0))-r;
}

float scanlineWeights(float distance, vec3 color, float x) {
	// "wid" controls the width of the scanline beam, for each RGB
	// channel The "weights" lines basically specify the formula
	// that gives you the profile of the beam, i.e. the intensity as
	// a function of distance from the vertical center of the
	// scanline. In this case, it is gaussian if width=2, and
	// becomes nongaussian for larger widths. Ideally this should
	// be normalized so that the integral across the beam is
	// independent of its width. That is, for a narrower beam
	// "weights" should have a higher peak at the center of the
	// scanline than for a wider beam.
	float wid = SCANLINE_WEIGHT + 0.15 * dot(color, vec3(0.25-VIGNETTE_WEIGHT*x));
	float weights = distance / wid;
	return 0.1 * exp(-weights * weights * weights) / wid;
}

// Returns gamma corrected output, compensated for scanline+mask embedded gamma
vec3 inv_gamma(vec3 col, vec3 power) {
	vec3 cir = col-1.0;
	cir *= cir;
	col = mix(sqrt(col),sqrt(1.0-cir),power);
	return col;
}

// standard 6500k
mat3 PAL = mat3 (
				 1.0740 , -0.0574 , -0.0119 ,
				 0.0384 , 0.9699 , -0.0059 ,
				 -0.0079 , 0.0204 , 0.9884
				 );

// standard 6500k
mat3 NTSC = mat3 (
				  0.9318 , 0.0412 , 0.0217 ,
				  0.0135 , 0.9711 , 0.0148 ,
				  0.0055 , -0.0143 , 1.0085
				  );

// standard 8500k
mat3 NTSC_J = mat3 (
					0.9501 , -0.0431 , 0.0857 ,
					0.0265 , 0.9278 , 0.0432 ,
					0.0011 , -0.0206 , 1.3153
					);

vec3 slot(vec2 pos) {
	float h = fract(pos.x/SLOTW);
	float v = fract(pos.y);

	float odd;
	if (v<0.5)
		odd = 0.0;
	else
		odd = 1.0;

	if (odd == 0.0) {
		if (h<0.5)
			return vec3(0.5);
		else
			return vec3(1.5);
	} else if (odd == 1.0) {
		if (h<0.5)
			return vec3(1.5);
		else
			return vec3(0.5);
	}
}

vec2 Warp(vec2 pos) {
	pos = pos*2.0-1.0;
	pos *= vec2(1.0+pos.y*pos.y*vWARP.x, 1.0+pos.x*pos.x*vWARP.y);
	pos = pos*0.5+0.5;
	return pos;
}

vec2 BarrelDistortion(vec2 uv) {
	vec2 delta = uv - 0.5;
	float delta2 = dot(delta.xy, delta.xy);
	float delta4 = delta2 * delta2;
	float delta_offset = delta4 * BARRELDISTORTION;

	vec2 warped = uv + delta * delta_offset;
	return (warped - 0.5) / mix(1.0,1.2,BARRELDISTORTION/5.0) + 0.5;
}

//Canonical noise function; replaced to prevent precision errors
//float rand(vec2 co){
//    return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
//}

float rand(vec2 co)
{
	float a = 12.9898;
	float b = 78.233;
	float c = 43758.5453;
	float dt= dot(co.xy ,vec2(a,b));
	float sn= mod(dt,3.14);
	return fract(sin(sn) * c);
}

void main() {

	if (POSTPROCESSING_LEVEL == 0) {
		FragColor = texture(A2TextureCurrent, TexCoords);
		if (bHalveFrameRate || ((GhostingPercent > 0.0001))) {
			if (bHalveFrameRate)
				FragColor = HalveFrameRate(TexCoords, FragColor);
			if (GhostingPercent > 0.0001)
				FragColor = GenerateGhosting(TexCoords, FragColor);
		}
		return;
	}

	vec2 q = (TexCoords.xy * TextureSize.xy / InputSize.xy);
	vec2 uv = q;
	float o =2.0*mod(fragCoord.y,2.0)/iResolution.x;

	if (uv.x < 0.0 || uv.x > 1.0)
		discard;
	if (uv.y < 0.0 || uv.y > 1.0)
		discard;


	// Apply simple horizontal scanline if required and exit
	if (POSTPROCESSING_LEVEL == 1) {
		FragColor = texture(A2TextureCurrent, TexCoords);
		FragColor.rgb = FragColor.rgb * (1.0 - mod(floor(TexCoords.y * TextureSize.y), 2.0));
		if (bHalveFrameRate)
			FragColor = HalveFrameRate(TexCoords, FragColor);
		if (GhostingPercent > 0.0001)
			FragColor = GenerateGhosting(TexCoords, FragColor);
		return;
	}

	if (iSCANLINE_TYPE == 1) {
		if (mod(floor(TexCoords.y * TextureSize.y), 2.0) > 0.9)
			discard;
	}

	// Hue matrix inside main() to avoid GLES error
	mat3 hue = mat3 (
					 1.0, RG, RB,
					 -RG, 1.0, GB,
					 -RB, -GB, 1.0
					 );

	// Curvature on both axes
	vec2 pos = Warp(TexCoords);

	// If people prefer the BarrelDistortion algo
	pos = BarrelDistortion(pos);

	vec2 bpos = pos;
	vec2 dx = vec2(ps.x,0.0);

	// Quillez
	vec2 ogl2 = pos*TextureSize.xy;
	vec2 i = floor(pos*TextureSize.xy) + 0.5;
	float f = ogl2.y - i.y;
	pos.y = (i.y + 4.0*f*f*f)*ps.y; // smooth
	pos.x = mix(pos.x, i.x*ps.x, 0.2);

	// Rounded corners
	float corn = 1.0;
	if (CORNER > 0.000001) {
		vec2 halfRes = 0.5 * OutputSize.xy;
		float b = 1.0 - roundCorners(pos.xy * OutputSize.xy - halfRes, halfRes, abs(CORNER * OutputSize.x * 30.0));
		if (bCORNER_SMOOTH)
			corn = b/10.0;	// if we want it smooth
		else
			if (b < CORNER)
				discard;
	}

	vec4 res0;
	vec3 res;

	if (BlurSize > 0.001)
		res0 = PhosphorBlur(A2TextureCurrent, pos, TextureSize, BlurSize);
	else {
		res0 = texture(A2TextureCurrent,pos);
		res0.rgb = s2l(res0.rgb);
	}

	res = res0.rgb;
	// Convergence
	if (C_STR > 0.0001) {
		if ((abs(CONV_R)+abs(CONV_G)+abs(CONV_B)) > 0.001) {
			float resr = texture(A2TextureCurrent,pos + dx*CONV_R).r;
			float resg = texture(A2TextureCurrent,pos + dx*CONV_G).g;
			float resb = texture(A2TextureCurrent,pos + dx*CONV_B).b;

			res = vec3(res0.r*(1.0-C_STR) + resr*C_STR,
					   res0.g*(1.0-C_STR) + resg*C_STR,
					   res0.b*(1.0-C_STR) + resb*C_STR
					   );
		}
	}

	float l = dot(vec3(BR_DEP),res);

	// Mask CGWG definition, used for scanlines
	float CGWG = 0.3;
	if (bSLOT)
		CGWG = mix(MASKL, MASKH, l);

	if (iSCANLINE_TYPE == 2) {
		// New style scanlines, derived from Mattias' shader
		// vignette
		if (VIGNETTE_WEIGHT > 0.00001) {
			float vig = (0.0 + 1.0*16.0*pos.x*pos.y*(1.0-pos.x)*(1.0-pos.y));
			vig = pow(vig,VIGNETTE_WEIGHT);
			res *= vec3(vig);
		}

		if (SCANLINE_WEIGHT > 0.00001) {
			float scans = clamp( 0.35+0.15*sin(2.0*(-fTime * SCAN_SPEED)+pos.y*float(ScanlineCount)*8.0/2.55), 0.0, 1.0);
			float s = pow(scans,SCANLINE_WEIGHT);
			s = pow(s,SCANLINE_WEIGHT);
			res = res*vec3(s);
		}

		// flicker
		res *= 1.0+INTERLACE_WEIGHT*sin(300.0*fTime);

		// film grain
		res *= vec3(1.0) - FILM_GRAIN * vec3(
											 rand(pos + 0.0001 * fTime),
											 rand(pos + 0.0001 * fTime + 0.3),
											 rand(pos + 0.0001 * fTime + 0.5)
											 );
	}

	// Masks
	vec2 xy = TexCoords*OutputSize.xy/MSIZE;
	res *= Mask(xy, CGWG);

	// Apply slot mask on top of Trinitron-like mask
	if (bSLOT)
		res *= mix(slot(xy/2.0),vec3(1.0),CGWG);

	// Do bLack level always in linear RGB space, it's more precise
	res = (res - vec3(BLACK)) / (1.0-BLACK);

	// =================================== Enter OKLab Color Space ===================================
	// Now convert to OKLab for color mods
	res.rgb = l2oklab(res.rgb);

	if (bUseOKlab) {

		// 1) Hue rotation in the a–b plane (preserves chroma magnitude)
		float c = cos(HUE), s = sin(HUE);
		vec2 ab = vec2(res.y, res.z);
		ab = mat2(c, -s, s, c) * ab;
		res.y = ab.x;
		res.z = ab.y;

		// 2) Saturation: scale chroma radius around neutral (L*, 0, 0)
		res.yz *= SATURATION;

		// 3) Contrast
		const float pivotL = 0.5;	//Default 0.5
		res.x = (res.x - pivotL) * CONTRAST + pivotL;

		// 4) Brightness: perceptual lightness gain (on L*)
		res.x *= BRIGHTNESS;
	} else {

		// 1) Hue
		res *= hue;

		// 2) Saturation
		float slum = dot(vec3(0.29,0.60,0.11),res);
		res = mix(vec3(slum),res,SATURATION);

		// 3) Contrast
		const float pivot = 0.18;	// 0.18 is a common scene-referred “middle gray” in linear
		const vec3 luma = vec3(0.2126, 0.7152, 0.0722);
		float lum  = dot(luma, res);
		float lum2 = (lum - pivot) * CONTRAST + pivot;
		// Scale color to reach target luminance while preserving hue
		float scale = (lum > 1e-6) ? (lum2 / lum) : 0.0;
		res *= scale;
		res = clamp(res, 0.0, 1.0);	// soft clamp to reduce clipping

		// 4) Brightness
		res *= BRIGHTNESS;
	}

	FragColor = vec4(res, corn);


	// revert back to linear rgb
	FragColor.rgb = oklab2l(FragColor.rgb);
	// =================================== Exit OKLab Color Space ===================================

	// Color Spaces
	if (iCOLOR_SPACE != 0) {
		vec3 clr = FragColor.rgb;
		if (iCOLOR_SPACE == 1)
			clr *= PAL;
		if (iCOLOR_SPACE == 2)
			clr *= NTSC;
		if (iCOLOR_SPACE == 3)
			clr *= NTSC_J;
		// Apply CRT-like luminances
		clr /= vec3(0.24,0.69,0.07);
		clr *= vec3(0.29,0.6,0.11);
		FragColor.rgb = clr;
	}

	if (bHalveFrameRate)
		FragColor = HalveFrameRate(TexCoords.xy, FragColor);

	if (GhostingPercent > 0.0001) {
		FragColor = GenerateGhosting(TexCoords.xy, FragColor);
	}
	FragColor.rgb = l2s(FragColor.rgb);

}

#endif
