/** \file
	\author Matthew Allen
 */

#ifndef __GMENU_H
#define __GMENU_H

// Os specific declarations
#if defined __GTK_H__
	#include <GMenuImpl.h>
	typedef MenuClickImpl *OsSubMenu;
	typedef MenuItemImpl *OsMenuItem;
#elif defined WIN32
	typedef HMENU OsSubMenu;
	typedef MENUITEMINFO OsMenuItem;

	#ifndef COLOR_MENUHILIGHT
	#define COLOR_MENUHILIGHT	29
	#endif
	#ifndef COLOR_MENUBAR
	#define COLOR_MENUBAR		30
	#endif
#elif defined BEOS
	typedef BMenu *OsSubMenu;
	typedef	BMenuItem *OsMenuItem;
#elif defined ATHEOS
	#include <gui/menu.h>
	typedef os::Menu *OsSubMenu;
	typedef os::MenuItem *OsMenuItem;
#elif defined MAC
	typedef MenuRef OsSubMenu;
	typedef MenuItemIndex OsMenuItem;
#else
	#error "Not impl."
#endif

#include "GXmlTree.h"
#include "Res.h"

///////////////////////////////////////////////////////////////////////////////////////////////
// Menu wrappers
class LgiClass GMenuLoader
{
	friend class GMenuItem;
	friend class GMenu;
	friend class GSubMenu;
	friend class MenuImpl;
	friend class SubMenuImplPrivate;

protected:
#ifdef WIN32
	OsSubMenu Info;
#endif
	List<GMenuItem> Items;

public:
	GMenuLoader()
	{
		#ifdef WIN32
		Info = 0;
		#endif
	}

	bool Load(	class LgiMenuRes *MenuRes,
				GXmlTag *Tag,
				ResFileFormat Format,
				class TagHash *TagList);

	virtual GMenuItem *AppendItem(char *Str, int Id, bool Enabled, int Where = -1, char *Shortcut = 0) = 0;
	virtual GSubMenu *AppendSub(char *Str, int Where = -1) = 0;
	virtual GMenuItem *AppendSeparator(int Where = -1) = 0;
};

/// Sub menu.
class LgiClass GSubMenu :
	public GBase,
	public GTarget,
	// public GFlags,
	public GMenuLoader,
	public GItemContainer
{
	friend class GMenuItem;
	friend class GMenu;
	friend class SubMenuImpl;
	friend class MenuItemImpl;
	friend class MenuImpl;

	#if !WIN32NATIVE
	OsSubMenu Info;
	#endif
	
	#if WIN32NATIVE

	HWND TrackHandle;
	// bool Load(HMENU hmenu);

	#elif defined BEOS

	void _CopyMenu(BMenu *To, GSubMenu *From);
	
	#else
	
	bool OnKey(GKey &k);

	#endif

protected:
	/// The parent menu item or NULL if the root menu
	GMenuItem		*Parent;
	/// The top level window this sub menu belongs to or NULL
	GMenu			*Menu;
	/// The window that the menu belongs to or NULL.
	GViewI			*Window;
	
	void OnAttach(bool Attach);

public:
	/// Constructor
	GSubMenu
	(
		/// Name of the menu
		char *name = (char*)"",
		/// True if it's popup
		bool Popup = true
	);
	virtual ~GSubMenu();

	/// Returns the OS handle
	OsSubMenu Handle() { return Info; }
	
	/// Add a new item
	GMenuItem *AppendItem
	(
		/// The text of the item.
		///
		/// If you put a tab control in the text, anything after the tab is considered
		/// to be the keyboard shortcut for the menu item. The shortcut can be a combination
		/// of keys added together with '+'.
		///
		/// e.g.
		/// <ul>
		///		<li> Ctrl+S
		///		<li> Alt+Del
		///		<li> F2
		/// </ul>
		char *Str,
		/// Command ID to post to the OnCommand() handler
		int Id,
		/// True if the item should be enabled
		bool Enabled = true,
		/// The index into the list to insert at, or -1 to insert at the end
		int Where = -1,
		// Shortcut if not embeded in "Str"
		char *Shortcut = 0
	);
	
	/// Add a submenu
	GSubMenu *AppendSub
	(
		/// The text of the item
		char *Str,
		/// The index to insert the item, or -1 to insert on the end
		int Where = -1
	);
	
	/// Add a separator
	GMenuItem *AppendSeparator(int Where = -1);
	
	/// Delete all items
	void Empty();
	
	/// Detachs an item from the sub menu but doesn't delete it
	bool RemoveItem
	(
		/// The index of the item to remove
		int i
	);

	/// Detachs an item from the sub menu but doesn't delete it
	bool RemoveItem
	(
		/// Pointer of the item to remove
		GMenuItem *Item
	);
	
	/// Returns numbers of items in menu
	int Length();

	/// Return a pointer to an item
	GMenuItem *ItemAt
	(
		/// The index of the item to return
		int i
	);
	
	/// Returns a pointer to an item
	GMenuItem *FindItem
	(
		/// The ID of the item to return
		int Id
	);

	/// Returns a pointer to an sub menu
	GSubMenu *FindSubMenu
	(
		/// The ID of the sub menu to return
		int Id
	);

	/// Floats the submenu anywhere on the screen
	int Float
	(
		/// The parent view
		GView *Parent,
		/// The x coord of the top-left corner
		int x,
		/// The y coord of the top-left corner
		int y,
		/// True if the menu is tracking the left button, else it tracks the right button
		bool Left = false
	);
	
	/// Returns the parent menu item
	GMenuItem *GetParent() { return Parent; }
	
	/// Returns the menu that this belongs to
	GMenu *GetMenu() { return Menu; }
};

/// An item an a menu
class LgiClass GMenuItem :
	public GBase,
	public GTarget
	// public GFlags
{
	friend class GSubMenu;
	friend class GMenu;
	friend class GView;
	friend class LgiMenuItem;
	friend class SubMenuImpl;
	friend class MenuItemImpl;
	friend class MenuImpl;
	friend class SubMenuImplPrivate;

private:
	#ifdef WIN32
	bool			Insert(int Pos);
	bool			Update();
	#endif

protected:
	GMenu			*Menu;
	GSubMenu		*Parent;
	GSubMenu		*Child;
	int				Position;
	int				_Icon;

	OsMenuItem		Info;
	class GMenuItemPrivate *d;

	#if defined BEOS
	BMessage		*Msg;
	#else
	int				_Id;
	bool			_Check;
	bool			_Enabled;	
	#endif

	virtual void _Measure(GdcPt2 &Size);
	virtual void _Paint(GSurface *pDC, int Flags);
	virtual void _PaintText(GSurface *pDC, int x, int y, int Width);

	void OnAttach(bool Attach);

public:
	GMenuItem();
	#if defined BEOS
	GMenuItem(BMenuItem *item);
	GMenuItem(GSubMenu *p);
	#else
	GMenuItem(GMenu *m, GSubMenu *p, int Pos, char *Shortcut = 0);
	#endif
	virtual ~GMenuItem();

	/// Creates a sub menu off the item
	GSubMenu *Create();
	/// Removes the item from it's place in the menu but doesn't delete it
	bool Remove();
	/// Returns the parent sub menu
	GSubMenu *GetParent();
	/// Scans the text of the item for a keyboard shortcut
	bool ScanForAccel();
	/// Returns the OS handle for the menuitem
	OsMenuItem Handle() { return Info; }

	/// Set the id
	void Id(int i);
	/// Turn the item into a separator
	void Separator(bool s);
	/// Put a check mark on the item
	void Checked(bool c);
	/// \brief Set the text of the item
	/// \sa GSubMenu::AppendItem()
	bool Name(char *n);
	/// Enable or disable the item
	void Enabled(bool e);
	void Visible(bool v);
	void Focus(bool f);
	/// Attach a sub menu to the item
	void Sub(GSubMenu *s);
	/// Set the icon for the item. The icon is stored in the GMenu's image list.
	void Icon(int i);

	/// Get the id
	int Id();
	/// Get the text of the item
	char *Name();
	/// Return whether this item is a separator
	bool Separator();
	/// Return whether this item has a check mark
	bool Checked();
	/// Return whether this item is enabled
	bool Enabled();
	bool Visible();
	bool Focus();
	/// Return whether this item's submenu
	GSubMenu *Sub();
	/// Return the icon of this this
	int Icon();
};

/// Encapsulates a keyboard shortcut
class LgiClass GAccelerator
{
	int Flags;
	int Key;
	int Id;

public:
	GAccelerator(int flags, int key, int id);
	
	int GetId() { return Id; }

	/// See if the accelerator matchs a keyboard event
	bool Match(GKey &k);
};

/** \brief Top level window menu

This class contains GMenuItem's and GSubMenu's.

A basic menu can be constructed inside a GWindow like this:
\code
Menu = new GMenu;
if (Menu)
{
	Menu->Attach(this);

	GSubMenu *File = Menu->AppendSub("&File");
	if (File)
	{
		File->AppendItem("&Open\tCtrl+O", IDM_OPEN, true);
		File->AppendItem("&Save All\tCtrl+S", IDM_SAVE_ALL, true);
		File->AppendItem("Save &As", IDM_SAVEAS, true);
		File->AppendSeparator();
		File->AppendItem("&Options", IDM_OPTIONS, true);
		File->AppendSeparator();
		File->AppendItem("E&xit", IDM_EXIT, true);
	}

	GSubMenu *Help = Menu->AppendSub("&Help");
	if (Help)
	{
		Help->AppendItem("&Help", IDM_HELP, true);
		Help->AppendItem("&About", IDM_ABOUT, true);
	}
}
\endcode

Or you can load a menu from a resource like this:
\code
Menu = new GMenu;
if (Menu)
{
	Menu->Attach(this);
	Menu->Load(this, "IDM_MENU");
}
\endcode
*/

class LgiClass GMenu :
	public GSubMenu
{
	friend class GSubMenu;
	friend class GMenuItem;
	friend class GWindow;

	static GFont *_Font;
	class GMenuPrivate *d;

	#if defined WIN32
	void OnChange();
	#else
	void OnChange() {}
	#endif

protected:
	/// List of keyboard shortcuts in the menu items attached
	List<GAccelerator> Accel;

public:
	/// Constructor
	GMenu();
	
	/// Destructor
	virtual ~GMenu();

	/// Returns the font used by the menu items
	static GFont *GetFont();

	/// Returns the top level window that this menu is attached to
	GViewI *WindowHandle() { return Window; }
	
	/// Attach the menu to a window
	bool Attach(GViewI *p);
	
	/// Detact the menu from the window
	bool Detach();
	
	/// Load the menu from a resource file
	bool Load
	(
		/// The parent view for any error message boxes
		GView *p,
		/// The resource to load. Will probably change to an int sometime.
		char *Res,
		/// Optional list of comma or space separated tags
		char *Tags = 0
	);
	
	/// \brief See if any of the accelerators match the key event
	/// \return true if none of the accelerators match
	bool OnKey
	(
		/// The view that will eventually receive the key event
		GView *v,
		/// The keyboard event details
		GKey &k
	);

	#if defined(WIN32)
	static int _OnEvent(GMessage *Msg);
	#elif defined(BEOS)
	GRect GetPos();
	#endif
};

#endif




