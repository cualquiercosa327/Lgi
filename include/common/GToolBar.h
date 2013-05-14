/// \file
///	\author Matthew Allen
/// \brief Toolbar and Imagelist classes.

#ifndef __GTOOLBAR_H
#define __GTOOLBAR_H

////////////////////////////////////////////////////////////////////////

/// GImageList::Draw() flag: Draw as selected
#define IMGLST_SELECTED				0x0001
/// GImageList::Draw() flag: Draw disabled
#define IMGLST_DISABLED				0x0002
/// GImageList::Draw() flag: Draw with GDC
#define IMGLST_GDC					0x0004

/// \brief A list of images/icons all the same size
///
/// Currently the image list treats the pixel at (0,0) as the transparent colour key.
class LgiClass GImageList : public GMemDC
{
	class GImageListPriv *d;

public:
	/// Create the image list
	GImageList
	(
		/// The width of each tile
		int TileX,
		/// The height of each tile
		int TileY,
		/// Initial data for the images
		GSurface *pDC = NULL
	);
	
	~GImageList();

	/// Returns the width of each image
	int TileX();
	/// Returns the height of each image
	int TileY();
	/// Gets the number of images in the list
	int GetItems();
	/// Finds the bounds of valid data for each image and returns it in an array
	/// valid from 0 to GetItems()-1.
	GRect *GetBounds();

	/// Notifies the image list that it's image data has changed and it should flush any
	/// cached info about the images
	void Update(int Flags);
	/// Draw an image onto a graphics surface
	void Draw
	(
		/// The output surface
		GSurface *pDest,
		/// The x coord to draw the top-left corner
		int x,
		/// The y coord to draw the top-left corner
		int y,
		/// A 0 based index into the list to draw
		int Image,
		/// Drawing options
		/// \sa The defines starting at IMGLST_SELECTED in GToolBar.h
		int Flags = 0
	);
};

/** \brief Loads an image list from a file

	This requires that somewhere in your project you have the image codec to load the type of 
	file you pass in. For instance if the file is a png, then include the Png.cpp file in your
	project. Uses LoadDC() internally.
*/
LgiFunc GImageList *LgiLoadImageList
(
	/// The file to load
	const char *File,
	/// The width of the icons
	int x,
	/// The height of the icons
	int y
);

///////////////////////////////////////////////////////////////////////////////////////////////
// Toolbar stuff

// Tool button types

/// Push type toolbar button
#define TBT_PUSH				0
/// Radio type toolbar button, uses separators to define the domain
#define TBT_RADIO				1
/// Toggle type toolbar button
#define TBT_TOGGLE				2

// Tool icon types
#define TOOL_ICO_NEXT			-1
#define TOOL_ICO_NONE			-2

// Commands
#define IDM_NONE				0
#define IDM_SEPARATOR			-1
#define IDM_BREAK				-2

// Dimensions
#define BORDER_SHADE			1
#define BORDER_SPACER			1
#define BORDER_SEPARATOR		4
#define BORDER_BUTTON			1

/// Button on a GToolBar
class LgiClass GToolButton :
	public GView
{
	friend class GToolBar;

protected:
	int			Type;		// Button Type
	bool		Clicked;
	bool		Down;
	bool		Over;
	int			ImgIndex;
	int			TipId;
	bool		NeedsRightClick;

	#if defined(MAC) && !defined(COCOA)
	HIToolbarItemRef ItemRef;
	#endif

	GArray<GDisplayString*> Text;
	void Layout();

	virtual	void SetDown(bool d);

public:
	GToolButton(int Bx, int By);
	~GToolButton();

	const char *GetClass() { return "GToolButton"; }

	int64 Value() { return Down; }
	void Value(int64 i);

	char *Name() { return GView::Name(); }
	bool Name(const char *n);

	/// Gets the icon index into the parent GToolBar's image list
	int Image() { return ImgIndex; }
	/// Sets the icon index into the parent GToolBar's image list
	void Image(int i);

	bool GetDown() { return Down; }
	int GetType() { return Type; }
	void SetType(int i) { Type = i; }
	bool Separator() { return GetId() == IDM_SEPARATOR; }
	void Separator(bool i) { SetId(IDM_SEPARATOR); }
	bool GetNeedsRightClick() { return NeedsRightClick; }
	void SetNeedsRightClick(bool b) { NeedsRightClick = b; }

	void OnPaint(GSurface *pDC);

	void OnMouseClick(GMouse &m);
	void OnMouseMove(GMouse &m);
	void OnMouseEnter(GMouse &m);
	void OnMouseExit(GMouse &m);

	virtual void OnCommand();
	virtual bool GetDimension(int &x, int &y) { return false; }
};

/** \brief A top level window toolbar

	A toolbar hosts buttons and separators in a row. The buttons contain
	icons and optionally text describing there function. When the user clicks
	a button a M_COMMAND message is passed up to the owning GWindow for the
	application to handle. In the same fashion as a menu command being clicked.
	You should override GWindow::OnCommand to catch events from a GToolBar.

	This should be attached to the GWindow before other windows so that it
	lays itself out under the menu.

	To initialize and attach a toolbar to your GWindow use something like:
	\code
	GToolBar *t = LgiLoadToolbar(this, "icons.png");
	if (t)
	{
		t->Attach(this);
		t->AppendButton("Open", IDM_OPEN, TBT_PUSH);
		t->AppendSeparator();
		t->AppendButton("Help", IDM_HELP, TBT_PUSH);
	}
	\endcode
*/
class LgiClass GToolBar : public GLayout
{
	friend class GToolButton;

protected:
	class GToolBarPrivate *d;

	// Local
	void _BuildCache(GImageList *From);
	void _DrawFromCache(GSurface *pDC, int x, int y, int Index, bool Hilight, bool Disabled);
	int GetBx();
	int GetBy();

	// Overridable
	virtual void ContextMenu(GMouse &m);	
	virtual int PostDescription(GView *Ctrl, const char *Text);
	
	#if defined(MAC) && !defined(COCOA)
	HIToolbarRef ToolbarRef;
	#endif

public:
	GToolBar();
	~GToolBar();

	const char *GetClass() { return "GToolBar"; }

	/// Called when a button is clicked
	virtual void OnButtonClick(GToolButton *Btn);

	/// True if the toolbar should layout in a vertical manner
	bool IsVertical();
	/// True if the toolbar should layout in a vertical manner
	void IsVertical(bool v);
	/// Shows text labels under the buttons
	bool TextLabels();
	/// Shows text labels under the buttons
	void TextLabels(bool i);
	/// Returns true if the the customizable menu is on [default: off]
	bool IsCustomizable();
	/// Switch for the customization menu.
	void Customizable
	(
		/// Set this to the properties store to switch on the right click menu, or NULL to switch it off.
		GDom *Store = 0,
		/// This is the property that the toolbar stores state under. It should be unique for every toolbar.
		const char *Option = 0
	);
	/// Sets the image list to use via a file
	bool SetBitmap(char *File, int Bx, int By);
	/// Sets the image list to use via a memory surface
	bool SetDC(GSurface *pDC, int Bx, int By);
	/// Gets the image list
	GImageList *GetImageList();
	/// Sets the image list to use
	bool SetImageList(GImageList *l, int Bx, int By, bool Own = true);
	/// Gets the font used to draw the text below the buttons
	GFont *GetFont();

	/// Adds a button to the toolbar
	GToolButton *AppendButton
	(
		/// The buttons help tip
		const char *Tip,
		/// The ID to post to the main window when activated
		int Id,
		/// The type of button
		/// \sa The define TBT_PUSH and below
		int Type = TBT_PUSH,
		/// Whether to enable the button
		int Enabled = true,
		/// The index into the image list to use on the button
		int IconId = -1
	);
	
	/// Appends any old control
	bool AppendControl(GView *Ctrl);
	
	/// Append a separator
	bool AppendSeparator();
	
	/// Append a line break (wraps the buttons onto the next line)
	bool AppendBreak();
	
	/// Empties the toolbar of buttons
	void Empty();

	/// Gets the number of buttons
	int Length() { return Children.Length(); }

	// Events
	GMessage::Result OnEvent(GMessage *Msg);
	void OnPaint(GSurface *pDC);
	void OnMouseClick(GMouse &m);
	void OnMouseEnter(GMouse &m);
	void OnMouseExit(GMouse &m);
	void OnMouseMove(GMouse &m);
	bool Pour(GRegion &r);
	void OnCreate();

	#ifdef MAC
	bool Attach(GViewI *parent);
	
	class Custom : public GView
	{
	public:
		Custom();
		~Custom();
	};
	#endif
};

/// Loads a toolbar from a file
LgiFunc GToolBar *LgiLoadToolbar
(
	/// A parent window for error message boxes
	GViewI *Parent,
	/// The graphics file to load as the image list
	const char *File,
	/// The width of the icons
	int x = 24,
	/// The height of the icons
	int y = 24
);

#endif
