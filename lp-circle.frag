
uniform sampler2DRect image;

void main()
{
    vec4 c = texture2DRect(image, gl_TexCoord[0].xy);
    gl_FragColor = c;
}

