#extension GL_ARB_texture_rectangle : enable

varying vec3 N;
varying vec3 T;

uniform sampler2DRect image;
uniform vec2          circle_p;
uniform float         circle_r;
uniform float         exposure;
uniform float         saturate;
uniform float         modulate;

/*----------------------------------------------------------------------------*/

vec2 probe(vec3 n)
{
    float kr = length(n.xy);
    float ks = sin(0.5 * acos(n.z));

    vec2 v = vec2(n.x, -n.y);
    vec2 p = circle_p + circle_r * v * ks / kr;

    return p;
}

float value(vec3 n)
{
    return smoothstep(-0.75, -0.25, n.z);
}

/*----------------------------------------------------------------------------*/

void main()
{
    vec3 n = normalize(N);
//  vec3 t = normalize(T);

    vec2  p = probe(n);
    float d = value(n);

    vec4  c = texture2DRect(image, p);
    float a = max(saturate, c.a * d);

    gl_FragColor = mix(vec4(c.rgb * a, a),
                       vec4(0.0, 0.0, 0.0, 1.0), modulate);
}

/*----------------------------------------------------------------------------*/
