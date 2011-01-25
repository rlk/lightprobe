
varying vec3 N;
varying vec3 T;

void main()
{
    mat3 M = mat3(gl_TextureMatrix[0][0].xyz,
                  gl_TextureMatrix[0][1].xyz,
                  gl_TextureMatrix[0][2].xyz);
    N = M * gl_Vertex.xyz;
    T = M * gl_Normal.xyz;

    float r = 2.0 - 2.0 * gl_MultiTexCoord0.y;
    vec2  p = vec2(sin(6.2831853064 * gl_MultiTexCoord0.x) * r,
                   cos(6.2831853064 * gl_MultiTexCoord0.x) * r);

    gl_Position = vec4(p * step(0.5, gl_MultiTexCoord0.y), 0.0, 1.0);
}

