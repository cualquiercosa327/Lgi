/// \file
/// \author Matthew Allen
#ifndef __DRAG_AND_DROP
#define __DRAG_AND_DROP

#include "GVariant.h"

#define DEBUG_DND	0

#if WINNATIVE

#include <shlobj.h>
// #include "GCom.h"

#else

#ifndef S_OK
#define S_OK								0
#endif

#ifndef DROPEFFECT_NONE
#define DROPEFFECT_NONE						0x0
#endif

#ifndef DROPEFFECT_COPY
#define DROPEFFECT_COPY						0x1
#endif

#ifndef DROPEFFECT_MOVE
#define DROPEFFECT_MOVE						0x2
#endif

#ifndef DROPEFFECT_LINK
#define DROPEFFECT_LINK						0x4
#endif

#endif

#ifdef __GTK_H__
LgiFunc int GtkGetDndType(const char *Format);
LgiFunc const char *GtkGetDndFormat(int Type);
LgiFunc Gtk::GdkDragAction DropEffectToAction(int DropEffect);
#endif

//////////////////////////////////////////////////////////////////////////////
struct LgiClass GDragData
{
	GString Format;
	GArray<GVariant> Data;
	
	bool IsFormat(GString Fmt)
	{
		return	Format.Get() != NULL &&
				Fmt != NULL &&
				!_stricmp(Format, Fmt);
	}
	
	bool IsFileDrop()
	{
		return IsFormat(LGI_FileDropFormat);
	}
};

/// A drag source class
class LgiClass GDragDropSource
#if WINNATIVE
	: public IDropSource, public IEnumFORMATETC
#endif
{
	friend class GDataObject;

protected:
	class GDndSourcePriv *d;
	
	#if WINNATIVE
	int Index;

	// IUnknown
	ULONG STDMETHODCALLTYPE AddRef();
	ULONG STDMETHODCALLTYPE Release();
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **ppv);

	// IEnumFORMATETC
	HRESULT STDMETHODCALLTYPE Next(ULONG celt, FORMATETC *rgelt, ULONG *pceltFetched);
	HRESULT STDMETHODCALLTYPE Skip(ULONG celt);
	HRESULT STDMETHODCALLTYPE Reset();
	HRESULT STDMETHODCALLTYPE Clone(IEnumFORMATETC **ppenum);

	// IDropSource
	HRESULT STDMETHODCALLTYPE QueryContinueDrag(BOOL fEscapePressed, DWORD InputState);
	HRESULT STDMETHODCALLTYPE GiveFeedback(DWORD dwEffect);
	#endif

public:
	GDragDropSource();
	~GDragDropSource();

	/// Sets the icon used to show what is being dragged.
	bool SetIcon
	(
		/// The image to show. The caller should retain the object in memory
		/// until after the drag.
		GSurface *Img,
		/// If only a portion of the image is needed set this to the part to
		/// use. Otherwise NULL means use the whole image.
		GRect *SubRgn = NULL
	);

	/// Start a drag operation
	/// \returns The operation that took effect: #DROPEFFECT_NONE, #DROPEFFECT_COPY etc. 
	int Drag(GView *SourceWnd, int Effect);

	/// Called when window is registered
	virtual void OnRegister(bool Suc) {}
	/// Called on start of drag op
	virtual void OnStartData() {}
	/// Called on end of drag op
	virtual void OnEndData();
	
	/// This is called when you are being asked for your
	/// data. You must provide it in the format specified
	/// and return it via the GVariant.
	virtual bool GetData
	(
		/// Fill out as many GDragData structures as you need.
		GArray<GDragData> &Data
	);

	/// [Deprecated] This is the old API for compatibility.
	/// The new GDragData version by default will call this
	/// method.
	DEPRECATED_PRE
	virtual bool GetData
	(
		/// [out] the data retreived
		GVariant *Data,
		/// [in] the format to get
		char *Format
	)
	{
		LgiAssert(!"Impl the proper handler.");
		return false;
	}
	DEPRECATED_POST

	/// This is called to see what formats your support
	/// Insert into the list dynamically allocated strings
	/// that describe the formats you can supply data in.
	/// One of these will be chosen by the target and passed
	/// to 'GetData'
	///
	/// You can define your own formats by making up a string
	/// or you can use OS defined ones to interact with other
	/// apps in the system. e.g. Win32 defines "CF_HDROP" for file
	/// d'n'd. You can use 'FormatToInt' and 'FormatToStr' to
	/// convert to and from the OS's integer id's. This works
	/// the same on Win32 as Linux. But the system formats will
	/// obviously be different.
	///
	/// The default file drop format for each OS is defined as
	/// #LGI_FileDropFormat.
	virtual bool GetFormats
	(
		/// List of format you can provide. You should keep the format
		/// length to 4 bytes on the Mac.
		List<char> &Formats
	)
	{ return false; }

	/// Creates a file drop
	bool CreateFileDrop(GDragData *OutputData, GMouse &m, List<char> &Files);
};

/// A drag target class
class LgiClass GDragDropTarget
#if WINNATIVE
	: public IDropTarget
#endif
{
private:
	GView *To;
	List<char> Formats;

	#ifdef WIN32
	LONG Refs;
	#endif

	#if WINNATIVE
	// IUnknown
	ULONG STDMETHODCALLTYPE AddRef();
	ULONG STDMETHODCALLTYPE Release();
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **ppv);

	// IDropTarget
	HRESULT STDMETHODCALLTYPE DragEnter(IDataObject *pDataObject, DWORD grfKeyState, POINTL pt, DWORD *pdwEffect);
	HRESULT STDMETHODCALLTYPE DragOver(DWORD grfKeyState, POINTL pt, DWORD *pdwEffect);
	HRESULT STDMETHODCALLTYPE DragLeave(void);
	HRESULT STDMETHODCALLTYPE Drop(IDataObject *pDataObject, DWORD grfKeyState, POINTL pt, DWORD *pdwEffect);

	#endif

protected:
	#if WINNATIVE
	// Data elements
	IDataObject *DataObject;

	// Tools
	bool OnDropFileGroupDescriptor(FILEGROUPDESCRIPTOR *Data, GArray<char*> &Files);
	#endif

public:
	GDragDropTarget();
	~GDragDropTarget();

	/// call this when you have a operating system view handle (e.g. HWND/Window/HIViewRef)
	void SetWindow(GView *To);

	// Override these

	/// Initialize event
	virtual void OnDragInit(bool Suc) {}
	/// Drag entered this target event
	virtual void OnDragEnter() {}
	/// Drag exited this target event
	virtual void OnDragExit() {}

	/// 'WillAccept' is called to see whether this target
	/// can cope with any of the data types being passed to it.
	/// Once you have decided what format you want the data in
	/// clear all the other formats from the list. The first
	/// format left in the list will be passed to the 'OnDrop'
	/// function.
	/// \returns #DROPEFFECT_NONE for failure or #DROPEFFECT_COPY, #DROPEFFECT_MOVE, #DROPEFFECT_LINK
	virtual int WillAccept
	(
		/// The list of formats the source provides, delete any you can't handle
		List<char> &Formats,
		/// The mouse pointer in view space co-ords
		GdcPt2 Pt,
		/// The current keyboard mobifiers
		/// \sa #LGI_EF_CTRL, #LGI_EF_ALT, #LGI_EF_SHIFT
		int KeyState
	) = 0;

	/// 'OnDrop' is called when the user releases the data over
	/// your window. The data is going to be a binary GVariant
	/// in the format you accepted earlier.
	/// \returns #DROPEFFECT_NONE for failure or #DROPEFFECT_COPY, #DROPEFFECT_MOVE, #DROPEFFECT_LINK
	virtual int OnDrop
	(
		/// All the available data formats for this drop
		GArray<GDragData> &Data,
		/// The mouse coords
		GdcPt2 Pt,
		/// The keyboard modifiers
		/// \sa #LGI_EF_CTRL, #LGI_EF_ALT, #LGI_EF_SHIFT
		int KeyState
	) = 0;

	/// [Deprecated] Old version of the drop handler.
	DEPRECATED_PRE
	virtual int OnDrop
	(
		/// The selected format
		char *Format,
		/// The data for the drop
		GVariant *Data,
		/// The mouse coords
		GdcPt2 Pt,
		/// The keyboard modifiers
		/// \sa #LGI_EF_CTRL, #LGI_EF_ALT, #LGI_EF_SHIFT
		int KeyState
	) DEPRECATED_POST
	{
		LgiAssert(!"Impl the proper handler.");
		return DROPEFFECT_NONE;
	}

	#ifdef MAC
		#if defined COCOA
		#else
		OSStatus OnDragWithin(GView *v, DragRef Drag);
		OSStatus OnDragReceive(GView *v, DragRef Drag);
		#endif
	#endif
};

#endif
