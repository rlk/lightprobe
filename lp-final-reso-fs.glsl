#extension GL_ARB_texture_rectangle : enable

uniform sampler2DRect image;

/*----------------------------------------------------------------------------*/

float range(float a, float b, float t)
{
    return step(a, t) * step(t, b);
}

vec3 acmecolor(float k)
{
    vec3 c = (range( 9.0, 10.0, k) * vec3(0.0, 0.0, 1.0) +
              range(10.0, 11.0, k) * vec3(1.0, 0.0, 0.0) +
              range(11.0, 12.0, k) * vec3(1.0, 0.0, 1.0) +
              range(12.0, 13.0, k) * vec3(0.0, 1.0, 0.0) +
              range(13.0, 14.0, k) * vec3(0.0, 1.0, 1.0) +
              range(14.0, 15.0, k) * vec3(1.0, 1.0, 0.0) +
              range(15.0, 16.0, k) * vec3(1.0, 1.0, 1.0));

    float l = (k - 8.0) / 8.0;

    return c * l;
}

/*----------------------------------------------------------------------------*/

void main()
{
    vec4 C = texture2DRect(image, gl_FragCoord.xy);

//  gl_FragColor = vec4(C.rgb, 1.0);
    gl_FragColor = vec4(acmecolor(C.r), 1.0);
}

/*----------------------------------------------------------------------------*/
