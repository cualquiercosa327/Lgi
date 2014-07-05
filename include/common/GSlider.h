/// \file
/// \author Matthew Allen (fret@memecode.com)
/// \brief A slider control

#ifndef _GSLIDER_H_
#define _GSLIDER_H_

/// Slider
class LgiClass GSlider :
	public GControl,
	public ResObject
{
	#if WINNATIVE
	uint32 Style();
	int SysOnNotify(int Code);
	#endif

	bool Vertical;
	int Min, Max;
	int Val;

	GRect Thumb;
	int Tx, Ty;

public:
	GSlider(int id, int x, int y, int cx, int cy, const char *name, bool vert);
	~GSlider();

	const char *GetClass() { return "GSlider"; }

	/// Sets the position of the slider
	void Value(int64 i);
	/// Gets the position of the slider
	int64 Value();
	/// Gets the limits of the slider
	void GetLimits(int64 &x, int64 &y);
	/// Sets the limits of the slider
	void SetLimits(int64 x, int64 y);

	GMessage::Result OnEvent(GMessage *Msg);
	
	#if defined(__GTK_H__) || defined(MAC) || defined(BEOS)
	void OnPaint(GSurface *pDC);
	void OnMouseClick(GMouse &m);
	void OnMouseMove(GMouse &m);
	#endif
};

#endif