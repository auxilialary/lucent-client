uniform sampler2D gTextureSampler;
// Pixel-space radial scene composition; zero feather disables the mask.
uniform vec4 gSceneMask;

noperspective in vec2 texCoord;
noperspective in vec4 vertColor;

out vec4 FragClr;
void main()
{
#ifdef TW_TEXTURED
	vec4 tex = texture(gTextureSampler, texCoord);
	FragClr = tex * vertColor;
#else
	FragClr = vertColor;
#endif
	if(gSceneMask.w > 0.0)
	{
		float coverage = smoothstep(0.0, gSceneMask.w, gSceneMask.z - distance(gl_FragCoord.xy, gSceneMask.xy));
		FragClr.a *= 1.0 - coverage;
	}
}
