#extension GL_ARB_texture_rectangle : enable

uniform sampler2DRect image;
uniform sampler1D     color;
uniform float         reso_k;
uniform float         expo_k;
uniform float         expo_n;

/*----------------------------------------------------------------------------*/

void main()
{
    vec4 p = texture2DRect(image, gl_FragCoord.xy);
    vec3 c = p.rgb / p.a;

    vec3 t = 1.0 - exp(-expo_n * c);
    vec3 r = texture1D(color, p.a).rgb;

    c = mix(c, t, expo_k);
    c = mix(c, r, reso_k);

    gl_FragColor = vec4(c, 1.0);
}

/*----------------------------------------------------------------------------*/
