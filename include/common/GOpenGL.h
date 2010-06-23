#ifndef _GOPENGL_H_
#define _GOPENGL_H_

#include <gl/gl.h>
#include <gl/glu.h>

class GOpenGL : public GView
{
	struct GOpenGLPriv *d;

public:
	GOpenGL();
	~GOpenGL();

	void OnCreate();
	void SwapBuffers();
};

#endif