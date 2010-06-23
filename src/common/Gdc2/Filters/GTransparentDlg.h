#ifndef __GTransparentDlg_h
#define __GTransparentDlg_h

#ifdef FILTER_UI

#include "Lgi.h"
#include "GVariant.h"

class GTransparentDlg : public GDialog
{
	GRadioGroup *Grp;
	GVariant *Trans;

public:
	GTransparentDlg(GView *parent, GVariant *trans);
	int OnNotify(GViewI *Ctrl, int Flags);
};
#endif


#endif
