#extension GL_ARB_texture_rectangle : enable

uniform sampler2DRect image;
uniform vec2          circle_p;
uniform float         circle_r;
uniform float         exposure;
uniform float         zoom;

float impulse(float r, float dr, float x)
{
    return smoothstep(r - dr, r, x) * (1.0 - smoothstep(r, r + dr, x));
}

void main()
{
    vec2  p = gl_TexCoord[0].xy;
    float d = length(fwidth(p));
    vec4  c = texture2DRect(image, p);
    vec3  C = 1.0 - exp(-exposure * c.rgb);

    float r = length(p - circle_p);
    float k = impulse(circle_r * 1.00, d * 2.0, r)
            + impulse(circle_r * 0.50, d * 0.8, r)
            + impulse(circle_r * 0.01, d * 0.8, r);
    vec3  m = mix(C, 1.0 - C, k);

    gl_FragColor = vec4(m, c.a);
}

