#include "Lgi.h"
// #include <cups/ipp.h>
// #include <cups/cups.h>

#define PS_SCALE			10

///////////////////////////////////////////////////////////////////////////////////////
class GPrintDCPrivate // : public GCups
{
public:
	class PrintPainter *p;
	Gtk::GtkPrintContext *Handle;
	GString PrintJobName;
	GString PrinterName;
	int Pages;
	GColour c;
	GRect Clip;
	
	GPrintDCPrivate(Gtk::GtkPrintContext *handle)
	{
		p = 0;
		Pages = 0;
		Handle = handle;
	}
	
	~GPrintDCPrivate()
	{
	}
	
	bool IsOk()
	{
		return	this != 0;
	}
};

/////////////////////////////////////////////////////////////////////////////////////
GPrintDC::GPrintDC(void *Handle, const char *PrintJobName, const char *PrinterName)
{
	d = new GPrintDCPrivate((Gtk::GtkPrintContext*)Handle);
	d->PrintJobName = PrintJobName;
	d->PrinterName = PrinterName;
	Cairo = gtk_print_context_get_cairo_context(d->Handle);
	ColourSpace = CsRgb24;
	d->Clip = Bounds();
}

GPrintDC::~GPrintDC()
{
	Cairo = NULL;
	DeleteObj(d);
}

Gtk::GtkPrintContext *GPrintDC::GetPrintContext()
{
	return d->Handle;
}

int GPrintDC::X()
{
	return gtk_print_context_get_width(d->Handle);
}

int GPrintDC::Y()
{
	return gtk_print_context_get_height(d->Handle);
}

int GPrintDC::GetBits()
{
	return 24;
}

int GPrintDC::DpiX()
{
	return Gtk::gtk_print_context_get_dpi_x(d->Handle);
}

int GPrintDC::DpiY()
{
	return Gtk::gtk_print_context_get_dpi_y(d->Handle);
}

GRect GPrintDC::ClipRgn(GRect *Rgn)
{
	GRect Prev = d->Clip;
	if (Rgn)
		d->Clip = *Rgn;
	else
		d->Clip = Bounds();
	return Prev;
}

GRect GPrintDC::ClipRgn()
{
	return d->Clip;
}

COLOUR GPrintDC::Colour()
{
	return d->c.c24();
}

COLOUR GPrintDC::Colour(COLOUR c, int Bits)
{
	GColour col(c, Bits);
	return Colour(col).c24();
}

GColour GPrintDC::Colour(GColour c)
{
	GColour Prev = d->c;
	d->c = c;
	if (Cairo)
		cairo_set_source_rgb(Cairo,
							(double)d->c.r() / 255.0,
							(double)d->c.g() / 255.0,
							(double)d->c.b() / 255.0);
	return Prev;
}

void GPrintDC::Set(int x, int y)
{
	if (Cairo)
	{
		cairo_new_path(Cairo);
		cairo_rectangle(Cairo, x, y, x+1, y+1);
		cairo_fill(Cairo);
	}
}

void GPrintDC::HLine(int x1, int x2, int y)
{
	Line(x1, y, x2, y);
}

void GPrintDC::VLine(int x, int y1, int y2)
{
	Line(x, y1, x, y2);
}

void GPrintDC::Line(int x1, int y1, int x2, int y2)
{
	if (Cairo)
	{
		cairo_set_line_width(Cairo, 0.5);
		cairo_new_path(Cairo);
		cairo_move_to(Cairo, x1, y1);
		cairo_line_to(Cairo, x2, y2);
		cairo_stroke(Cairo);
	}
}

void GPrintDC::Circle(double cx, double cy, double radius)
{
	LgiAssert(!"Not impl.");
}

void GPrintDC::FilledCircle(double cx, double cy, double radius)
{
	LgiAssert(!"Not impl.");
}

void GPrintDC::Arc(double cx, double cy, double radius, double start, double end)
{
	LgiAssert(!"Not impl.");
}

void GPrintDC::FilledArc(double cx, double cy, double radius, double start, double end)
{
	LgiAssert(!"Not impl.");
}

void GPrintDC::Ellipse(double cx, double cy, double x, double y)
{
	LgiAssert(!"Not impl.");
}

void GPrintDC::FilledEllipse(double cx, double cy, double x, double y)
{
	LgiAssert(!"Not impl.");
}

void GPrintDC::Box(int x1, int y1, int x2, int y2)
{
	GRect r(x1, y1, x2, y2);
	Box(&r);
}

void GPrintDC::Box(GRect *a)
{
	GRect r;
	if (a)
		r = *a;
	else
		r = Bounds();
	if (Cairo)
	{
		double Half = 0.5;
		cairo_set_line_width(Cairo, Half);
		cairo_new_path(Cairo);
		cairo_rectangle(Cairo,
						Half + r.x1,
						Half + r.y1,
						-Half + r.X(),
						-Half + r.Y());
		cairo_stroke(Cairo);
	}
}

void GPrintDC::Rectangle(int x1, int y1, int x2, int y2)
{
	GRect r(x1, y1, x2, y2);
	Rectangle(&r);
}

void GPrintDC::Rectangle(GRect *a)
{
	GRect r;
	if (a)
		r = *a;
	else
		r = Bounds();
	if (Cairo)
	{
		cairo_new_path(Cairo);
		cairo_rectangle(Cairo, r.x1, r.y1, r.X(), r.Y());
		cairo_fill(Cairo);
	}
}

void GPrintDC::Blt(int x, int y, GSurface *Src, GRect *SrcClip)
{
	GRect s = SrcClip ? *SrcClip : Src->Bounds();
	GRect d = s;
	d.ZOff(x, y);
	StretchBlt(&d, Src, &s);
}

void GPrintDC::StretchBlt(GRect *d, GSurface *Src, GRect *s)
{
	if (!Cairo)
	{
		LgiAssert(0);
		return;
	}
	
	uint8 *Scan0 = (*Src)[0];
	if (!Scan0)
	{
		LgiAssert(0);
		return;
	}
	
	Gtk::cairo_format_t Fmt = Gtk::CAIRO_FORMAT_INVALID;
	switch (Src->GetBits())
	{
		case 16:
			Fmt = Gtk::CAIRO_FORMAT_RGB16_565;
			break;
		case 24:
			Fmt = Gtk::CAIRO_FORMAT_RGB24;
			break;
		case 32:
			Fmt = Gtk::CAIRO_FORMAT_ARGB32;
			break;
	}
	if (Fmt == Gtk::CAIRO_FORMAT_INVALID)
	{
		LgiAssert(0);
		return;
	}
	
	Gtk::cairo_surface_t *Img = cairo_image_surface_create_for_data(Scan0,
																	Fmt,
																	Src->X(),
																	Src->Y(),
																	Src->GetRowStep());
	if (!Img)
	{
		LgiAssert(0);
		return;
	}

	Gtk::cairo_pattern_t *Pat = cairo_pattern_create_for_surface(Img);
	if (Pat)
	{
		Gtk::cairo_matrix_t m;
		double Sx = (double) s->X() / d->X();
		double Sy = (double) s->Y() / d->Y();
		cairo_matrix_init_scale(&m, Sx, Sy);
		cairo_matrix_translate(&m, -d->x1,-d->y1);
		cairo_pattern_set_matrix(Pat, &m);

		cairo_save(Cairo);
		cairo_set_source(Cairo, Pat);
		
		cairo_new_path(Cairo);
		cairo_rectangle(Cairo, d->x1, d->y1, d->X(), d->Y());
		cairo_fill(Cairo);
		
		cairo_restore(Cairo);
		cairo_pattern_destroy(Pat);
	}
	
	cairo_surface_destroy(Img);
}

void GPrintDC::Polygon(int Points, GdcPt2 *Data)
{
	LgiAssert(!"Not impl.");
}

void GPrintDC::Bezier(int Threshold, GdcPt2 *Pt)
{
	LgiAssert(!"Not impl.");
}

