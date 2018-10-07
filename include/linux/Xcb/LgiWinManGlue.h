/// \file
/// \author Matthew Allen
/// \brief This is the layer between LGI and the window manager on Linux.
#ifndef __LGI_WINDOWN_MANAGER_GLUE_H__
#define __LGI_WINDOWN_MANAGER_GLUE_H__

#include "LgiDefs.h"
#include "GMem.h"
#include "GArray.h"
#include "LgiClass.h"

/// A RGB colour
struct WmColour
{
	/// the colour components
	uint16 r, g, b, a;
	/// the maximum value for each componet, normal 255 for 8 bit components but could
	/// be higher if more precision is available.
	uint16 range;
	
	WmColour()
	{
		r = g = b = 0;
		range = a = 255;
	}
};

struct WmInitParams
{
	#ifndef _XLIB_H_
	// struct
	#endif
	xcb_connection_t *Dsp;
	int Args;
	char **Arg;
};

typedef bool (*Proc_LgiWmInit)(WmInitParams *Params);
typedef bool (*Proc_LgiWmExit)();
typedef bool (*Proc_LgiWmFileToMime)(char *Mime, char *File);
typedef bool (*Proc_LgiWmMimeToApps)(char *Mime, GArray<GAppInfo*> &Apps, int Limit);
typedef bool (*Proc_LgiWmGetSysFont)(char *Type, char *Font, int FontLen, int &PtSize);
typedef bool (*Proc_LgiWmGetLanguage)(char *Lang);
typedef bool (*Proc_LgiWmGetColour)(int Which, WmColour *Colour);
typedef bool (*Proc_LgiWmGetPath)(int Which, char *Buf, int BufLen);

#endif