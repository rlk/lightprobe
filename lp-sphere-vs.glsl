
varying vec3 V;

void main()
{
    V           = gl_Vertex.xyz;
    gl_Position = ftransform();
}
