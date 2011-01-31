
varying vec3 N;
varying vec3 T;

void main()
{
    mat3 M = mat3(gl_TextureMatrix[0][0].xyz,
                  gl_TextureMatrix[0][1].xyz,
                  gl_TextureMatrix[0][2].xyz);
    N = M * gl_Vertex.xyz;
    T = M * gl_Normal.xyz;

    gl_TexCoord[0] = gl_TextureMatrix[0] * gl_Vertex;

    gl_Position = vec4(-2.0 * gl_MultiTexCoord0.x + 1.0,
                        2.0 * gl_MultiTexCoord0.y - 1.0, 1.0, 1.0);
}

