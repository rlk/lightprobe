#extension GL_ARB_texture_rectangle : enable

varying vec3 N;

uniform sampler2DRect image;
uniform sampler1D     color;
uniform vec2          circle_p;
uniform float         circle_r;

/*----------------------------------------------------------------------------*/

vec2 probe(vec3 n)
{
    float kr = length(n.xy);
    float ks = sin(0.5 * acos(n.z));

    vec2 v = vec2(n.x, -n.y);
    vec2 p = circle_p + circle_r * v * ks / kr;

    return p;
}

float value(vec2 p)
{
    return 1.0 / length(vec2(length(dFdx(p)), length(dFdy(p))));
}

/*----------------------------------------------------------------------------*/

void main()
{
    vec3  n = normalize(N);
    vec2  p = probe(n);
    float d = value(p);

    vec4  c = texture2DRect(image, p);
    float a = c.a * d * d;

    gl_FragColor = vec4(c.rgb * a, a);
}

/*----------------------------------------------------------------------------*/
