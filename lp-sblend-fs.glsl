#extension GL_ARB_texture_rectangle : enable

uniform sampler2DRect image;
uniform sampler2DRect coord;

/*----------------------------------------------------------------------------*/

// a b c
// d e f
// g h i

void main()
{
    vec2 p = gl_FragCoord.xy;

    vec2 a = texture2DRect(coord, p + vec2(-1.0,  1.0)).xy;
    vec2 b = texture2DRect(coord, p + vec2( 0.0,  1.0)).xy;
    vec2 c = texture2DRect(coord, p + vec2( 1.0,  1.0)).xy;
    vec2 d = texture2DRect(coord, p + vec2(-1.0,  0.0)).xy;
    vec2 e = texture2DRect(coord, p + vec2( 0.0,  0.0)).xy;
    vec2 f = texture2DRect(coord, p + vec2( 1.0,  0.0)).xy;
    vec2 g = texture2DRect(coord, p + vec2(-1.0, -1.0)).xy;
    vec2 h = texture2DRect(coord, p + vec2( 0.0, -1.0)).xy;
    vec2 i = texture2DRect(coord, p + vec2( 1.0, -1.0)).xy;

    vec2 x = -a - 2.0 * b - c + g + 2.0 * h + i;
    vec2 y = -a - 2.0 * d - g + c + 2.0 * f + i;

    float D = length(vec2(length(x), length(y)));
    vec4  C = texture2DRect(image, e);

    gl_FragColor = vec4(C.rgb * C.a / D, C.a / D);
}

/*----------------------------------------------------------------------------*/
