#extension GL_ARB_texture_rectangle : enable

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
    vec3 v = normalize(gl_TexCoord[0].xyz);

    gl_FragColor = vec4((v + 1.0) * 0.5, 1.0);
}

/*----------------------------------------------------------------------------*/
