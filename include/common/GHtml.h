/// \file
/// \author Matthew Allen
#ifndef _GHTML_H
#define _GHTML_H

#include "GMap.h"
#include "GDocView.h"
#include "GHtmlStatic.h"

class GTag;
class GFontCache;

/// A lightwight scripting safe HTML control. It has limited CSS support, renders
/// most tables, even when nested. You can provide support for loading external
/// images by implementing the GDocumentEnv::GetImageUri method of the 
/// GDocumentEnv interface and passing it into GHtml::SetEnv.
/// All attempts to open URL's are passed into GDocumentEnv::OnNavigate method of the
/// environment if set. Likewise any content inside active scripting tags, e.g. &lt;? ?&gt;
/// will be striped out of the document and passed to GDocumentEnv::OnDynamicContent, which
/// should return the relevant HTML that the script resolves to. A reasonable default
/// implementation of the GDocumentEnv interface is the GDefaultDocumentEnv object.
///
/// You can set the content of the control through the GHtml::Name method.
///
/// Retreive any selected content through GHtml::GetSelection.
class GHtml :
	public GDocView,
	public ResObject
{
	friend class GTag;
	friend class GFlowRegion;
	class GHtmlPrivate *d;

protected:	
	// Data
	GFontCache			*FontCache;
	GTag				*Tag;				// Tree root
	GTag				*Cursor;			// Cursor location..
	GTag				*Selection;			// Edge of selection or NULL
	List<GTag>			OpenTags;
	char				*Source;
	char				*DocCharSet;
	char				IsHtml;
	int					ViewWidth;
	GToolTip			Tip;
	GTag				*PrevTip;
	GMap<char*,char*>	CssMap;

	// Display
	GSurface			*MemDC;

	// Methods
	void _New();
	void _Delete();
	GFont *DefFont();
	GTag *GetOpenTag(char *Tag);
	void Parse();
	void AddCss(char *Css);
	int ScrollY();
	void SetCursorVis(bool b);
	bool GetCursorVis();
	GRect *GetCursorPos();
	bool IsCursorFirst();
	int GetTagDepth(GTag *Tag);
	GTag *PrevTag(GTag *t);
	GTag *NextTag(GTag *t);
	GTag *GetLastChild(GTag *t);

	friend bool FindCallback(GFindReplaceCommon *Dlg, bool Replace, void *User);
	bool OnFind(class GFindReplaceCommon *Params);

public:
	GHtml(int Id, int x, int y, int cx, int cy, GDocumentEnv *system = 0);
	~GHtml();

	// Html
	const char *GetClass() { return "GHtml"; }

	/// Get the tag at an x,y location
	GTag *GetTagByPos(int x, int y, int *Index);
	/// Layout content and return size.
	GdcPt2 Layout();

	// Options
	bool GetLinkDoubleClick();
	void SetLinkDoubleClick(bool b);

	// GDocView
	
	/// Copy the selection to the clipboard
	bool Copy();
	/// Returns true if there is a selection
	bool HasSelection();
	/// Unselect all the text in the control
	void UnSelectAll();
	/// Select all the text in the control (not impl)
	void SelectAll();
	/// Return the selection in a dynamically allocated string
	char *GetSelection();
	
	// Prop
	void SetLoadImages(bool i);

	// Window

	/// Sets the HTML content of the control
	bool Name(const char *s);
	/// Returns the HTML content
	char *Name();
	/// Sets the HTML content of the control
	bool NameW(const char16 *s);
	/// Returns the HTML content
	char16 *NameW();

	// Impl
	void OnPaint(GSurface *pDC);
	void OnMouseClick(GMouse &m);
	void OnMouseMove(GMouse &m);
	void OnMouseWheel(double Lines);
	bool OnKey(GKey &k);
	int OnNotify(GViewI *c, int f);
	void OnPosChange();
	void OnPulse();
	int OnEvent(GMessage *Msg);
	const char *GetMimeType() { return "text/html"; }

	// Javascript handlers
	GDom *getElementById(char *Id);
};

/*
/// All the CSS styles the HTML control uses/supports
enum CssStyle
{
	CSS_NULL = 0,

	// Colour
	CSS_COLOUR,
	CSS_BACKGROUND,
	CSS_BACKGROUND_COLOUR,
	CSS_BACKGROUND_REPEAT,

	// Font
	CSS_FONT,
	CSS_FONT_SIZE,
	CSS_FONT_WEIGHT,
	CSS_FONT_FAMILY,
	CSS_FONT_STYLE,

	// Dimensions
	CSS_WIDTH,
	CSS_HEIGHT,

	// Margin
	CSS_MARGIN,
	CSS_MARGIN_LEFT,
	CSS_MARGIN_RIGHT,
	CSS_MARGIN_TOP,
	CSS_MARGIN_BOTTOM,

	// Padding
	CSS_PADDING,
	CSS_PADDING_LEFT,
	CSS_PADDING_TOP,
	CSS_PADDING_RIGHT,
	CSS_PADDING_BOTTOM,

	// Border
	CSS_BORDER,
	CSS_BORDER_LEFT,
	CSS_BORDER_TOP,
	CSS_BORDER_RIGHT,
	CSS_BORDER_BOTTOM,

	// Alignment
	CSS_ALIGN,
	CSS_VERTICAL_ALIGN,
	CSS_TEXT_ALIGN,
};
*/

/// The CSS align types
enum CssAlign
{
	AlignInherit,
	AlignLeft,
	AlignCenter,
	AlignRight,
	AlignJustify,
	AlignTop,
	AlignMiddle,
	AlignBottom,
};

/// Css background repeat options
enum CssBackgroundRepeat
{
	BgInherit,
	BgRepeat,
	BgRepeatX,
	BgRepeatY,
	BgNoRepeat,
};


#endif
