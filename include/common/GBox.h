#ifndef _GBOX_H_
#define _GBOX_H_

/// This is a vertical or horizontal layout box, similar to the
/// old GSplitter control except it can handle any number of children
class GBox : public GView
{
	struct GBoxPriv *d;

protected:

public:
	GBox(int Id = -1);
	~GBox();
	
	void OnCreate();
	void OnPaint(GSurface *pDC);
	void OnPosChange();
};

#endif