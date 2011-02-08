#extension GL_ARB_texture_rectangle : enable

varying vec3 V;

uniform vec2  circle_p;
uniform float circle_r;

/*----------------------------------------------------------------------------*/

vec2 unwrap(vec3 n)
{
    return circle_p + circle_r * n.xy * sin(0.5 * acos(n.z)) / length(n.xy);
}

/*----------------------------------------------------------------------------*/

void main()
{
    mat3 M = mat3(gl_TextureMatrix[0][0].xyz,
                  gl_TextureMatrix[0][1].xyz,
                  gl_TextureMatrix[0][2].xyz);

    vec2 s = vec2(atan(V.x, V.y), 1.5707963 * (1.0 - length(V.xy)));

    vec3 n = vec3((sin(s.x) * cos(s.y)),
                  (          -sin(s.y)),
                  (cos(s.x) * cos(s.y)));

    gl_FragColor = vec4(unwrap(M * n), 0.0, 0.0);
}

/*----------------------------------------------------------------------------*/
