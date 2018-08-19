#ifndef _IMAGE_COMPARE_H_
#define _IMAGE_COMPARE_H_

class ImageCompareDlg : public GWindow
{
	struct ImageCompareDlgPriv *d;

public:
	ImageCompareDlg(GView *p, const char *OutPath);
	~ImageCompareDlg();
	
	int OnNotify(GViewI *Ctrl, int Flags);
};

#endif