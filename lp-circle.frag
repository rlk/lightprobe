
uniform sampler2DRect image;
uniform vec2          size;
uniform float         exposure;
uniform float         zoom;

void main()
{
    vec2 u = gl_TexCoord[0].xy;
    vec4 c = texture2DRect(image, u);
    vec3 C = 1.0 - exp(-exposure * c.rgb);

    gl_FragColor = vec4(C, c.a);
}

