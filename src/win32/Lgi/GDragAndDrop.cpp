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
#include "GCom.h"

// #define DND_DEBUG_TRACE

class GDataObject : public GUnknownImpl<IDataObject>
{
	friend class GDndSourcePriv;
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

class GDndSourcePriv
{
public:
	GArray<GDragData> CurData;
	GDataObject *InDrag;

	GDndSourcePriv()
	{
		InDrag = NULL;
	}

	~GDndSourcePriv()
	{
		if (InDrag)
			InDrag->Source = NULL;
	}
};

/////////////////////////////////////////////////////////////////////////////////////////
int MapW32FlagsToLgi(int W32Flags)
{
	int f = 0;

	if (W32Flags & MK_CONTROL) f |= LGI_EF_CTRL;
	if (W32Flags & MK_SHIFT)   f |= LGI_EF_SHIFT;
	if (W32Flags & MK_ALT)     f |= LGI_EF_ALT;
	if (W32Flags & MK_LBUTTON) f |= LGI_EF_LEFT;
	if (W32Flags & MK_MBUTTON) f |= LGI_EF_MIDDLE;
	if (W32Flags & MK_RBUTTON) f |= LGI_EF_RIGHT;

	return f;
}

int FormatToInt(GString s)
{
	if (s && stricmp(s, "CF_HDROP") == 0) return CF_HDROP;
	return RegisterClipboardFormatA(s);
}

char *FormatToStr(int f)
{
	static char b[128];
	if (GetClipboardFormatNameA(f, b, sizeof(b)) > 0) return b;
	if (f == CF_HDROP) return "CF_HDROP";
	return 0;
}

GDataObject::GDataObject(GDragDropSource *source)
{
	AddInterface(IID_IDataObject, this);
	Source = source;
	Source->d->InDrag = this;
	Source->OnStartData();
}

GDataObject::~GDataObject()
{
	if (Source)
	{
		Source->OnEndData();
		Source->d->InDrag = NULL;
	}
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
	Source->d->CurData.Length(0);
	Source->GetFormats(Formats);
	for (char *f=Formats.First(); f; f=Formats.Next())
	{
		int n = FormatToInt(f);
		char *efmt = FormatToStr(pFormatEtc->cfFormat);
		if (n == pFormatEtc->cfFormat)
		{
			CurFormat = n;
			GDragData &CurData = Source->d->CurData.New();
			CurData.Format = f;
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
		Source->d->CurData.Length())
	{
		// the users don't HAVE to use this...
		Source->GetData(Source->d->CurData);

		ZeroObj(*PMedium);

		uchar *Ptr = 0;
		ssize_t Size = 0;
		GDragData &CurData = Source->d->CurData[0];
		if (CurData.Data.Length() > 0)
		{
			GVariant &Data = CurData.Data[0];
			switch (Data.Type)
			{
				case GV_NULL:
					break;
				case GV_BINARY:
				{
					PMedium->tymed = TYMED_HGLOBAL;
					Ptr = (uchar*)Data.Value.Binary.Data;
					Size = Data.Value.Binary.Length;
					break;
				}
				case GV_STRING:
				{
					PMedium->tymed = TYMED_HGLOBAL;
					Ptr = (uchar*)Data.Value.String;
					Size = Ptr ? strlen((char*)Ptr) + 1 : 0;
					break;
				}
				case GV_VOID_PTR:
				{
					PMedium->tymed = TYMED_HGLOBAL;
					Ptr = (uchar*)&Data.Value.Ptr;
					Size = sizeof(Data.Value.Ptr);
					break;
				}
				case GV_STREAM:
				{
					PMedium->tymed = TYMED_ISTREAM;
					
					PMedium->pstm = new GStreamWrap(Data.Value.Stream.Ptr);
					PMedium->pstm->AddRef();
					
					PMedium->pUnkForRelease = PMedium->pstm;
					PMedium->pUnkForRelease->AddRef();

					Ret = S_OK;
					break;
				}
				default:
				{
					// Unsupported format...
					LgiAssert(0);
					break;
				}
			}

			if (PMedium->tymed == TYMED_HGLOBAL &&
				Ptr &&
				Size > 0)
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

/////////////////////////////////////////////////////////////////////////////////////////
GDragDropSource::GDragDropSource()
{
	d = new GDndSourcePriv;
	Index = 0;

	OnRegister(true);
}

GDragDropSource::~GDragDropSource()
{
	DeleteObj(d);
}

bool GDragDropSource::CreateFileDrop(GDragData *OutputData, GMouse &m, List<char> &Files)
{
	if (!OutputData || !Files.First())
		return false;

	size_t Size = sizeof(DROPFILES) + sizeof(char16);

	List<char> Native;
	List<char16> NativeW;
	for (char *File=Files.First(); File; File=Files.Next())
	{
		char16 *f = Utf8ToWide(File);
		if (f)
		{
			auto Len = StrlenW(f) + 1;
			Size += Len * sizeof(char16);
			NativeW.Insert(f);
		}
	}

	DROPFILES *Dp = (DROPFILES*) new char[Size];
	bool Status = false;
	if (Dp)
	{
		Dp->pFiles = sizeof(DROPFILES);
		Dp->fWide = TRUE;
		Dp->fNC = true;
		Dp->pt.x = m.x;
		Dp->pt.y = m.y;

		char16 *f = (char16*) (((char*)Dp) + Dp->pFiles);
		for (char16 *File=NativeW.First(); File; File=NativeW.Next())
		{
			auto Len = StrlenW(File) + 1;
			StrcpyW(f, File);
			f += Len;
		}
		*f++ = 0;

		Status = OutputData->Data[0].SetBinary(Size, (uchar*)Dp);
		if (Status)
			OutputData->Format = LGI_FileDropFormat;
		DeleteArray((char*&)Dp);
	}

	Native.DeleteArrays();
	NativeW.DeleteArrays();

	return Status;
}

bool GDragDropSource::SetIcon(GSurface *Img, GRect *SubRgn)
{
	return false;
}

int GDragDropSource::Drag(GView *SourceWnd, int Effect)
{
	LgiAssert(SourceWnd != 0);
	if (!SourceWnd)
		return -1;

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
}

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

////////////////////////////////////////////////////////////////////////////////////////////
GDragDropTarget::GDragDropTarget()
{
	To = 0;

	#if WINNATIVE
	Refs = 0;
	DataObject = 0;
	#endif
}

GDragDropTarget::~GDragDropTarget()
{
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
					Formats.Insert(NewStr(s));
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

/*
int GDragDropTarget::OnDrop(GArray<GDragData> &DropData,
							GdcPt2 Pt,
							int KeyState)
{
	if (DropData.Length() == 0 ||
		DropData[0].Data.Length() == 0)
		return DROPEFFECT_NONE;
	
	char *Fmt = DropData[0].Format;
	GVariant *Var = &DropData[0].Data[0];
	return OnDrop(Fmt, Var, Pt, KeyState);
}
*/

HRESULT STDMETHODCALLTYPE GDragDropTarget::Drop(IDataObject *pDataObject, DWORD grfKeyState, POINTL pt, DWORD *pdwEffect)
{
	HRESULT Result = E_UNEXPECTED;

	LgiAssert(To != NULL);

	DataObject = pDataObject;

	POINT p;
	p.x = pt.x;
	p.y = pt.y;
	ScreenToClient(To->Handle(), &p);
	GdcPt2 Pt(p.x, p.y);

	GArray<GDragData> Data;	
	for (char *FormatName = Formats.First(); FormatName; FormatName = Formats.Next())
	{
		GString Str;
		bool IsStreamDrop = !_stricmp(FormatName, Str = LGI_StreamDropFormat);
		bool IsFileContents = !_stricmp(FormatName, Str = CFSTR_FILECONTENTS);
		GDragData &CurData = Data.New();
		CurData.Format = FormatName;

		FORMATETC Format;
		Format.cfFormat = FormatToInt(FormatName);
		Format.dwAspect = DVASPECT_CONTENT;
		Format.lindex = IsFileContents ? 0 : -1;
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
					auto Size = GlobalSize(Medium.hGlobal);
					void *Ptr = GlobalLock(Medium.hGlobal);
					if (Ptr)
					{
						if (IsStreamDrop)
						{
							// Do special case handling of CFSTR_FILEDESCRIPTORW here,
							// because we need to call GetData more times to get the file
							// contents as well... and the layers after this don't have the
							// capability to do it.
							
							// For each file...
							LPFILEGROUPDESCRIPTORW Gd = (LPFILEGROUPDESCRIPTORW)Ptr;
							for (UINT i = 0; i < Gd->cItems; i++)
							{
								FORMATETC Fmt;
								Fmt.cfFormat = FormatToInt(CFSTR_FILECONTENTS);
								Fmt.dwAspect = DVASPECT_CONTENT;
								Fmt.lindex = i;
								Fmt.tymed = TYMED_ISTREAM;
								Fmt.ptd = 0;

								STGMEDIUM Med;
								ZeroObj(Med);
								
								// Get the stream for it...
								HRESULT res = pDataObject->GetData(&Fmt, &Med);
								if (SUCCEEDED(res) &&
									Med.tymed == TYMED_ISTREAM)
								{
									IStreamWrap *w = new IStreamWrap(Med.pstm);
									if (w)
									{
										// Wrap a GStream around it so we can talk to it...
										GVariant *s = &CurData.Data.New();
										s->Type = GV_STREAM;
										s->Value.Stream.Ptr = w;
										s->Value.Stream.Own = true;

										// Store the name/size as well...
										w->Desc.Reset(new FILEDESCRIPTORW(Gd->fgd[i]));
									}
								}
							}
						}
						else
						{
							// Default handling
							GVariant *s = &CurData.Data.New();
							s->SetBinary(Size, Ptr);
						}
						
						GlobalUnlock(Ptr);
					}
					break;
				}
				case TYMED_ISTREAM:
				{
					GVariant *s = &CurData.Data.New();
					s->Type = GV_STREAM;
					s->Value.Stream.Own = true;
					s->Value.Stream.Ptr = new IStreamWrap(Medium.pstm);
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

	if (Data.Length() > 0)
	{
		OnDrop(Data, Pt, MapW32FlagsToLgi(grfKeyState));
	}

	OnDragExit();

	return Result;
}

bool GDragDropTarget::OnDropFileGroupDescriptor(FILEGROUPDESCRIPTOR *Data, GArray<char*> &Files)
{
	bool Status = false;

	if (Data && Data->cItems > 0 && DataObject)
	{
		for (UINT i=0; i<Data->cItems; i++)
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
				size_t Size = 0;
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
							if (Medium.pstm->Read(Ptr, (ULONG)Size, &Read) == S_OK)
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
					GString Str;
					LgiGetSystemPath(LSP_TEMP, Path, sizeof(Path));
					LgiMakePath(Path, sizeof(Path), Path, Str = Data->fgd[i].cFileName);

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

