/// \file
/// \author Matthew Allen (fret@memecode.com)
/// \brief Extension to the GWindow class to create a document processing application.
#ifndef __GDOCAPP_H
#define __GDOCAPP_H

#include <stdio.h>

#include "Lgi.h"
#include "GXml.h"
#include "GMru.h"

////////////////////////////////////////////////////////////////////////////////////////////
// Defines

// window messages
#define		IDM_SAVE			15002
#define		IDM_CLOSE			15003
#define		IDM_EXIT			15004

// Misc
// #define		MainWnd				((AppWnd*)GApp::ObjInstance()->AppWnd)

/// A base for a document processing application.
/// The 2 types you can use for OptionsFmt are (ideally) GOptionsFile or (deprecated) ObjProperties.
template<typename OptionsFmt>
class GDocApp :
	public GWindow,
	public GMru
{
private:
	class GDocAppPrivate *d;
	OptionsFmt		*Options;

	void			_Close();

protected:
	/// Loads the given menu.
	bool			_LoadMenu(char *Resource = 0, char *Tags = 0);
	/// Call this to create the window, usually in the constructor of your main window class.
	bool			_Create();
	/// Call this to destroy the window, usually in the destuctor of your main window class.
	bool			_Destroy();
	/// Use the open file dialog to select a docuement to open.
	bool			_OpenFile(char *File, bool ReadOnly);
	/// Save the current document.
	bool			_SaveFile(char *File);

	bool			_DoSerialize(bool Write);
	bool			_SerializeFile(bool Write);

	// Data
	GSubMenu		*_FileMenu;

	/// Set this to the name of the language setting in the option file.
	char			*_LangOptsName;

public:
	/// Creates the class.
	GDocApp
	(
		/// The name of the application
		char *appname = 0,
		/// [Optional] The icon resource for the window.
		char *icon = 0,
		/// Options file base name..
		char *optsname = 0

	);
	~GDocApp();

	/// Sets the current file name.
	void SetCurFile(char *f);
	/// Gets the current file name.
	char *GetCurFile();
	/// Sets the dirty state. If the doc is clean you are not asked to save it when you close the window.
	bool SetDirty(bool Dirty);
	/// Gets the dirty state.
	bool GetDirty();
	/// Gets the options list.
	OptionsFmt *GetOptions();
	/// Gets the application name.
	char *GetAppName();
	/// Gets the options file name
	char *GetOptionsFileName();

	/// Implement this to clear your docment from memory, i.e. "Close".
	virtual void Empty() {}
	/// Implement this to read/write your application specific options.
	virtual bool SerializeOptions(OptionsFmt *Options, bool Write) { return false; }
	/// This is called when the dirty state changes.
	virtual void OnDirty(bool NewValue) {}

	/// Set the language for the application, which causes a restart. The lang id is saved
	/// to the options file, the user is asked whether they want to restart, if yes, then
	/// the application closes, and executes itself.
	///
	/// \sa Requires _LangOptsName to be set to the option name of the language setting.
	/// \sa Uses the resource string L_DOCAPP_RESTART_APP to warn the user of the restart (there is an english default).
	bool SetLanguage(char *LangId);

	// Impl
	void OnReceiveFiles(GArray<char*> &Files);
	bool OnRequestClose(bool OsShuttingDown);
	int OnCommand(int Cmd, int Event, OsView Window);
	int OnEvent(GMessage *m);
};

#endif
