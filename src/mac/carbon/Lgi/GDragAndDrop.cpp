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
#include "GDisplayString.h"
#include "INet.h"

// #define DND_DEBUG_TRACE

class GDndSourcePriv
{
public:
	GAutoString CurrentFormat;
	GSurface *ExternImg;
	GRect ExternSubRgn;
	
	#if !defined(COCOA)
	GAutoPtr<CGImg> Img;
	#endif
	
	GDndSourcePriv()
	{
		ExternImg = NULL;
		ExternSubRgn.ZOff(-1, -1);
	}
};

/////////////////////////////////////////////////////////////////////////////////////////
GDragDropSource::GDragDropSource()
{
	d = new GDndSourcePriv;
	OnRegister(true);
}

GDragDropSource::~GDragDropSource()
{
	DeleteObj(d);
}

bool GDragDropSource::SetIcon(GSurface *Img, GRect *SubRgn)
{
	d->ExternImg = Img;
	if (SubRgn)
		d->ExternSubRgn = *SubRgn;
	else
		d->ExternSubRgn.ZOff(-1, -1);

	return true;
}

bool GDragDropSource::CreateFileDrop(GVariant *OutputData, GMouse &m, List<char> &Files)
{
	if (OutputData && Files.First())
	{
		if (Files.Length() == 1)
		{
			GUri u;
			u.Protocol = NewStr("file");
			u.Host = NewStr("localhost");
			u.Path = NewStr(Files.First());
			OutputData->OwnStr(u.GetUri().Release());
		}
		else
		{
			if (!OutputData->SetList())
				return false;

			for (char *f=Files.First(); f; f=Files.Next())
			{
				GUri u;
				u.Protocol = NewStr("file");
				u.Host = NewStr("localhost");
				u.Path = NewStr(f);
				GVariant *v = new GVariant;
				if (!v)
					return false;
				v->OwnStr(u.GetUri().Release());
				OutputData->Value.Lst->Insert(v);
			}
		}

		return true;
	}

	return false;
}

int GDragDropSource::Drag(GView *SourceWnd, int Effect)
{
	LgiAssert(SourceWnd);
	if (!SourceWnd)
		return -1;

	#if !defined(COCOA)
	
	DragRef Drag = 0;
	OSErr err = NewDrag(&Drag);
	if (err)
	{
		printf("%s:%i - NewDrag failed with %i\n", _FL, err);
		return DROPEFFECT_NONE;
	}

	PasteboardRef Pb;
	OSStatus status = GetDragPasteboard(Drag, &Pb);
	if (status)
	{
		printf("%s:%i - GetDragPasteboard failed with %li\n", _FL, status);
		return DROPEFFECT_NONE;
	}
	
	EventRecord Event;
	Event.what = mouseDown;
	Event.message = 0;
	Event.when = LgiCurrentTime();
	Event.where.h = 0;
	Event.where.v = 0;
	Event.modifiers = 0;

	List<char> Formats;
	if (!GetFormats(Formats))
	{
		printf("%s:%i - GetFormats failed\n", _FL);
		return DROPEFFECT_NONE;
	}

	GDragDropSource *Src = this;
	uint32 LgiFmt;
	memcpy(&LgiFmt, LGI_LgiDropFormat, 4);
	#ifndef __BIG_ENDIAN__
	LgiFmt = LgiSwap32(LgiFmt);
	#endif

	#if 1
	CFStringRef FlavorType = CFStringCreateWithCString(NULL, LGI_LgiDropFormat, kCFStringEncodingUTF8);
	CFDataRef Data = CFDataCreate(NULL, (const UInt8 *)&Src, sizeof(Src));
	PasteboardFlavorFlags Flags = kPasteboardFlavorSenderOnly;
	status = PasteboardPutItemFlavor(Pb, (PasteboardItemID)0, FlavorType, Data, Flags);
	if (status) printf("%s:%i - PasteboardPutItemFlavor=%li\n", _FL, status);
	CFRelease(FlavorType);
	CFRelease(Data);
	#else
	OSErr e = AddDragItemFlavor(Drag,
								1000,
								LgiFmt,
								(const void*) &Src,
								sizeof(Src),
								flavorSenderOnly | flavorNotSaved);
	if (e) printf("%s:%i - AddDragItemFlavor=%i\n", _FL, e);
	#endif
	
	int n = 1;
	for (char *f = Formats.First(); f; f = Formats.Next(), n++)
	{
		// if (strlen(f) == 4)
		{
			/*
			FlavorType t;
			memcpy(&t, f, 4);
			#ifndef __BIG_ENDIAN__
			t = LgiSwap32(t);
			#endif
			*/
			
			GVariant Data;
			GArray<GVariant*> a;
			if (GetData(&Data, f))
			{
				if (Data.Type == GV_LIST)
				{
					for (GVariant *i=Data.Value.Lst->First(); i; i=Data.Value.Lst->Next())
					{
						a.Add(i);
					}
				}
				else a.Add(&Data);
				
				for (int i=0; i<a.Length(); i++)
				{
					void *Ptr = NULL;
					int Size = 0;
					GVariant *v = a[i];
					
					if (v->Type == GV_STRING)
					{
						Ptr = v->Str();
						if (Ptr)
							Size = strlen((char*)Ptr);
					}
					else if (v->Type == GV_BINARY)
					{
						Ptr = v->Value.Binary.Data;
						Size = v->Value.Binary.Length;
					}
					else
					{
						printf("%s:%i - Unsupported drag flavour %i\n", _FL, v->Type);
						LgiAssert(0);
					}
					
					if (Ptr)
					{
						#if 1
						CFStringRef FlavorType = CFStringCreateWithCString(NULL, f, kCFStringEncodingUTF8);
						CFDataRef Data = CFDataCreate(NULL, (const UInt8 *)Ptr, Size);
						PasteboardFlavorFlags Flags = kPasteboardFlavorNoFlags;
						status = PasteboardPutItemFlavor(Pb, (PasteboardItemID)n, FlavorType, Data, Flags);
						if (status) printf("%s:%i - PasteboardPutItemFlavor=%li\n", _FL, status);
						CFRelease(FlavorType);
						CFRelease(Data);
						#else
						e = AddDragItemFlavor(Drag,
											1000+i,
											t,
											Ptr,
											Size,
											flavorNotSaved);
						if (e) printf("%s:%i - AddDragItemFlavor=%i\n", _FL, e);
						#endif
					}
				}
			}
			else printf("%s:%i - GetData failed.\n", _FL);
		}
	}
	
	GMemDC m;
	
	if (d->ExternImg)
	{
		GRect r = d->ExternSubRgn;
		if (!r.Valid())
		{
			r = d->ExternImg->Bounds();
		}
		
		if (m.Create(r.X(), r.Y(), System32BitColourSpace))
		{
			m.Blt(0, 0, d->ExternImg, &r);
			d->Img.Reset(new CGImg(&m));
		}
	}
	else
	{
		SysFont->Fore(Rgb24(0xff, 0xff, 0xff));
		SysFont->Transparent(true);
		GDisplayString s(SysFont, "+");

		if (m.Create(s.X() + 12, s.Y() + 2, 32))
		{
			m.Colour(Rgb32(0x30, 0, 0xff));
			m.Rectangle();
			s.Draw(&m, 6, 0);
			d->Img.Reset(new CGImg(&m));
		}
	}
	
	if (d->Img)
	{
		HIPoint Offset = { 16, 16 };
		status = SetDragImageWithCGImage(Drag, *d->Img, &Offset, kDragDarkerTranslucency);
		if (status) printf("%s:%i - SetDragImageWithCGImage failed with %li\n", _FL, status);
	}
	
	status = TrackDrag(Drag, &Event, 0);
	if (status == dragNotAcceptedErr)
	{
		printf("%s:%i - error 'dragNotAcceptedErr', formats were:\n", _FL);
		for (char *f = Formats.First(); f; f = Formats.Next())
		{
			printf("\t'%s'\n", f);
		}
	}
	else if (status)
	{
		printf("%s:%i - TrackDrag failed with %li\n", _FL, status);
	}
			
	DisposeDrag(Drag);
	
	#endif
	
	return DROPEFFECT_NONE;
}

////////////////////////////////////////////////////////////////////////////////////////////
GDragDropTarget::GDragDropTarget()
{
	DragDropData = 0;
	DragDropLength = 0;
	To = 0;
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
		if (To->WindowHandle())
		{
			OnDragInit(Status);
		}
		else
		{
			printf("%s:%i - Error\n", __FILE__, __LINE__);
		}
	}
}


