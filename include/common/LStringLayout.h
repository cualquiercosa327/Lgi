#ifndef _GDISPLAY_STRING_LAYOUT_H_
#define _GDISPLAY_STRING_LAYOUT_H_

#include "GUtf8.h"
#include "GDisplayString.h"
#include "GCss.h"
#include "GFontCache.h"

/// Section of text with the same style.
struct LLayoutRun : public GCss
{
	GString Text;

	LLayoutRun(GCss *Style)
	{
		if (Style)
		{
			GCss *This = this;
			*This = *Style;
		}
	}
};

/// Display string of a certain style.
struct LLayoutString : public GDisplayString
{
	int Fx, y, Line;
	GColour Fore, Back;
	ssize_t Offset;
	LLayoutRun *Src;
	
	LLayoutString(GFont *f,
				const char *s,
				ssize_t l = -1,
				GSurface *pdc = 0) :
		GDisplayString(f, s, l, pdc)
	{
		Offset = 0;
		Fx = y = 0;
		Src = NULL;
	}

	LLayoutString(LLayoutString *Ls,
				const char *s,
				ssize_t l = -1) :
		GDisplayString(Ls->GetFont(), s, l)
	{
		Fx = Ls->Fx;
		y = Ls->y;
		Fore = Ls->Fore;
		Back = Ls->Back;
		Line = Ls->Line;
		Offset = Ls->Offset;
		Src = Ls->Src;
	}

	void Set(int LineIdx, int FixX, int YPx, LLayoutRun *Lr, ssize_t Start);
};

/// This class lays out a block of text according to the given styles. It
/// builds upon the GDisplayString class to render sections of text.
class LStringLayout
{
protected:
	GFontCache *FontCache;

	// Min and max bounds
	GdcPt2 Min, Max;
	int MinLines;
	
	// Setting
	bool Wrap;
	bool AmpersandToUnderline;

	// Array of display strings...
	GArray<LLayoutRun*> Text;
	GArray<GDisplayString*> Strs;
	GRect Bounds;

public:
	LStringLayout(GFontCache *fc);	
	~LStringLayout();

	void Empty();

	bool GetWrap() { return Wrap; }
	void SetWrap(bool b) { Wrap = b; }
	GdcPt2 GetMin() { return Min; }
	GdcPt2 GetMax() { return Max; }
	GArray<GDisplayString*> *GetStrs() { return &Strs; }
	GRect GetBounds() { return Bounds; }

	/// Adds a run of text with the same style
	bool Add(const char *Str, GCss *Style);
	uint32 NextChar(char *s);
	uint32 PrevChar(char *s);
	GFont *GetBaseFont();
	void SetBaseFont(GFont *f);

	// Pre-layout min/max calculation
	void DoPreLayout(int32 &MinX, int32 &MaxX);

	// Creates the layout of from the runs in 'Text'
	bool DoLayout(int Width, int MinYSize = 0, bool Debug = false);
	
	/// Paints the laid out strings at 'pt'.
	void Paint(	GSurface *pDC,
				GdcPt2 pt,
				GColour Back,
				GRect &rc,
				bool Enabled);
};

#endif
