/**
	\file
	\author Matthew Allen
	\date 8/9/1998
	\brief Dialog widgets / components
*/

#ifndef __LGI_WIDGETS_H
#define __LGI_WIDGETS_H

/////////////////////////////////////////////////////////////////////////////////////////
#ifdef IDM_STATIC
#undef IDM_STATIC
#endif
#define IDM_STATIC				-1

/////////////////////////////////////////////////////////////////////////////////////////
class GDialog;
#if defined WIN32
class GDlgFunc;
#endif
class GListItem;
class GTreeItem;

class GButton;
class GEdit;
class GCheckBox;
class GText;
class GRadioGroup;
class GRadioButton;
class GTabView;
class GTabPage;
class GSlider;
class GCombo;
class GBitmap;
class GList;
class GTree;

///////////////////////////////////////////////////////////////////////////////////////////////

/// Resource loader
class LgiClass GLgiRes
{
public:
	/// Loading a dialog from the resource collection
	bool LoadFromResource
	(
		/// The resource ID
		int Resource,
		/// The target view to populate
		GViewI *Parent,
		/// The size of the view in the resource
		GRect *Pos = 0,
		/// The name of the window
		GAutoString *Name = 0,
		/// [Optional] List of tags to exclude/include items
		char *TagList = 0
	);
};

/// \brief A top level dialog window.
///
/// GDialog's can either be modal or modeless. A modal dialog blocks the user from accessing
/// the parent GWindow until the dialog has been dismissed. A modeless dialog behaves like
/// any other top level window in that it doesn't block the user accessing any other top level
/// window while it's open.
///
/// GDialog's can be created in two different ways. Firstly you can create code to instantiate
/// all the various controls and add them manually to the GView::Children list. Or you can load
/// all the controls from a resource file. The resource files are in XML format and there is a
/// graphical tool <a href="http://www.memecode.com/lgires.php">LgiRes</a>.
///
/// Manual example:
/// \code
/// #define IDC_STRING			100
///
/// class Example : public GDialog
/// {
/// public:
///		char *Str;
///
/// 	Example(GView *p)
/// 	{
///			Str = 0;
/// 		SetParent(p);
/// 		GRect r(0, 0, 400, 300);
/// 		SetPos(p);
/// 		MoveToCenter();
/// 			
/// 		Children.Insert(new GEdit(IDC_STRING, 10, 10, 200, 20, ""));
/// 		Children.Insert(new GButton(IDOK, 10, 30, 80, 20, "Ok"));
/// 		Children.Insert(new GButton(IDCANCEL, 100, 30, 80, 20, "Cancel"));
/// 	}
///
///		~Example()
///		{
///			DeleteArray(Str);
///		}
/// 		
/// 	int OnNotify(GViewI *c, int Flags)
/// 	{
/// 		switch (c->GetId())
/// 		{
/// 			case IDOK:
///				{
///					Str = NewStr(GetCtrlName(IDC_STRING));
///					// fall thru
///				}
///				case IDCANCEL:
/// 			{
/// 				EndModal(c->GetId());
/// 				break;
/// 			}
/// 		}
/// 
/// 		return 0;
/// 	}
/// };
/// \endcode
///
/// This example shows how to insert child widgets into the window and then process clicks on the buttons
/// and finally return data to the caller via a public member variable. It is desirable to pass data using
/// this method because a dialog could be running in it's own thread on some systems and therefor should not
/// be accessing data structures outside of itself directly without locking them. If your main application'
/// doesn't have thread locking in place for it's main data structures then accessing them from the dialog
/// wouldn't be thread safe. This is mainly the case with the BeOS port of Lgi, but is good practise for
/// the Windows and Linux ports for cross platform compatibility.
///
/// The resource file method of creating dialogs is arguably the easier route to take once your've got it
/// set up. Firstly create a resource file using LgiRes with the same name as your programs executable, but
/// a different extension ("lr8"). When you call the GLgiRes::LoadFromResource function Lgi will automatically
/// look for a file named after the running executable with the right extension and load it. Then it will
/// find the dialog resource and instantiate all the controls specified in the resource. All the built in
/// Lgi controls are supported directly as tags in the XML file and you can create your own custom controls
/// that the resource loader can instantiate as well using the GViewFactory system.
///
/// Resource file example:
/// \code
/// #include "Resdefs.h" // For the dialog defines
///
/// class Example : public GDialog
/// {
/// public:
///		char *Str;
///
/// 	Example(GView *p)
/// 	{
///			Str = 0;
/// 		SetParent(p);
///			if (LoadFromResource(IDD_EXAMPLE))
///			{
/// 			MoveToCenter();
/// 		}
/// 	}
///
///		~Example()
///		{
///			DeleteArray(Str);
///		}
/// 		
/// 	int OnNotify(GViewI *c, int Flags)
/// 	{
/// 		switch (c->GetId())
/// 		{
/// 			case IDOK:
///				{
///					Str = NewStr(GetCtrlName(1));
///					// fall thru
///				}
///				case IDCANCEL:
/// 			{
/// 				EndModal(c->GetId());
/// 				break;
/// 			}
/// 		}
/// 
/// 		return 0;
/// 	}
/// };
/// \endcode
///
/// This assumes you have created an lr8 file with the resource named 'IDD_EXAMPLE' in it.
///
/// Now to actually call either of these dialogs you would use code like this:
/// \code
/// Example Dlg(MyWindow);
/// if (Dlg.DoModal() == IDOK)
/// {
/// 	// Do something with Dlg.Str
/// }
/// \endcode
///
/// The built in controls that you can use are:
/// <ul>
///		<li> GButton (Push button)
///		<li> GEdit (Editbox for text entry)
///		<li> GText (Static label)
///		<li> GCheckBox (Independant boolean)
///		<li> GCombo (Select one from many)
///		<li> GSlider (Select a position)
///		<li> GBitmap (Display an image)
///		<li> GProgress (Show a progress)
///		<li> GList (List containing GListItem)
///		<li> GTree (Heirarchy of GTreeItem)
///		<li> GRadioGroup (One of many selection using GRadioButton)
///		<li> GTabView (Containing GTabPage)
/// </ul>
class LgiClass GDialog :
	public GWindow,
	public GLgiRes,
	public ResObject
{
	friend class GControl;

private:
    struct GDialogPriv *d;

protected:
	/// Load the dialog from a resource
	bool LoadFromResource
	(
		/// The resource ID
		int Resource,
		/// [Optional] tag list to exclude/include various controls via tag
		char *TagList = 0
	);

public:
	/// Constructor
	GDialog();
	
	/// Destructor
	~GDialog();

	/// \brief Run the dialog in modal mode.
	///
	/// The user has to dismiss the dialog to continue.
	virtual int DoModal
	(
		/// Optional override parent window handle
		OsView ParentHnd = 0
	);
	
	/// \brief Run the dialog in modeless mode
	///
	/// It will behave like any other top level window. The user doesn't
	/// have to dismiss the window to continue, it can just move to the back.
	virtual int DoModeless();
	
	/// End a modal window. Typically calling in the OnNotify event of the GDialog.
	virtual void EndModal(int Code = 0);

	/// End a modeless window. Typically calling in the OnNotify event of the GDialog.
	virtual void EndModeless(int Code = 0);

	GMessage::Result OnEvent(GMessage *Msg);
	bool OnRequestClose(bool OsClose);
	void OnPosChange();
    void Pour() {}
	void Quit(bool DontDelete = false);
	
	#if defined(MAC)
	void OnPaint(GSurface *pDC);
	#elif defined(__GTK_H__)
    bool IsResizeable();
    void IsResizeable(bool r);
	#endif
};

/// Implementation class
class LgiClass GControl :
	public GView
{
	friend class GDialog;

protected:
	#if defined BEOS
	bigtime_t Sys_LastClick;
	void MouseClickEvent(bool Down);
	#elif WIN32NATIVE
	GWin32Class *SubClass;
	#endif

	GdcPt2 SizeOfStr(const char *Str);

public:
	#if WIN32NATIVE

	GControl(char *SubClassName = 0);

	#else

	GControl(OsView view = 0);

	#endif

	~GControl();

	GMessage::Result OnEvent(GMessage *Msg);
};

//////////////////////////////////////////////////////////////////////////////////////
// Controls
#if defined WIN32
#define LGI_BUTTON			"LGI_Button"
#define LGI_EDITBOX			"LGI_Editbox"
#define LGI_CHECKBOX		"LGI_CheckBox"
#define LGI_TEXT			"LGI_Text"
#define LGI_RADIOGROUP		"LGI_RadioGroup"
#define LGI_TABCONTROL		"LGI_TabControl"
#define LGI_TABPAGE			"LGI_TabPage"
#define LGI_SLIDER			"LGI_Slider"
#define LGI_COMBO			"LGI_Combo"
#define LGI_BITMAP			"LGI_Bitmap"
#define LGI_LIST			"LGI_ListView"
#define LGI_TREE			"LGI_TreeView"
#define LGI_PROGRESS		"LGI_Progress"
#define LGI_SCROLLBAR		"LGI_ScrollBar"
#endif

///////////////////////////////////////////////////////////////////////////////////////////
// Mac specific
#ifdef MAC

struct GLabelData
{
	GView *Ctrl;
	GSurface *pDC;
	GRect r;
	int Justification;
	
	GLabelData()
	{
		Justification = 0;
		r.ZOff(0, 0);
		pDC = 0;
		Ctrl = 0;
	}
};

#ifndef COCOA
void LgiLabelProc(	const Rect *r,
					ThemeButtonKind kind,
					const ThemeButtonDrawInfo *info,
					UInt32 UserData,
					SInt16 depth,
					Boolean ColourDev);

extern ThemeButtonDrawUPP LgiLabelUPP;
#endif

#endif

#endif

