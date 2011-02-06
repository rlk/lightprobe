#extension GL_ARB_texture_rectangle : enable

varying vec3 V;
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
    mat3 M = mat3(gl_TextureMatrix[0][0].xyz,
                  gl_TextureMatrix[0][1].xyz,
                  gl_TextureMatrix[0][2].xyz);

    vec2 s = V.xy * vec2(-3.1415927, 1.5707963);

    vec3 n = vec3((sin(s.x) * cos(s.y)),
                  (           sin(s.y)),
                  (cos(s.x) * cos(s.y)));

    gl_FragColor = vec4(unwrap(M * n), 0.0, 0.0);
//  gl_FragColor = vec4(unwrap(M * n) / vec2(2600.0, 1800.0), 0.0, 0.0);
}

/*----------------------------------------------------------------------------*/
