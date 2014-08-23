/*
**	FILE:			GDragAndDrop.cpp
**	AUTHOR:			Matthew Allen
**	DATE:			30/11/98
**	DESCRIPTION:	Drag and drop support
**
**	Copyright (C) 1998-2003, Matthew Allen
**		fret@memecode.com
*/

#include <stdio.h>

#include "Lgi.h"
#include "GDragAndDrop.h"
#ifdef MAC
#include "INet.h"
#endif

// #define DND_DEBUG_TRACE

#ifdef __GTK_H__
static int NextDndType = 600;
static GHashTbl<const char*, int> DndTypes(0, false, NULL, -1);

int GtkGetDndType(const char *Format)
{
	int Type = DndTypes.Find(Format);
	if (Type < 0)
		DndTypes.Add(Format, Type = NextDndType++);
	return Type;
}

const char *GtkGetDndFormat(int Type)
{
	return DndTypes.FindKey(Type);
}
#endif

class GDndSourcePriv
{
public:
	GAutoString CurrentFormat;
	#if defined(MAC) && !defined(COCOA)
	GAutoPtr<CGImg> Img;
	#endif
	
	#ifdef __GTK_H__
	Gtk::GdkDragContext *Ctx;
	
	GDndSourcePriv()
	{
		Ctx = NULL;
	}
	
	~GDndSourcePriv()
	{
		
	}
	#endif
};

/////////////////////////////////////////////////////////////////////////////////////////
#if WINNATIVE
int MapW32FlagsToLgi(int W32Flags)
{
	int f = 0;

	if (W32Flags & MK_CONTROL) f |= LGI_EF_CTRL;
	if (W32Flags & MK_SHIFT) f |= LGI_EF_SHIFT;
	if (W32Flags & MK_ALT) f |= LGI_EF_ALT;
	if (W32Flags & MK_LBUTTON) f |= LGI_EF_LEFT;
	if (W32Flags & MK_MBUTTON) f |= LGI_EF_MIDDLE;
	if (W32Flags & MK_RBUTTON) f |= LGI_EF_RIGHT;

	return f;
}

int FormatToInt(char *s)
{
	if (s && stricmp(s, "CF_HDROP") == 0) return CF_HDROP;
	return RegisterClipboardFormat(s);
}

char *FormatToStr(int f)
{
	static char b[128];
	if (GetClipboardFormatName(f, b, sizeof(b)) > 0) return b;
	if (f == CF_HDROP) return "CF_HDROP";
	return 0;
}

class GDataObject : public GUnknownImpl<IDataObject>
{
	GDragDropSource *Source;

public:
	GDataObject(GDragDropSource *source);
	~GDataObject();

	// IDataObject
	HRESULT STDMETHODCALLTYPE GetData(FORMATETC *pFormatEtc, STGMEDIUM *PMedium);
	HRESULT STDMETHODCALLTYPE QueryGetData(FORMATETC *pFormatEtc);
	HRESULT STDMETHODCALLTYPE EnumFormatEtc(DWORD dwDirection, IEnumFORMATETC **ppFormatEtc);

	// Not impl
	HRESULT STDMETHODCALLTYPE SetData(FORMATETC *pFormatetc, STGMEDIUM *pmedium, BOOL fRelease) { return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE GetDataHere(FORMATETC *pFormatEtc, STGMEDIUM *PMedium) { return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE DAdvise(FORMATETC *pFormatetc, DWORD advf, IAdviseSink *pAdvSink, DWORD *pdwConnection) { return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE DUnadvise(DWORD dwConnection) { return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE EnumDAdvise(IEnumSTATDATA **ppenumAdvise) { return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE GetCanonicalFormatEtc(FORMATETC * pFormatetcIn, FORMATETC * pFormatetcOut) { return E_NOTIMPL; }
};

GDataObject::GDataObject(GDragDropSource *source)
{
	AddInterface(IID_IDataObject, this);
	Source = source;
	Source->OnStartData();
}

GDataObject::~GDataObject()
{
	Source->OnEndData();
}

HRESULT GDataObject::EnumFormatEtc(DWORD dwDirection, IEnumFORMATETC **ppFormatEtc)
{
	*ppFormatEtc = dynamic_cast<IEnumFORMATETC*>(Source);
	return (*ppFormatEtc) ? S_OK : E_OUTOFMEMORY;
}

HRESULT GDataObject::GetData(FORMATETC *pFormatEtc, STGMEDIUM *PMedium)
{
	HRESULT Ret = E_INVALIDARG;

	int CurFormat = 0;
	List<char> Formats;
	Source->d->CurrentFormat.Reset();
	Source->GetFormats(Formats);
	for (char *f=Formats.First(); f; f=Formats.Next())
	{
		int n = FormatToInt(f);
		char *efmt = FormatToStr(pFormatEtc->cfFormat);
		if (n == pFormatEtc->cfFormat)
		{
			CurFormat = n;
			Source->d->CurrentFormat.Reset(NewStr(f));
			break;
		}
	}
	Formats.DeleteArrays();

	#ifdef DND_DEBUG_TRACE
	LgiTrace(	"GetData 0, pFormatEtc{'%s',%p,%i,%i,%i} PMedium{%i} CurrentFormat=%s",
				FormatToStr(pFormatEtc->cfFormat),
				pFormatEtc->ptd,
				pFormatEtc->dwAspect,
				pFormatEtc->lindex,
				pFormatEtc->tymed,
				PMedium->tymed,
				Source->CurrentFormat);
	#endif

	if (pFormatEtc &&
		PMedium &&
		Source->d->CurrentFormat)
	{
		GVariant Data;

		// the users don't HAVE to use this...
		Source->GetData(&Data, Source->d->CurrentFormat);

		ZeroObj(*PMedium);
		PMedium->tymed = TYMED_HGLOBAL;

		uchar *Ptr = 0;
		int Size = 0;
		switch (Data.Type)
		{
			case GV_NULL:
				break;
			case GV_BINARY:
			{
				Ptr = (uchar*)Data.Value.Binary.Data;
				Size = Data.Value.Binary.Length;
				break;
			}
			case GV_STRING:
			{
				Ptr = (uchar*)Data.Value.String;
				Size = Ptr ? strlen((char*)Ptr) + 1 : 0;
				break;
			}
			default:
			{
				// Unsupported format...
				LgiAssert(0);
				break;
			}
		}

		if (Ptr && Size > 0)
		{
			PMedium->hGlobal = GlobalAlloc(GHND, Size);
			if (PMedium->hGlobal)
			{
				void *g = GlobalLock(PMedium->hGlobal);
				if (g)
				{
					memcpy(g, Ptr, Size);
					GlobalUnlock(PMedium->hGlobal);
					Ret = S_OK;
				}
			}
		}

		#ifdef DND_DEBUG_TRACE
		LgiTrace("GetData 2, Ret=%i", Ret);
		#endif
	}

	return Ret;
}

HRESULT GDataObject::QueryGetData(FORMATETC *pFormatEtc)
{
	HRESULT Ret = E_INVALIDARG;

	if (pFormatEtc)
	{
		Ret = S_OK;

		List<char> Formats;
		Source->GetFormats(Formats);
		bool HaveFormat = false;
		for (char *i=Formats.First(); i; i=Formats.Next())
		{
			if (pFormatEtc->cfFormat == FormatToInt(i))
			{
				HaveFormat = true;
			}
		}
		Formats.DeleteArrays();

		if (!HaveFormat)
		{
			#ifdef DND_DEBUG_TRACE
			char *TheirFormat = FormatToStr(pFormatEtc->cfFormat);
			LgiTrace("QueryGetData didn't have '%s' format.", TheirFormat);
			#endif
			Ret = DV_E_FORMATETC;
		}
		else if (pFormatEtc->tymed != TYMED_HGLOBAL)
		{
			#ifdef DND_DEBUG_TRACE
			LgiTrace("QueryGetData no TYMED!");
			#endif
			Ret = DV_E_TYMED;
		}
	}
	
	return Ret;
}
#endif

/////////////////////////////////////////////////////////////////////////////////////////
GDragDropSource::GDragDropSource()
{
	d = new GDndSourcePriv;
	#if WINNATIVE
	Index = 0;
	#endif

	OnRegister(true);
}

GDragDropSource::~GDragDropSource()
{
	DeleteObj(d);
}

bool GDragDropSource::CreateFileDrop(GVariant *OutputData, GMouse &m, List<char> &Files)
{
	if (OutputData && Files.First())
	{
		#if WINNATIVE
		
		int Size = sizeof(DROPFILES) + ((IsWin9x) ? sizeof(char) : sizeof(char16));

		List<char> Native;
		List<char16> NativeW;
		for (char *File=Files.First(); File; File=Files.Next())
		{
			if (IsWin9x)
			{
				char *f = LgiToNativeCp(File);
				if (f)
				{
					Size += strlen(f) + 1;
					Native.Insert(f);
				}
			}
			else
			{
				char16 *f = LgiNewUtf8To16(File);
				if (f)
				{
					int Len = StrlenW(f) + 1;
					Size += Len * sizeof(char16);
					NativeW.Insert(f);
				}
			}
		}

		DROPFILES *Dp = (DROPFILES*) new char[Size];
		if (Dp)
		{
			Dp->pFiles = sizeof(DROPFILES);
			Dp->fWide = !IsWin9x;
			Dp->fNC = true;
			Dp->pt.x = m.x;
			Dp->pt.y = m.y;

			if (IsWin9x)
			{
				char *f = ((char*)Dp) + Dp->pFiles;
				for (char *File=Native.First(); File; File=Native.Next())
				{
					strcpy(f, File);
					f += strlen(File) + 1;
				}
				*f++ = 0;
			}
			else
			{
				char16 *f = (char16*) (((char*)Dp) + Dp->pFiles);
				for (char16 *File=NativeW.First(); File; File=NativeW.Next())
				{
					int Len = StrlenW(File) + 1;
					StrcpyW(f, File);
					f += Len;
				}
				*f++ = 0;
			}

			OutputData->SetBinary(Size, (uchar*)Dp);
			DeleteArray((char*&)Dp);
		}

		Native.DeleteArrays();
		NativeW.DeleteArrays();

		return Dp != 0;
		
		#elif defined LINUX
		
		GStringPipe p;
		for (char *f=Files.First(); f; f=Files.Next())
		{
			char s[256];
			sprintf_s(s, sizeof(s), "file:%s", f);
			if (p.GetSize()) p.Push("\n");
			p.Push(s);
		}

		char *s = p.NewStr();
		if (s)
		{
			OutputData->SetBinary(strlen(s), s);
			DeleteArray(s);
			return true;
		}
		
		#elif defined MAC
		
		for (char *f=Files.First(); f; f=Files.Next())
		{
			GUri u;
			u.Protocol = NewStr("file");
			u.Host = NewStr("localhost");
			u.Path = NewStr(f);
			OutputData->OwnStr(u.GetUri().Release());
			
			return true;
		}

		#endif
	}

	return false;
}

#ifdef __GTK_H__
static Gtk::GdkDragAction EffectToDragAction(int Effect)
{
	switch (Effect)
	{
		default:
		case DROPEFFECT_NONE:
			return Gtk::GDK_ACTION_DEFAULT;
		case DROPEFFECT_COPY:
			return Gtk::GDK_ACTION_COPY;
		case DROPEFFECT_MOVE:
			return Gtk::GDK_ACTION_MOVE;
		case DROPEFFECT_LINK:
			return Gtk::GDK_ACTION_LINK;
	}
}
#endif

int GDragDropSource::Drag(GView *SourceWnd, int Effect)
{
	LgiAssert(SourceWnd);
	if (!SourceWnd)
		return -1;

	#if WINNATIVE

	DWORD dwEffect = 0;
	Reset();
	IDataObject *Data = 0;
	if (QueryInterface(IID_IDataObject, (void**)&Data) == S_OK)
	{
		int Ok = ::DoDragDrop(Data, this, Effect, &dwEffect) == DRAGDROP_S_DROP;
		Data->Release();

		if (Ok)
		{
			return Effect;
		}
	}
	return DROPEFFECT_NONE;
	
	#elif defined(MAC) && !defined(COCOA)
	
	DragRef Drag = 0;
	OSErr e = NewDrag(&Drag);
	if (e) printf("%s:%i - NewDrag failed with %i\n", _FL, e);
	else
	{
		EventRecord Event;
		Event.what = mouseDown;
		Event.message = 0;
		Event.when = LgiCurrentTime();
		Event.where.h = 0;
		Event.where.v = 0;
		Event.modifiers = 0;

		List<char> Formats;
		if (GetFormats(Formats))
		{
			GDragDropSource *Src = this;
			uint32 LgiFmt;
			memcpy(&LgiFmt, LGI_LgiDropFormat, 4);
			#ifndef __BIG_ENDIAN__
			LgiFmt = LgiSwap32(LgiFmt);
			#endif

			OSErr e = AddDragItemFlavor(Drag,
										1000,
										LgiFmt,
										(const void*) &Src,
										sizeof(Src),
										flavorSenderOnly | flavorNotSaved);
			if (e) printf("%s:%i - AddDragItemFlavor=%i\n", __FILE__, __LINE__, e);
			
			int n = 1;
			for (char *f = Formats.First(); f; f = Formats.Next(), n++)
			{
				if (strlen(f) == 4)
				{
					FlavorType t;
					memcpy(&t, f, 4);
					#ifndef __BIG_ENDIAN__
					t = LgiSwap32(t);
					#endif
					
					GVariant v;
					if (GetData(&v, f))
					{
						void *Data = 0;
						int Size = 0;
						
						if (v.Type == GV_STRING)
						{
							Data = v.Str();
							if (Data)
								Size = strlen(v.Str());
						}
						else if (v.Type == GV_BINARY)
						{
							Data = v.Value.Binary.Data;
							Size = v.Value.Binary.Length;
						}
						else printf("%s:%i - Unsupported drag flavour %i\n", _FL, v.Type);
						
						if (Data)
						{
							e = AddDragItemFlavor(Drag,
												1000,
												t,
												Data,
												Size,
												flavorNotSaved);
							if (e) printf("%s:%i - AddDragItemFlavor=%i\n", __FILE__, __LINE__, e);
						}
					}
					else printf("%s:%i - GetData failed.\n", _FL);
				}
			}
			
			/*
			{
				SysFont->Fore(Rgb24(0xff, 0xff, 0xff));
				SysFont->Transparent(true);
				GDisplayString s(SysFont, "+");

				GMemDC m;
				if (m.Create(s.X() + 12, s.Y() + 2, 32))
				{
					m.Colour(Rgb32(0x30, 0, 0xff));
					m.Rectangle();
					d->Img.Reset(new CGImg(&m));
					s.Draw(&m, 6, 0);
				}
			}
			
			if (d->Img)
			{
				HIPoint Offset = { 16, 16 };
				e = SetDragImageWithCGImage(Drag, *d->Img, &Offset, kDragDarkerTranslucency);
				if (e) printf("%s:%i - SetDragImageWithCGImage failed with %i\n", _FL, e);
			}
			*/
			
			e = TrackDrag(Drag, &Event, 0);
			if (e == dragNotAcceptedErr)
			{
				printf("%s:%i - error 'dragNotAcceptedErr', formats were:\n", _FL);
				for (char *f = Formats.First(); f; f = Formats.Next())
				{
					printf("\t'%s'\n", f);
				}
			}
            else if (e)
            {
                printf("%s:%i - TrackDrag failed with %i\n", _FL, e);
			}
		}
		else printf("%s:%i - No formats to drag.\n", _FL);
				
		DisposeDrag(Drag);
	}
	
	return DROPEFFECT_NONE;

	#elif defined __GTK_H__

	if (!SourceWnd || !SourceWnd->Handle())
	{
		LgiTrace("%s:%i - Error: No source window or handle.\n", _FL);
		return -1;
	}

	List<char> Formats;
	if (!GetFormats(Formats))
	{
		LgiTrace("%s:%i - Error: Failed to get source formats.\n", _FL);
		return -1;
	}
	
	GArray<Gtk::GtkTargetEntry> e;
	for (char *f = Formats.First(); f; f = Formats.Next())
	{
		Gtk::GtkTargetEntry &entry = e.New();
		entry.target = f;
		entry.flags = 0;
		entry.info = GtkGetDndType(f);
	}
	
	Gtk::GtkTargetList *Targets = Gtk::gtk_target_list_new(&e[0], e.Length());
	
	Gtk::GdkDragAction Action = EffectToDragAction(Effect);
	
	int Button = 1;

	d->Ctx = Gtk::gtk_drag_begin(SourceWnd->Handle(),
								Targets,
								Action,
								Button,
								NULL); // Gdk event if available...

	#elif defined BEOS

	if (SourceWnd)
	{
		BMessage Msg(M_DRAG_DROP);
		Msg.AddPointer("GDragDropSource", this);

		GMouse m;
		SourceWnd->GetMouse(m);
		BRect r(m.x, m.y, m.x+14, m.y+14);

		SourceWnd->Handle()->DragMessage(&Msg, r);

		return DROPEFFECT_COPY;
	}
	return DROPEFFECT_NONE;

	#endif

    return -1;
}

#if WINNATIVE
ULONG STDMETHODCALLTYPE
GDragDropSource::AddRef()
{
	return 1;
}

ULONG STDMETHODCALLTYPE
GDragDropSource::Release()
{
	return 0;
}

HRESULT
STDMETHODCALLTYPE
GDragDropSource::QueryInterface(REFIID iid, void **ppv)
{
	*ppv=NULL;
	if (IID_IUnknown==iid)
	{
		*ppv=(void*)(IUnknown*)(IDataObject*)this;
	}

	if (IID_IEnumFORMATETC==iid)
	{
		*ppv=(void*)(IEnumFORMATETC*) this;
	}

	if (IID_IDataObject==iid)
	{
		GDataObject *Obj = new GDataObject(this);
		if (Obj)
		{
			*ppv= (void*)Obj;
			Obj->AddRef();
			return S_OK;
		}
		else
		{
			return E_NOINTERFACE;
		}
	}

	if (IID_IDropSource==iid)
	{
		*ppv=(void*)(IDropSource*) this;
	}

	if (NULL==*ppv)
	{
		return E_NOINTERFACE;
	}

	AddRef();
	
	return S_OK;
}

HRESULT STDMETHODCALLTYPE
GDragDropSource::Next(ULONG celt, FORMATETC *rgelt, ULONG *pceltFetched)
{
	HRESULT Ret = S_FALSE;

	#ifdef DND_DEBUG_TRACE
	LgiTrace("Next[%i]=", Index);
	#endif

	List<char> Formats;
	if (rgelt &&
		GetFormats(Formats))
	{
		char *i = Formats.ItemAt(Index);

		if (pceltFetched)
		{
			*pceltFetched = i ? 1 : 0;
		}

		if (i)
		{
			#ifdef DND_DEBUG_TRACE
			LgiTrace("\t%s", i);
			#endif

			rgelt->cfFormat = FormatToInt(i);
			rgelt->ptd = 0;
			rgelt->dwAspect = DVASPECT_CONTENT;
			rgelt->lindex = -1;
			rgelt->tymed = TYMED_HGLOBAL;
			Index++;

			Ret = S_OK;
		}
	}

	Formats.DeleteArrays();

	return Ret;
}

HRESULT STDMETHODCALLTYPE
GDragDropSource::Skip(ULONG celt)
{
	Index += celt;
	#ifdef DND_DEBUG_TRACE
	LgiTrace("Skip Index=%i", Index);
	#endif
	return S_FALSE;
}

HRESULT STDMETHODCALLTYPE
GDragDropSource::Reset()
{
	Index = 0;
	#ifdef DND_DEBUG_TRACE
	LgiTrace("Reset Index=%i", Index);
	#endif
	return S_OK;
}

HRESULT STDMETHODCALLTYPE
GDragDropSource::Clone(IEnumFORMATETC **ppenum)
{
	if (ppenum)
	{
		*ppenum = NULL;
		if (*ppenum)
		{
			return S_OK;
		}
		return E_OUTOFMEMORY;
	}
	return E_INVALIDARG;
}

HRESULT
STDMETHODCALLTYPE
GDragDropSource::QueryContinueDrag(BOOL fEscapePressed, DWORD InputState)
{
	HRESULT Ret = S_OK;

	if (fEscapePressed)
	{
		Ret = DRAGDROP_S_CANCEL;
	}
	else if (!(InputState & (MK_LBUTTON | MK_MBUTTON | MK_RBUTTON)))
	{
		Ret = DRAGDROP_S_DROP;
	}
	
	#ifdef DND_DEBUG_TRACE
	// LgiTrace("QueryContinueDrag, fEscapePressed=%i, InputState=%i, Result=%i", fEscapePressed, InputState, Ret);
	#endif

	return Ret;
}

HRESULT
STDMETHODCALLTYPE
GDragDropSource::GiveFeedback(DWORD dwEffect)
{
	return DRAGDROP_S_USEDEFAULTCURSORS;
}
#endif

////////////////////////////////////////////////////////////////////////////////////////////
GDragDropTarget::GDragDropTarget()
{
	DragDropData = 0;
	DragDropLength = 0;
	To = 0;

	#if WINNATIVE
	Refs = 0;
	DataObject = 0;
	#endif
}

GDragDropTarget::~GDragDropTarget()
{
	DragDropLength = 0;
	DeleteArray(DragDropData);
	Formats.DeleteArrays();
}

void GDragDropTarget::SetWindow(GView *to)
{
	bool Status = false;
	To = to;
	if (To)
	{
		To->DropTarget(this);
		Status = To->DropTarget(true);
		#ifdef MAC
		if (To->WindowHandle())
		#else
		if (To->Handle())
		#endif
		{
			OnDragInit(Status);
		}
		else
		{
			printf("%s:%i - Error\n", __FILE__, __LINE__);
		}
	}
}

#if WINNATIVE
ULONG STDMETHODCALLTYPE GDragDropTarget::AddRef()
{
	return InterlockedIncrement(&Refs); 
}

ULONG STDMETHODCALLTYPE GDragDropTarget::Release()
{
	return InterlockedDecrement(&Refs); 
}

HRESULT STDMETHODCALLTYPE GDragDropTarget::QueryInterface(REFIID iid, void **ppv)
{
	*ppv=NULL;

	if (IID_IUnknown==iid)
	{
		*ppv=(void*)(IUnknown*)(IDataObject*) this;
	}

	if (IID_IDropTarget==iid)
	{
		*ppv=(void*)(IDropTarget*) this;
	}

	if (NULL==*ppv)
	{
		return E_NOINTERFACE;
	}

	AddRef();
	
	return NOERROR;
}

HRESULT STDMETHODCALLTYPE GDragDropTarget::DragEnter(IDataObject *pDataObject, DWORD grfKeyState, POINTL pt, DWORD *pdwEffect)
{
	HRESULT Result = E_UNEXPECTED;

	DragDropLength = 0;
	DeleteArray(DragDropData);
	*pdwEffect = DROPEFFECT_NONE;

	POINT p;
	p.x = pt.x;
	p.y = pt.y;
	ScreenToClient(To->Handle(), &p);
	GdcPt2 Pt(p.x, p.y);

	// Clean out format list
	Formats.DeleteArrays();

	// Something from another app, enum the formats.
	IEnumFORMATETC *FormatETC = 0;
	if (pDataObject->EnumFormatEtc(DATADIR_GET, &FormatETC) == S_OK &&
		FormatETC)
	{
		ULONG Fetched = 0;
		FORMATETC Format;

		// Ask what formats are being dropped
		FormatETC->Reset();
		while (FormatETC->Next(1, &Format, &Fetched) == S_OK)
		{
			if (Fetched == 1)
			{
				char *s = FormatToStr(Format.cfFormat);
				if (s)
				{
					Formats.Insert(NewStr(s));
				}
			}
		}
		FormatETC->Release();
	}

	// Process the format list
	if (Formats.Length() > 0)
	{
		// Ask the app what formats it supports.
		// It deletes those it doesn't and leaves the formats it can handle
		*pdwEffect = WillAccept(Formats, Pt, MapW32FlagsToLgi(grfKeyState));
		Result = S_OK;
	}

	OnDragEnter();
	return Result;
}

HRESULT STDMETHODCALLTYPE GDragDropTarget::DragOver(DWORD grfKeyState, POINTL pt, DWORD *pdwEffect)
{
	if (pdwEffect)
	{
		POINT p;
		p.x = pt.x;
		p.y = pt.y;
		ScreenToClient(To->Handle(), &p);
		GdcPt2 Pt(p.x, p.y);
		*pdwEffect = WillAccept(Formats, Pt, MapW32FlagsToLgi(grfKeyState));
	}

	return S_OK;
}

HRESULT STDMETHODCALLTYPE GDragDropTarget::DragLeave()
{
	OnDragExit();
	return S_OK;
}

HRESULT STDMETHODCALLTYPE GDragDropTarget::Drop(IDataObject *pDataObject, DWORD grfKeyState, POINTL pt, DWORD *pdwEffect)
{
	HRESULT Result = E_UNEXPECTED;

	LgiAssert(To);

	DataObject = pDataObject;

	POINT p;
	p.x = pt.x;
	p.y = pt.y;
	ScreenToClient(To->Handle(), &p);
	GdcPt2 Pt(p.x, p.y);

	char *FormatName;
	if (FormatName = Formats.First())
	{
		bool FileContents = strcmp(FormatName, CFSTR_FILECONTENTS) == 0;

		FORMATETC Format;
		Format.cfFormat = FormatToInt(FormatName);
		Format.dwAspect = DVASPECT_CONTENT;
		Format.lindex = FileContents ? 0 : -1;
		Format.tymed = TYMED_ISTREAM | TYMED_HGLOBAL;
		Format.ptd = 0;

		STGMEDIUM Medium;
		ZeroObj(Medium);

		HRESULT Err;
		if ((Err = pDataObject->GetData(&Format, &Medium)) == S_OK)
		{
			Result = S_OK;
			switch (Medium.tymed)
			{
				case TYMED_HGLOBAL:
				{
					int Size = GlobalSize(Medium.hGlobal);
					void *Ptr = GlobalLock(Medium.hGlobal);
					if (Ptr)
					{
						GVariant v;
						v.SetBinary(Size, Ptr);
						OnDrop(FormatName, &v, Pt, MapW32FlagsToLgi(grfKeyState));

						GlobalUnlock(Ptr);
					}
					break;
				}
				case TYMED_ISTREAM:
				{
					int asd=0;
					break;
				}
				default:
				{
					// unsupported TYMED
					Result = E_UNEXPECTED;
					break;
				}
			}
		}
	}

	OnDragExit();

	return Result;
}

bool GDragDropTarget::OnDropFileGroupDescriptor(FILEGROUPDESCRIPTOR *Data, GArray<char*> &Files)
{
	bool Status = false;

	if (Data && Data->cItems > 0 && DataObject)
	{
		for (int i=0; i<Data->cItems; i++)
		{
			FORMATETC Format;
			Format.cfFormat = RegisterClipboardFormat(CFSTR_FILECONTENTS);
			Format.dwAspect = DVASPECT_CONTENT;
			Format.lindex = i;
			Format.tymed = TYMED_ISTORAGE | TYMED_HGLOBAL;
			Format.ptd = 0;

			STGMEDIUM Medium;
			ZeroObj(Medium);

			if (DataObject->GetData(&Format, &Medium) == S_OK)
			{
				int Size = 0;
				void *Ptr = 0;

				// Get data
				if (Medium.tymed == TYMED_HGLOBAL)
				{
					Size = GlobalSize(Medium.hGlobal);
					Ptr = GlobalLock(Medium.hGlobal);
				}
				else if (Medium.tymed == TYMED_ISTREAM)
				{
					STATSTG Stat;
					ZeroObj(Stat);
					// Stat.grfStateBits = -1;
					if (Medium.pstm->Stat(&Stat, STATFLAG_DEFAULT) == S_OK)
					{
						Size = Stat.cbSize.QuadPart;
						Ptr = new char[Size];
						if (Ptr)
						{
							ulong Read;
							if (Medium.pstm->Read(Ptr, Size, &Read) == S_OK)
							{
								Size = Read;
							}
						}
					}
				}

				// Process data..
				if (Ptr)
				{
					char Path[256];
					LgiGetSystemPath(LSP_TEMP, Path, sizeof(Path));
					LgiMakePath(Path, sizeof(Path), Path, Data->fgd[i].cFileName);

					GFile f;
					if (f.Open(Path, O_WRITE))
					{
						if (f.Write(Ptr, Size) == Size)
						{
							Files.Add(NewStr(Path));
							Status = true;
						}
					}
				}

				// Clean up
				if (Medium.tymed == TYMED_HGLOBAL)
				{
					GlobalUnlock(Ptr);
				}
				else if (Medium.tymed == TYMED_ISTREAM)
				{
					DeleteArray(Ptr);
				}
			}
		}
	}

	return Status;
}

#endif
