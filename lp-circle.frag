
uniform sampler2DRect image;
uniform float         exposure;

void main()
{
    vec4 c = texture2DRect(image, gl_TexCoord[0].xy);
    vec3 C = 1.0 - exp(-exposure * c.rgb);

    gl_FragColor = vec4(C, c.a);
}

