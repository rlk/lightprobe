
varying vec3 N;

void main()
{
    mat3 M = mat3(gl_TextureMatrix[0][0].xyz,
                  gl_TextureMatrix[0][1].xyz,
                  gl_TextureMatrix[0][2].xyz);
    N = M * gl_Normal;
    gl_Position = ftransform();
}
