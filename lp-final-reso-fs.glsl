#extension GL_ARB_texture_rectangle : enable

uniform sampler2DRect image;
uniform sampler1D     color;

/*----------------------------------------------------------------------------*/

float range(float a, float b, float t)
{
    return step(a, t) * step(t, b);
}

float impulse(float r, float dr, float x)
{
    return smoothstep(r - dr, r, x) * (1.0 - smoothstep(r, r + dr, x));
}

vec3 acmecolor(float k)
{
    float d = fwidth(k) * 3.0;

    float a = smoothstep(0.0, d, fract(k));
    float z = smoothstep(1.0, 1.0 - d, fract(k));
    return texture1D(color, k / 16.0).rgb * a * z;
}

/*----------------------------------------------------------------------------*/

void main()
{
    vec4 C = texture2DRect(image, gl_FragCoord.xy);

//  gl_FragColor = vec4(C.rgb, 1.0);
    gl_FragColor = vec4(acmecolor(C.r), 1.0);
}

/*----------------------------------------------------------------------------*/
