#include <GL/glew.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glut.h>

#include <GLCheck.h>
#include <EGL.h>
#include <iostream>


int main(int argc, char **argv){
 // Setup EGL to off-screen rendering
EGLCtx egl;
//egl.PrintInformation();
  
if(!checkGLVersion()) {
    return 1;
}

	std::cout << glGetString(GL_VENDOR) << std::endl;
	return 0;	
}
