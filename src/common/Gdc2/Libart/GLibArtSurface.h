#ifndef _LIB_ART_SURFACE_H
#define _LIB_ART_SURFACE_H

#include "libart_lgpl/libart.h"
#include "libart_lgpl/art_render.h"
#include "libart_lgpl/art_render_svp.h"

class GLibArtPen
{
	friend class GLibArtSurface;
	friend class GLibArtPath;

	COLOUR Colour;
	double Width;
	double MiterLimit;
	double Flatness;
	ArtPathStrokeJoinType Join;
	ArtPathStrokeCapType Cap;

public:
	GLibArtPen()
	{
		Colour = 0;
		Width = 1.0;
		MiterLimit = 2.0;
		Flatness = 1.0;
		Join = ART_PATH_STROKE_JOIN_MITER;
		Cap = ART_PATH_STROKE_CAP_BUTT;
	}

	GLibArtPen(COLOUR c)
	{
		Colour = c;
		Width = 1.0;
		MiterLimit = 2.0;
		Flatness = 1.0;
		Join = ART_PATH_STROKE_JOIN_MITER;
		Cap = ART_PATH_STROKE_CAP_BUTT;
	}

	GLibArtPen(COLOUR c, double wid)
	{
		Colour = c;
		Width = wid;
		MiterLimit = 2.0;
		Flatness = 1.0;
		Join = ART_PATH_STROKE_JOIN_MITER;
		Cap = ART_PATH_STROKE_CAP_BUTT;
	}
};

class GLibArtPath
{
	int Points;
	int Max;
	ArtVpath *Path;

	void _Add(ArtPathcode code, double x = 0.0, double y = 0.0);

public:
	GLibArtPath();
	~GLibArtPath();

	int GetPoints();
	ArtVpath *PointAt(int i);
	ArtSVP *FillSvp();
	ArtSVP *StrokeSvp(GLibArtPen &Pen);

	void AddCircle(double x, double y, double r);
	void AddRectangle(int x1, int y1, int x2, int y2);
	void AddRectangle(double x1, double y1, double x2, double y2);
	void AddRoundedRectangle(double x1, double y1, double x2, double y2, double r);
	void AddLine(int x1, int y1, int x2, int y2);
	void AddLine(double x1, double y1, double x2, double y2);
};

class GLibArtSurface : public GMemDC
{
	ArtRender * Ar;

public:
	GLibArtSurface(int x = -1, int y = -1);

	ArtRender *RenderNew(int x, int y);
	void DrawPath(GLibArtPen &Pen, GLibArtPath *Path);
	void FillPath(COLOUR c, GLibArtPath *Path);

	COLOUR Colour(COLOUR c, int Bits = -1);
};


#endif
