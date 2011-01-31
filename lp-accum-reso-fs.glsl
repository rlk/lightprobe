#extension GL_ARB_texture_rectangle : enable

varying vec3 N;

uniform sampler2DRect image;
uniform vec2          circle_p;
uniform float         circle_r;

/*----------------------------------------------------------------------------*/

vec2 probe(vec3 n)
{
    float kr = length(n.xy);
    float ks = sin(0.5 * acos(n.z));

    return circle_p + circle_r * vec2(n.x, -n.y) * ks / kr;
}

float value(vec3 n)
{
    return smoothstep(-0.75, -0.25, n.z);
}

/*----------------------------------------------------------------------------*/

void main()
{
    vec2 p = probe(normalize(N));

    float r = 1.0 / length(vec2(length(dFdx(p)), length(dFdy(p))));

    gl_FragColor = vec4(vec3(r), 1.0);
}

/*----------------------------------------------------------------------------*/
/*
float solidangle(vec3 a, vec3 b, vec3 c)
{
    float y = dot(a, cross(b, c));

    float aa = length(a);
    float bb = length(b);
    float cc = length(c);

    float x = aa * bb * cc + (dot(a, b) * cc +
                              dot(a, c) * bb +
                              dot(b, c) * aa);

    return 2.0 * atan(y / x);
}

float trianglearea(vec2 a, vec2 b, vec2 c)
{
    return 0.5 * ((a.x - c.x) * (b.y - a.y) - (a.x - b.x) * (c.y - a.y));
}

float acme(float p)
{
    return log2(2.0943951 * p) / 2.0;
}
*/
/*----------------------------------------------------------------------------*/
/*
void main()
{
    const float d = 0.001;

    vec3 n = normalize(N);
    vec3 t = normalize(T);
    vec3 b = normalize(cross(n, t));

    vec3 u = normalize(n + t * d);
    vec3 v = normalize(n + b * d);

    vec2 p = probe(n);
    vec2 q = probe(u);
    vec2 r = probe(v);

    float dp = 1.0 / pow(max(length(q - p), length(r - p)), 2.0);
//  float dp = trianglearea(p, q, r);   // square pixels
    float dn = solidangle(n, u, v);     // steradians

    float pps = dp / dn;          // square pixels per steradian

//  gl_FragColor = vec4(vec3(dp), 1.0);
    gl_FragColor = vec4(vec3(acme(pps)), 1.0);
}
*/
/*----------------------------------------------------------------------------*/
