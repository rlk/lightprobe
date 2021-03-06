#extension GL_ARB_texture_rectangle : enable

uniform sampler2DRect image;
uniform float         exposure;

/*----------------------------------------------------------------------------*/

void main()
{
    vec4 C = texture2DRect(image, gl_FragCoord.xy);
    vec3 c = 1.0 - exp(-exposure * C.rgb / C.a);

    gl_FragColor = vec4(c, 1.0);
}

/*----------------------------------------------------------------------------*/
