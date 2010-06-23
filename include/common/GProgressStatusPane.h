
#ifndef __PROGRESS_STATUS_PANE
#define __PROGRESS_STATUS_PANE

// Progress pane
class GProgressStatusPane : public GStatusPane, public Progress, public DoEvery {

public:
	GProgressStatusPane();
	~GProgressStatusPane();

	void OnPaint(GSurface *pDC);
	int64 Value();
	void Value(int64 v);
};

#endif
