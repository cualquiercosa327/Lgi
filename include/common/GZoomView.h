/// \author Matthew Allen <fret@memecode.com>
/// \brief Allows display of a zoomable and scroll bitmap using tiles.
#ifndef _ZOOM_VIEW_H_
#define _ZOOM_VIEW_H_

#include "GPath.h"

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
	
	virtual void DrawBackground(GSurface *Dst, GRect *Where) = 0;
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
		/// The current scroll offsets
		int Sx, Sy;
	};
	
	enum SampleMode
	{
		SampleNearest,
		SampleAverage,
		SampleMax,
	};

	// Notifications
	enum NotifyEvents
	{
		NotifyViewportChanged = 1000,
	};

	GZoomView(GZoomViewCallback *App);
	~GZoomView();

	// Methods
	void SetCallback(GZoomViewCallback *cb);
	bool Convert(GPointF &p, int x, int y);
	ViewportInfo GetViewport();
	void SetViewport(ViewportInfo i);
	void SetSampleMode(SampleMode sm);

	// Subclass
	void SetSurface(GSurface *dc, bool own);
	GSurface *GetSurface();
	void Update(GRect *Where = NULL);
	void Reset();
	int GetBlockSize();

	// Events
	void OnMouseClick(GMouse &m);
	bool OnMouseWheel(double Lines);
	void OnPulse();
	void OnPaint(GSurface *pDC);
	int OnNotify(GViewI *v, int f);
	GMessage::Param OnEvent(GMessage *m);
	bool OnLayout(GViewLayoutInfo &Inf);
};

#endif