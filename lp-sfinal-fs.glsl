#extension GL_ARB_texture_rectangle : enable

uniform sampler2DRect image;
uniform sampler1D     color;
uniform float         expo_k;
uniform float         expo_n;

/*----------------------------------------------------------------------------*/

void main()
{
    vec4 p = texture2DRect(image, gl_FragCoord.xy);
    vec3 c = p.rgb / p.a;
    vec3 t = 1.0 - exp(-expo_n * c);

    gl_FragColor = vec4(mix(c, t, expo_k), 1.0);
}

/*----------------------------------------------------------------------------*/
