
varying vec3 N;
varying vec3 T;

void main()
{
    N = gl_Vertex.xyz;
    T = gl_Normal.xyz;
    gl_Position = ftransform();
}

