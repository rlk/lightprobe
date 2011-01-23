
void main()
{
    gl_TexCoord[0] = gl_Vertex;
    gl_Position = ftransform();
}

