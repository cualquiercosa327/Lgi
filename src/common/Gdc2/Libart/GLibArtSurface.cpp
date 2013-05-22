#include <math.h>

#include "Lgi.h"
#include "GLibArtSurface.h"

GLibArtSurface::GLibArtSurface(int x, int y)
{
	Ar = 0;

	if (x > 0 AND y > 0)
	{
		RenderNew(x, y);
	}
}

ArtRender *GLibArtSurface::RenderNew(int x, int y)
{
	if (Create(x, y, 32))
	{
		// Clear surface
		Colour(0);
		Rectangle();

		// Create render
		Ar = ::art_render_new(	0, 0,
								x-1, y-1,
								pMem->Base,
								pMem->Line,
								3,
								8,
								ART_ALPHA_PREMUL,
								0);
	}

	return Ar;
}

COLOUR GLibArtSurface::Colour(COLOUR c, int Bits)
{
	COLOUR Old = GSurface::Colour(c, Bits);

	if (Ar)
	{
		Ar->opacity = (c & 0xff000000) >> 16;
		
		c = GSurface::Colour();
		
		ArtPixMaxDepth Col[] = { B32(c), G32(c), R32(c) };

		art_render_image_solid(Ar, Col);
	}

	return Old;
}

void GLibArtSurface::DrawPath(GLibArtPen &Pen, GLibArtPath *Path)
{
	if (Path)
	{
		Colour(Pen.Colour);

		ArtSVP *Svp = Path->StrokeSvp(Pen);
		if (Svp)
		{
			art_render_svp(Ar, Svp);
			art_render_invoke(Ar);
			art_svp_free(Svp);
		}
	}
}

void GLibArtSurface::FillPath(COLOUR c, GLibArtPath *Path)
{
	if (Path)
	{
		Colour(c);

		ArtSVP *Svp = Path->FillSvp();
		if (Svp)
		{
			art_render_svp(Ar, Svp);
			art_render_invoke(Ar);
			art_svp_free(Svp);
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////
#define CIRCLE_STEPS			128

GLibArtPath::GLibArtPath()
{
	Points = 0;
	Path = 0;
	Max = 0;
}

GLibArtPath::~GLibArtPath()
{
	art_free(Path);
}

ArtSVP *GLibArtPath::FillSvp()
{
	_Add(ART_END);
	return art_svp_from_vpath(Path);
}

ArtSVP *GLibArtPath::StrokeSvp(GLibArtPen &Pen)
{
	_Add(ART_END);
	return art_svp_vpath_stroke (	Path,
									Pen.Join,
									Pen.Cap,
									Pen.Width,
									Pen.MiterLimit,
									Pen.Flatness);
}

ArtVpath *GLibArtPath::PointAt(int i)
{
	if (Path AND i >= 0 AND i<Points)
	{
		return Path + i;
	}

	return 0;
}

int GLibArtPath::GetPoints()
{
	return Points;
}

void GLibArtPath::_Add(ArtPathcode code, double x, double y)
{
	int i = Points++;
	if (i >= Max)
	{
		if (!Max) Max = 8;
		else Max <<= 1;
		Path = art_renew(Path, ArtVpath, Max);
	}

	if (Path)
	{
		Path[i].code = code;
		Path[i].x = x;
		Path[i].y = y;
	}
}

void GLibArtPath::AddCircle(double x, double y, double r)
{
	int i;
	double theta;

	for (i = 0; i < CIRCLE_STEPS + 1; i++)
	{
		theta = (i & (CIRCLE_STEPS - 1)) * (M_PI * 2.0 / CIRCLE_STEPS);
		_Add(i ? ART_LINETO : ART_MOVETO, x + r * cos(theta), y - r * sin(theta));
	}
}

void GLibArtPath::AddRoundedRectangle(double x1, double y1, double x2, double y2, double r)
{
	int i;
	double theta, x, y, d;

	d = min(x2-x1, y2-y1);
	r = min(d / 2, r);
	x = x2 - r;
	y = y1 + r;
	for (i = 0; i < CIRCLE_STEPS + 1; i++)
	{
		theta = (i & (CIRCLE_STEPS - 1)) * (M_PI * 2.0 / CIRCLE_STEPS);
		
		double tx = x + r * cos(theta);
		double ty = y - r * sin(theta);
		_Add(i ? ART_LINETO : ART_MOVETO, x + r * cos(theta), y - r * sin(theta));

		if (i == CIRCLE_STEPS / 4)
		{
			_Add(ART_LINETO, x1+r, y1);
			x = x1 + r;
			y = y1 + r;
		}
		else if (i == CIRCLE_STEPS / 2)
		{
			_Add(ART_LINETO, x1, y2-r);
			x = x1 + r;
			y = y2 - r;
		}
		else if (i == CIRCLE_STEPS * 3 / 4)
		{
			_Add(ART_LINETO, x2-r, y2);
			x = x2 - r;
			y = y2 - r;
		}
		else if (i == CIRCLE_STEPS)
		{
			_Add(ART_LINETO, x2, y1+r);
			break;
		}
	}
}

void GLibArtPath::AddRectangle(int x1, int y1, int x2, int y2)
{
	_Add(ART_MOVETO, x1, y1);
	_Add(ART_LINETO, x1, y2);
	_Add(ART_LINETO, x2, y2);
	_Add(ART_LINETO, x2, y1);
	_Add(ART_LINETO, x1, y1);
}

void GLibArtPath::AddRectangle(double x1, double y1, double x2, double y2)
{
	_Add(ART_MOVETO, x1, y1);
	_Add(ART_LINETO, x1, y2);
	_Add(ART_LINETO, x2, y2);
	_Add(ART_LINETO, x2, y1);
	_Add(ART_LINETO, x1, y1);
}

void GLibArtPath::AddLine(int x1, int y1, int x2, int y2)
{
	_Add(ART_MOVETO, x1, y1);
	_Add(ART_LINETO, x2, y2);
}

void GLibArtPath::AddLine(double x1, double y1, double x2, double y2)
{
	_Add(ART_MOVETO, x1, y1);
	_Add(ART_LINETO, x2, y2);
}

