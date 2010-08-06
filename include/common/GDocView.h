/// \file
/// \author Matthew Allen (fret@memecode.com)
/// \brief This is the base data and code for all the text controls (inc. HTML)

#ifndef __GDOCVIEW_H
#define __GDOCVIEW_H

// Word wrap

/// No word wrapping
#define TEXTED_WRAP_NONE			0
/// Dynamically wrap line to editor width
#define TEXTED_WRAP_REFLOW			1

// Notify flags

/// GView::OnNotify flag: the document has changed
#define GTVN_DOC_CHANGED			0x01
/// GView::OnNotify flag: the cursor moved
#define GTVN_CURSOR_CHANGED			0x02
/// GView::OnNotify flag: the charset has changed
#define GTVN_CODEPAGE_CHANGED		0x04
/// GView::OnNotify flag: the fixed width font setting has changed
#define GTVN_FIXED_WIDTH_CHANGED	0x08
/// GView::OnNotify flag: the show images setting has changed
#define GTVN_SHOW_IMGS_CHANGED		0x10

// Util macros

/// Returns true if 'c' is an ascii character
#define IsAlpha(c)					(((c) >= 'a' AND (c) <= 'z') OR ((c) >= 'A' AND (c) <= 'Z'))
/// Returns true if 'c' is whitespace
#define IsWhiteSpace(c)				((c) < 126 AND strchr(GDocView::WhiteSpace, c) != 0)
/// Returns true if 'c' is a delimiter
#define IsDelimiter(c)				((c) < 126 AND strchr(GDocView::Delimiters, c) != 0)
/// Returns true if 'c' is a digit (number)
#define IsDigit(c)					((c) >= '0' AND (c) <= '9')
/// Returns true if 'c' is letter
#define IsLetter(c)					(((c) >= 'a' AND (c) <= 'z') OR ((c) >= 'A' AND (c) <= 'Z'))
/// Returns true if 'c' is a letter or number
#define IsText(c)					(IsDigit(c) OR IsAlpha(c) OR (c) == '_')
/// Returns true if 'c' is word boundry
#define IsWordBoundry(c)			(strchr(GDocView::WhiteSpace, c) OR strchr(GDocView::Delimiters, c))
/// Returns true if 'c' is a valid URL character
#define UrlChar(c)					( \
										strchr(GDocView::UrlDelim, (c)) OR \
										AlphaOrDigit((c)) OR \
										((c) >= 256) \
									)
/// Returns true if 'c' is email address character
#define EmailChar(c)				(strchr("._-:+", (c)) OR AlphaOrDigit((c)))

// Messages
#define M_IMAGE_LOADED				(M_USER + 0x500) // a=(char*)Uri, b=(GSurface*)pDC;

extern char16 *ConvertToCrLf(char16 *Text);

// Call back class to handle viewer events
class GDocView;

/// An environment class to handle requests from the text view to the outside world.
class GDocumentEnv
{
	friend class GDocView;

protected:
	GArray<GDocView*> Viewers;

public:
	GDocumentEnv(GDocView *v = 0);
	virtual ~GDocumentEnv();
	
	/// Creating a context menu, usually when the user right clicks on the 
	/// document.
	virtual bool AppendItems(GSubMenu *Menu, int Base = 1000) { return false; }
	
	/// Do something when the menu items created by GDocumentEnv::AppendItems 
	/// are clicked.
	virtual bool OnMenu(GDocView *View, int Id, void *Context) { return false; }
	
	/// Return an external image resource based on URI
	///
	/// Either by setting *pDC to something or by sending the GDocView a M_IMAGE_LOADED message
	/// sometime down the track with the Uri loaded and the image handle. This
	/// is useful for loading images in the background. In that case you should still return
	/// true here but leave the *pDC as NULL.
	virtual bool GetImageUri(char *Uri, GSurface **pDC, char *FileName = 0, int FileBufSize = 0) { return 0; }
	
	/// Handle a click on URI
	virtual bool OnNavigate(char *Uri) { return false; }

	/// Process dynamic content, returning a dynamically allocated string
	/// for the result of the executed script. Dynamic content is enclosed
	/// between &lt;? and ?&gt;.
	virtual char *OnDynamicContent(char *Code) { return 0; }

	/// Some script was received, the owner should compile it
	virtual bool OnCompileScript(char *Script, char *Language, char *MimeType) { return false; }

	/// Some script needs to be executed, the owner should compile it
	virtual bool OnExecuteScript(char *Script) { return false; }
};

/// Default text view environment
///
/// This class defines the default behaviour of the environment,
/// However you will need to instantiate this yourself and call
/// SetEnv with your instance. i.e. it's not automatic.
class GDefaultDocumentEnv : public GDocumentEnv
{
public:
	bool GetImageUri(char *Uri, GSurface **pDC, char *FileName = 0, int FileBufSize = 0);
	bool OnNavigate(char *Uri);
};

/// Find params
class GDocFindReplaceParams {};

/// TextView class is a base for all text controls
class
#ifdef MAC
LgiClass
#endif
GDocView : public GLayout
{
	friend class GDocumentEnv;

protected:
	GDocumentEnv *Environment;
	char *Charset;

public:
	// Static
	static char *WhiteSpace;
	static char *Delimiters;
	static char *UrlDelim;
	static bool AlphaOrDigit(char c);

	///////////////////////////////////////////////////////////////////////
	// Properties
	#define _TvMenuProp(Type, Name)						\
	protected:											\
		Type Name;										\
	public:												\
		virtual void Set##Name(Type i) { Name=i; }		\
		Type Get##Name() { return Name; }

	_TvMenuProp(uint16, WrapAtCol)
	_TvMenuProp(bool, UrlDetect)
	_TvMenuProp(bool, ReadOnly)
	_TvMenuProp(uint8, WrapType)
	_TvMenuProp(uint8, TabSize)
	_TvMenuProp(uint8, IndentSize)
	_TvMenuProp(bool, HardTabs)
	_TvMenuProp(bool, ShowWhiteSpace)
	_TvMenuProp(bool, ObscurePassword)
	_TvMenuProp(bool, CrLf)
	_TvMenuProp(bool, AutoIndent)
	_TvMenuProp(bool, FixedWidthFont)
	_TvMenuProp(COLOUR, BackColour)
	_TvMenuProp(bool, LoadImages)
	_TvMenuProp(bool, OverideDocCharset)
	#undef _TvMenuProp

	char *GetCharset() { return Charset; }
	void SetCharset(char *s) { char *cs = NewStr(s); DeleteArray(Charset); Charset = cs; }
	virtual char *GetMimeType() = 0;

	///////////////////////////////////////////////////////////////////////
	// Object
	GDocView(GDocumentEnv *e = 0)
	{
		WrapAtCol = 0;
		UrlDetect = true;
		ReadOnly = false;
		WrapType = TEXTED_WRAP_REFLOW;
		TabSize = 4;
		IndentSize = 4;
		HardTabs = true;
		ShowWhiteSpace = false;
		ObscurePassword = false;
		CrLf = false;
		AutoIndent = true;
		FixedWidthFont = false;
		BackColour = Rgb24(255, 255, 255);
		LoadImages = false;
		Charset = 0;
		OverideDocCharset = false;

		Environment = 0;

		SetEnv(e);
	}

	virtual ~GDocView()
	{
		SetEnv(0);
	}

	char *GetClass() { return "GDocView"; }

	/// Open a file handler
	virtual bool Open(char *Name, char *Cs = 0) { return false; }
	/// Save a file handler
	virtual bool Save(char *Name, char *Cs = 0) { return false; }

	///////////////////////////////////////////////////////////////////////

	/// Find window handler
	virtual bool DoFind() { return false; }
	/// Replace window handler
	virtual bool DoReplace() { return false; }
	virtual GDocFindReplaceParams *CreateFindReplaceParams() { return 0; }
	virtual void SetFindReplaceParams(GDocFindReplaceParams *Params) { }

	///////////////////////////////////////////////////////////////////////

	/// Get the current environment
	virtual GDocumentEnv *GetEnv() { return Environment; }
	/// Set the current environment
	virtual void SetEnv(GDocumentEnv *e)
	{
		if (Environment)
		{
			LgiAssert(Environment->Viewers.HasItem(this));
			Environment->Viewers.Delete(this);
		}
		Environment = e;
		if (Environment)
		{
			LgiAssert(!Environment->Viewers.HasItem(this));
			Environment->Viewers.Add(this);
		}
	}
	
	///////////////////////////////////////////////////////////////////////
	// State / Selection
	
	/// Set the cursor position, to select an area, move the cursor with Select=false
	/// then set the other end of the region with Select=true.
	virtual void SetCursor(int i, bool Select, bool ForceFullUpdate = false) {}

	/// Cursor=false means the other end of the selection if any. The cursor is alwasy
	/// at one end of the selection.
	virtual int GetCursor(bool Cursor = true) { return 0; }

	/// True if there is a selection
	virtual bool HasSelection() { return false; }
	/// Unselect all the text
	virtual void UnSelectAll() {}
	/// Select the word from index 'From'
	virtual void SelectWord(int From) {}
	/// Select all the text in the control
	virtual void SelectAll() {}
	/// Get the selection as a dynamicially allocated utf-8 string
	virtual char *GetSelection() { return 0; }

	/// Returns the character index at the x,y location
	virtual int IndexAt(int x, int y) { return 0; }

	/// Index=-1 returns the x,y of the cursor, Index >=0 returns the specified x,y
	virtual void PositionAt(int &x, int &y, int Index = -1) { }

	/// True if the document has changed
	virtual bool IsDirty() { return false; }
	/// Gets the number of lines of text
	virtual int GetLines() { return 0; }
	/// Gets the pixels required to display all the text
	virtual void GetTextExtent(int &x, int &y) {}

	///////////////////////////////////////////////////////////////////////
	
	/// Cuts the selection from the document and puts it on the clipboard
	virtual bool Cut() { return false; }
	/// Copies the selection from the document to the clipboard
	virtual bool Copy() { return false; }
	/// Pastes the current contents of the clipboard into the document
	virtual bool Paste() { return false; }

	///////////////////////////////////////////////////////////////////////

	/// Called when the user hits the escape key
	virtual void OnEscape(GKey &K) {}
	/// Called when the user hits the enter key
	virtual void OnEnter(GKey &k) {}
	/// Called when the user clicks a URL
	virtual void OnUrl(char *Url) {}

	///////////////////////////////////////////////////////////////////////

	/// Gets the document in format of a desired MIME type
	virtual bool GetFormattedContent(char *MimeType, GAutoString &Out) { return false; }
};

#endif
