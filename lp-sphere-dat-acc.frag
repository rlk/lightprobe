#extension GL_ARB_texture_rectangle : enable

varying vec3 N;
varying vec3 T;

uniform sampler2DRect image;
uniform vec2          circle_p;
uniform float         circle_r;
uniform float         sphere_e;
uniform float         sphere_a;
uniform float         sphere_r;
uniform float         exposure;

/*----------------------------------------------------------------------------*/

void main()
{
    vec3 n = normalize(N);
    vec3 t = normalize(T);

    float kr = length(n.xy);
    float ks = sin(0.5 * acos(n.z));

    vec2 p = circle_p - circle_r * n.xy * ks / kr;

    vec4 c = texture2DRect(image, p);

    gl_FragColor = c;
}

/*----------------------------------------------------------------------------*/
