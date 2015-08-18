#ifndef _GCSSTOOLS_H_
#define _GCSSTOOLS_H_

class LgiClass GCssTools
{
	GCss *Css;
	GFont *Font;
	
	GColour Fore, Back;
	uint8 ForeInit : 1, BackInit : 1;
	
	bool SetLineStyle(GSurface *pDC, GCss::BorderDef &d);
	
public:
	GCssTools(GCss *css, GFont *font)
	{
		Css = css;
		Font = font;
		ForeInit = 0;
		BackInit = 0;
	}
	
	/// Gets the foreground colour for text
	GColour &GetFore(GColour *Default = NULL);
	
	/// Gets the background colour for filling
	GColour &GetBack(GColour *Default = NULL);

	/// Applies the margin to a rectangle
	GRect ApplyMargin(GRect &in);

	/// Applies the padding to a rectangle
	GRect ApplyPadding(GRect &in);

	/// Paint the CSS border
	/// \returns the content area within the borders
	GRect PaintBorder(GSurface *pDC, GRect &in);

	/// Paint the CSS padding
	/// \returns the content area within the padding
	GRect PaintPadding(GSurface *pDC, GRect &in);
	
	GRect PaintBorderAndPadding(GSurface *pDC, GRect &in)
	{
		GRect r = PaintBorder(pDC, in);
		r = PaintPadding(pDC, r);
		return r;
	}
	
	/// Paint the content area
	void PaintContent(GSurface *pDC, GRect &in, const char *utf8, GSurface *img = NULL);
};

#endif