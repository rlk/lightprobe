
varying vec3 N;
varying vec3 T;

void main()
{
    mat3 M = mat3(gl_TextureMatrix[0][0].xyz,
                  gl_TextureMatrix[0][1].xyz,
                  gl_TextureMatrix[0][2].xyz);
    N = M * gl_Vertex.xyz;
    T = M * gl_Normal.xyz;
    gl_Position = ftransform();
}

