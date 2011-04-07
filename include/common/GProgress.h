/// \file
/// \author Matthew Allen (fret@memecode.com)
/// /brief A progress control

#ifndef _GPROGRESS_H_
#define _GPROGRESS_H_

/// Progress bar control
class LgiClass GProgress :
	public GControl,
	public Progress,
	public ResObject
{
	COLOUR c;
	#ifdef WIN32
	int Shift;
	#endif

public:
	GProgress(int id, int x, int y, int cx, int cy, const char *name);
	~GProgress();

	const char *GetClass() { return "GProgress"; }
	/// Sets the range that the Value() is between
	void SetLimits(int64 l, int64 h);
	/// Sets the point between the limits that defines where the progress is up to
	void Value(int64 v);
	int64 Value();
	int OnEvent(GMessage *Msg);

	#if WIN32NATIVE
	bool SetForegroundFill(GViewFill *Fill);
	bool SetBackgroundFill(GViewFill *Fill);
	#else
	void OnPaint(GSurface *pDC);
	void GetLimits(int64 &l, int64 &h);
	void Colour(COLOUR Col);
	COLOUR Colour();
	#endif
};

#endif
