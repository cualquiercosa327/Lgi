
#ifndef __QScrollBar_h
#define __QScrollBar_h

#include "xwidget.h"

class XScrollBar : public XWidget
{
	class XScrollBarPrivate *d;

public:
	enum Orientation
	{
		Vertical,
		Horizontal
	};

	XScrollBar(XWidget *p = 0, char *name = 0);
	~XScrollBar();

	// Api
	Orientation orientation();
	void setOrientation(Orientation o);
	int value();
	void setValue(int v);
	int minValue();
	int maxValue();
	void setRange(int min, int max);
	int pageStep();
	void setPageStep(int i);

	// Impl
	void paintEvent(XlibEvent *e);
	void resizeEvent(XlibEvent *e);
	void mousePressEvent(XlibEvent *e);
	void mouseReleaseEvent(XlibEvent *e);
	void mouseMoveEvent(XlibEvent *e);
	bool keyPressEvent(XlibEvent *e);
	void wheelEvent(XlibEvent *e);
};

#endif
