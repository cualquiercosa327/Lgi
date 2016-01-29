/// \author Matthew Allen <fret@memecode.com>
/// \brief Allows display of a zoomable and scroll bitmap using tiles.
#ifndef _ZOOM_VIEW_H_
#define _ZOOM_VIEW_H_

#include "GPath.h"
#include "GNotifications.h"

class GZoomView;

class GZoomViewCallback
{
public:
	#define DefOption(type, name, default) \
		virtual type name() { return default; } \
		virtual void name(type i) {}

	DefOption(int, DisplayGrid, false);
	DefOption(int, GridSize, 0);
	DefOption(int, DisplayTile, false);
	DefOption(int, TileType, 0);
	DefOption(int, TileX, 32);
	DefOption(int, TileY, 32);
	
	#undef DefOption
	
	virtual void DrawBackground(GZoomView *View, GSurface *Dst, GdcPt2 Offset, GRect *Where = NULL) = 0;
	virtual void DrawForeground(GZoomView *View, GSurface *Dst, GdcPt2 Offset, GRect *Where = NULL) = 0;
	virtual void SetStatusText(const char *Msg, int Pane = 0) {}
};

#define GZOOM_SELECTION_MS          100

/// Zoomable and scrollable bitmap viewer.
class GZoomView : public GLayout, public ResObject
{
    class GZoomViewPriv *d;

public:
	struct ViewportInfo
	{
		/// The current zoom level
		///
		/// < 0 then the image is scaled down by (1 - zoom) times
		/// == 0 then the image is 1:1
		/// > 0 then the image is scale up by (zoom - 1) times
		int Zoom;
		/// The current scroll offsets in document coordinates (not screen space)
		int Sx, Sy;
	};
	
	enum SampleMode
	{
		SampleNearest,
		SampleAverage,
		SampleMax,
	};
	
	enum DefaultZoomMode
	{
		ZoomFitBothAxis,
		ZoomFitX,
		ZoomFitY,
		/* Not implemented yet...
		ZoomFitLongest,
		ZoomFitShortest,
		*/
	};

	// Notifications
	GZoomView(GZoomViewCallback *App);
	~GZoomView();

	// Methods
	void SetCallback(GZoomViewCallback *cb);
	bool Convert(GPointF &p, int x, int y);
	ViewportInfo GetViewport();
	void SetViewport(ViewportInfo i);
	void SetSampleMode(SampleMode sm);
	void SetDefaultZoomMode(DefaultZoomMode m);
	DefaultZoomMode GetDefaultZoomMode();
	void ScrollToPoint(GdcPt2 DocCoord);

	// Subclass
	void SetSurface(GSurface *dc, bool own);
	GSurface *GetSurface();
	void Update(GRect *Where = NULL);
	void Reset();
	int GetBlockSize();

	// Events and Impl
	void OnMouseClick(GMouse &m);
	bool OnMouseWheel(double Lines);
	void OnPulse();
	void OnPaint(GSurface *pDC);
	int OnNotify(GViewI *v, int f);
	GMessage::Param OnEvent(GMessage *m);
	bool OnLayout(GViewLayoutInfo &Inf);
	void UpdateScrollBars(GdcPt2 *MaxScroll = NULL, bool ResetPos = false);
	void OnPosChange();
};

#endif