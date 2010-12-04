/// \file
/// \author Matthew Allen (fret@memecode.com)
/// \brief This is the base data and code for all the text controls (inc. HTML)

#ifndef __GDOCVIEW_H
#define __GDOCVIEW_H

#include "GVariant.h"

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
#define IsAlpha(c)					(((c) >= 'a' && (c) <= 'z') || ((c) >= 'A' && (c) <= 'Z'))
/// Returns true if 'c' is whitespace
#define IsWhiteSpace(c)				((c) < 126 && strchr(GDocView::WhiteSpace, c) != 0)
/// Returns true if 'c' is a delimiter
#define IsDelimiter(c)				((c) < 126 && strchr(GDocView::Delimiters, c) != 0)
/// Returns true if 'c' is a digit (number)
#define IsDigit(c)					((c) >= '0' && (c) <= '9')
/// Returns true if 'c' is letter
#define IsLetter(c)					(((c) >= 'a' && (c) <= 'z') || ((c) >= 'A' && (c) <= 'Z'))
/// Returns true if 'c' is a letter or number
#define IsText(c)					(IsDigit(c) || IsAlpha(c) || (c) == '_')
/// Returns true if 'c' is word boundry
#define IsWordBoundry(c)			(strchr(GDocView::WhiteSpace, c) || strchr(GDocView::Delimiters, c))
/// Returns true if 'c' is alphanumeric or a digit
#define AlphaOrDigit(c)				(IsDigit(c) || IsLetter(c))
/// Returns true if 'c' is a valid URL character
#define UrlChar(c)					( \
										strchr(GDocView::UrlDelim, (c)) || \
										AlphaOrDigit((c)) || \
										((c) >= 256) \
									)
/// Returns true if 'c' is email address character
#define EmailChar(c)				(strchr("._-:+", (c)) || AlphaOrDigit((c)))

extern char16 *ConvertToCrLf(char16 *Text);

/// This class contains infomation about a link.
/// \sa LgiDetectLinks
struct GLinkInfo
{
	int Start;
	int Len;
	bool Email;
};

/// Detects links in text, returning their location and type
extern bool LgiDetectLinks(GArray<GLinkInfo> &Links, char16 *Text, int Size = -1);

// Call back class to handle viewer events
class GDocView;

/// An environment class to handle requests from the text view to the outside world.
class GDocumentEnv : public GThreadOwner
{
	GArray<GDocView*> Viewers;

public:
	GDocumentEnv(GDocView *v = 0);
	virtual ~GDocumentEnv();

	enum LoadType
	{
		LoadError,
		LoadNotImpl,
		LoadImmediate,
		LoadDeferred,
	};

	struct LoadJob : public GThreadJob
	{
		enum PrefFormat
		{
			FmtNone,
			FmtStream,
			FmtSurface,
			FmtFilename,
		};
		
		// View data
		GDocView *View;
		void *UserData;
		uint32 UserUid;
		PrefFormat Pref;

		// Input data
		GAutoString Uri;

		// Output data
		GAutoPtr<GStreamI> Stream;
		GAutoPtr<GSurface> pDC;
		GAutoString Filename;
		GAutoString Error;

		LoadJob(GThreadTarget *o) : GThreadJob(o)
		{
			View = 0;
			UserUid = 0;
			UserData = 0;
			Pref = FmtNone;
		}
	};

	LoadJob *NewJob()
	{
		return new LoadJob(this);
	}

	bool AttachView(GDocView *v)
	{
		if (!v)
			return false;
		if (!Lock(_FL))
			return false;
		LgiAssert(!Viewers.HasItem(v));
		Viewers.Add(v);
		Unlock();
		return true;
	}

	bool DetachView(GDocView *v)
	{
		if (!v)
			return false;
		if (!Lock(_FL))
			return false;
		LgiAssert(Viewers.HasItem(v));
		Viewers.Delete(v);
		Unlock();
		return true;
	}
	
	/// Creating a context menu, usually when the user right clicks on the 
	/// document.
	virtual bool AppendItems(GSubMenu *Menu, int Base = 1000) { return false; }
	
	/// Do something when the menu items created by GDocumentEnv::AppendItems 
	/// are clicked.
	virtual bool OnMenu(GDocView *View, int Id, void *Context) { return false; }
	
	/// Asks the env to get some data linked from the document, e.g. a css file or an iframe source etc.
	/// If the GetContent implementation takes ownership of the job pointer then it should set 'j' to NULL.
	virtual LoadType GetContent(LoadJob *&j) { return LoadNotImpl; }
	/// After the env's thread loads the resource it calls this to pass it to the doc
	void OnDone(GThreadJob *j);
	
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
	LoadType GetContent(LoadJob *&j);
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
		if (Environment) Environment->DetachView(this);
		Environment = e;
		if (Environment) Environment->AttachView(this);
	}
	/// When the env has loaded a resource it can pass it to the doc control via this method.
	/// It MUST be thread safe. Often an environment will call this function directly from
	/// it's worker thread.
	virtual void OnContent(GDocumentEnv::LoadJob *Res) {}
	
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

	struct ContentMedia
	{
		GAutoString Id;
		GVariant Data;
		GAutoPtr<GStream> Stream;
	};

	/// Gets the document in format of a desired MIME type
	virtual bool GetFormattedContent
	(
		/// [In] The desired mime type of the content
		char *MimeType,
		/// [Out] The content in the specified mime type
		GAutoString &Out,
		/// [Out/Optional] Any attached media files that the content references
		GArray<ContentMedia> *Media = 0
	)
	{ return false; }
};

#endif
