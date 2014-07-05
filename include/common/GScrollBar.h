/// \file
/// \author Matthew Allen (fret@memecode.com)
/// /brief A tree/heirarchy control

#ifndef __GScrollBar_h
#define __GScrollBar_h

#define SCROLL_BAR_SIZE			15

/// \brief Scroll bar control
///
/// This control can be used as an actual free standing window or to address built in controls on a GLayout
/// view.
class LgiClass GScrollBar :
	public GControl,
	public ResObject
{
	friend class GLayout;

protected:
	class GScrollBarPrivate *d;

	#if WINNATIVE
	GViewI *GetMyView();
	void Update();
	void SetParentFlag(bool Bool);
	#endif

public:
	/// Call this constructor for embeded scrollbar say in a window
	GScrollBar();
	
	const char *GetClass() { return "GScrollBar"; }

	/// Call this constructor for a control based scrollbar, say in a dialog
	GScrollBar(int id, int x, int y, int cx, int cy, const char *name);
	~GScrollBar();

	/// Returns the size of the bar, i.e. the width if vertical or the height if horizontal
	static int GetScrollSize();

	/// True if vertical
	bool Vertical();
	/// Makes the scrollar vertical
	void SetVertical(bool v);
	/// Gets the current position of the scrollbar
	int64 Value();
	/// Sets the position of the scrollbar
	void Value(int64 p);
	/// Gets the limits
	void Limits(int64 &Low, int64 &High);
	/// Sets the limits
	void SetLimits(int64 Low, int64 High);
	/// Gets the page size
	int Page();
	/// Sets the page size
	void SetPage(int p);
	/// Returns true if the range is valid
	bool Valid();

	#if WINNATIVE
	bool SetPos(GRect &p, bool Repaint = false);
	void SetParent(GViewI *p);
	bool Invalidate(GRect *r = NULL, bool Repaint = false, bool NonClient = false);
	#else
	bool Attach(GViewI *p);
	void OnPaint(GSurface *pDC);
	void OnPosChange();
	void OnMouseClick(GMouse &m);
	void OnMouseMove(GMouse &m);
	bool OnKey(GKey &k);
	bool OnMouseWheel(double Lines);
	void OnPulse();
	#endif
	
	// events
	GMessage::Result OnEvent(GMessage *Msg);

	/// Called when the value changes
	virtual void OnChange(int Pos) {}
	/// Called when the Limits or Page changes
	virtual void OnConfigure() {}
};

#endif
