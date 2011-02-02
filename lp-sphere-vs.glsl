
varying vec3 N;

void main()
{
    N = (gl_TextureMatrix[0] * gl_Vertex).xyz;
    gl_Position = ftransform();
}
