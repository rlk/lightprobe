#ifdef __APPLE__
#include <OpenGL/OpenGL.h>
#else
#include <GL/glew.h>
#endif

#include "lp-render.h"

/*----------------------------------------------------------------------------*/

void draw(int w, int h)
{
    GLdouble x = (GLdouble) w / (GLdouble) h;

    glClearColor(0.2, 0.2, 0.2, 0.0);
    glClear(GL_COLOR_BUFFER_BIT);

    glViewport(0, 0, w, h);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-x, x, -1.0, 1.0, 2.0, 10.0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslated(0.0, 0.0, -5.0);

    glBegin(GL_QUADS);
    {
        glColor4d(1.0, 1.0, 0.0, 1.0);
        glVertex3d(0.0, 0.0, 0.0);
        glVertex3d(1.0, 0.0, 0.0);
        glVertex3d(1.0, 1.0, 0.0);
        glVertex3d(0.0, 1.0, 0.0);
    }
    glEnd();
}

