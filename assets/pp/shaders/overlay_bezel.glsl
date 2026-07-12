/*
By Rikkles.

This shader takes an overlay image and displays it fully within the limits of the quad,
with a special fake "reflection" in defined bezel areas of the overlay.

Its specificity is that whenever the alpha of the pixel is > 0 but < 1, then
the shader samples another input texture and mixes it in with the overlay pixel, using
a mix value passed in as a uniform parameter.
The other texture is sampled via a transformation of the original pixel coordinates.
This transformation is handled by uniforms.

Hence in order to create the reflection for the overlay image, you modify the alpha.
The lower the alpha value, the less translation the shader will make, and the closer
to the bezel the light will be "reflected" (using a cheap LOD 1 lookup to simulate blur)

There is also an optional Glass texture. This one is a straightforward RGBA texture
that is applied on top of the final result. It is to be used for the CRT glass.
*/

#ifdef GL_ES
	#define COMPAT_PRECISION mediump
	precision mediump float;
#else
	#define COMPAT_PRECISION
#endif

///////////////////////////////////////// VERTEX SHADER /////////////////////////////////////////
#if defined(VERTEX)

in vec2 aPos;		// [-1, 1] range (NDC)
in vec2 aTexCoord;	// Main texture coordinates, [0,1] range

out vec2 vTexCoord; // Pass main texcoord to fragment shader

uniform mat4 uTransform;

void main()
{
	gl_Position = uTransform * vec4(aPos, 0.0, 1.0);
	vTexCoord = aTexCoord;
}

///////////////////////////////////////// FRAGMENT SHADER /////////////////////////////////////////
#elif defined(FRAGMENT)

uniform sampler2D uMainTex;  // Overlay texture
uniform sampler2D uA2Tex;    // Apple 2 output texture with GL_MIRRORED_REPEAT wrap

uniform sampler2D uGlassTex;    // Glass overlay texture (the top layer)
uniform float uGlassThickness;  // 0 means no glass applied

in vec2 vTexCoord;
out vec4 FragColor;

uniform float uReflectionAmount;
uniform float uReflectionBlur;
uniform vec2 uReflectionScale;
uniform vec2 uReflectionTranslation;
uniform bool uOutlineQuad;

void main() {
	
    // Sample the main texture at the incoming texture coordinate
    vec4 mainColor = texture(uMainTex, vTexCoord);

    // We only do the "special compositing" if alpha is strictly between 0 and 1
    if (uReflectionAmount > 0.00001)
    {
        if (mainColor.a > 0.0 && mainColor.a < 1.0)
        {
            vec2 a2TexCoord = (vTexCoord - vec2(0.5)) * uReflectionScale + vec2(0.5) + uReflectionTranslation;

            // When the user changes the quad parameters, we outline the quad boundaries in red
            if (uOutlineQuad) {
                if (a2TexCoord.x > 0.00 && a2TexCoord.x < 0.01) {
                    FragColor = vec4(1.0, 0.0, 0.0, 1.0); // red
                    return;
                }
                if (a2TexCoord.y > 0.00 && a2TexCoord.y < 0.01) {
                    FragColor = vec4(1.0, 0.0, 0.0, 1.0); // red
                    return;
                }
                if (a2TexCoord.x > 0.99 && a2TexCoord.x < 1.0) {
                    FragColor = vec4(1.0, 0.0, 0.0, 1.0); // red
                    return;
                }
                if (a2TexCoord.y > 0.99 && a2TexCoord.y < 1.0) {
                    FragColor = vec4(1.0, 0.0, 0.0, 1.0); // red
                    return;
                }
            }

			bool outOfBounds =
				any(lessThan(a2TexCoord, vec2(0.0))) ||
				any(greaterThan(a2TexCoord, vec2(1.0)));
			vec4 a2Color = outOfBounds
							? textureLod(uA2Tex, a2TexCoord, uReflectionBlur)
							: vec4(0.,0.,0.,1.);

            // Combine the mainColor over the a2Color, forcing alpha=1.0
            // Standard "over" alpha compositing: final = foreground * a + background * (1-a)
            float finalReflection = (1.0 - uReflectionAmount)*a2Color.a;
            vec3 finalRgb = mainColor.rgb * finalReflection + a2Color.rgb * (1.0 - finalReflection);
            FragColor  = vec4(finalRgb, 1.0);
        } else {
            FragColor = mainColor;
        }
    } else {
        FragColor = mainColor;
    }
    // glass overlay
    if (uGlassThickness > 0.0001) {
        vec4 glassColor = texture(uGlassTex, vTexCoord);
        glassColor.a *= uGlassThickness;
        float outA = glassColor.a + FragColor.a * (1.0 - glassColor.a);
        FragColor.rgb = (glassColor.rgb * glassColor.a + 
                        FragColor.rgb * FragColor.a * (1.0 - glassColor.a))
                        / outA;
        FragColor.a = outA;
        //FragColor = clamp(FragColor + glassColor,0.0,1.0);
    }
    return;
}

#endif
