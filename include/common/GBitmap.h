/// \file
/// \author Matthew Allen (fret@memecode.com)
/// \brief Bitmap control

#ifndef _GBITMAP_H_
#define _GBITMAP_H_

/// Bitmap control
class LgiClass GBitmap :
	public GControl,
	public ResObject
{
	GSurface *pDC;
	class GThread *pThread;

public:
	/// Construct the bitmap control with the usual parameters
	GBitmap(int id, int x, int y, char *FileName, bool Async = false);
	~GBitmap();

	/// Sets the surface to display in the control
	virtual void SetDC(GSurface *pDC = 0);
	/// Gets the surface being displayed
	virtual GSurface *GetSurface();

	GMessage::Result OnEvent(GMessage *Msg);
	void OnPaint(GSurface *pDC);
	void OnMouseClick(GMouse &m);
};

#endif