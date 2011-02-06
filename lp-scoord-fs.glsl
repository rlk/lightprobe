#extension GL_ARB_texture_rectangle : enable

varying vec3 N;

uniform vec2  circle_p;
uniform float circle_r;

/*----------------------------------------------------------------------------*/

vec2 unwrap(vec3 n)
{
    float kr = length(n.xy);
    float ks = sin(0.5 * acos(n.z));

    vec2 v = vec2(n.x, -n.y);
    vec2 p = circle_p + circle_r * v * ks / kr;

    return p;
}

/*----------------------------------------------------------------------------*/

void main()
{
    gl_FragColor = vec4(unwrap(normalize(N)), 0.0, 0.0);
}

/*----------------------------------------------------------------------------*/
