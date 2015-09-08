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
#include "GViewPriv.h"

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

bool GDragDropSource::GetData(GArray<GDragData> &DragData)
{
	if (DragData.Length() == 0)
		return false;

	// Call the deprecated version of 'GetData'
	GVariant *v = &DragData[0].Data[0];
	char *fmt = DragData[0].Format;
	return GetData(v, fmt);
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

	/*
	GDragDropSource *Src = this;

	CFStringRef FlavorType = CFStringCreateWithCString(NULL, LGI_LgiDropFormat, kCFStringEncodingUTF8);
	CFDataRef Data = CFDataCreate(NULL, (const UInt8 *)&Src, sizeof(Src));
	PasteboardFlavorFlags Flags = kPasteboardFlavorSenderOnly;
	status = PasteboardPutItemFlavor(Pb, (PasteboardItemID)0, FlavorType, Data, Flags);
	if (status) printf("%s:%i - PasteboardPutItemFlavor=%li\n", _FL, status);
	CFRelease(FlavorType);
	CFRelease(Data);
	*/
	
	int n = 1;
	for (char *f = Formats.First(); f; f = Formats.Next(), n++)
	{
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
					CFStringRef FlavorType = CFStringCreateWithCString(NULL, f, kCFStringEncodingUTF8);
					CFDataRef Data = CFDataCreate(NULL, (const UInt8 *)Ptr, Size);
					PasteboardFlavorFlags Flags = kPasteboardFlavorNoFlags;
					
					
					printf("PasteboardPutItemFlavor(%i, %s)\n", i+1, f);
					
					status = PasteboardPutItemFlavor(Pb, (PasteboardItemID)(i+1), FlavorType, Data, Flags);
					
					
					if (status) printf("%s:%i - PasteboardPutItemFlavor=%li\n", _FL, status);
					CFRelease(FlavorType);
					CFRelease(Data);
				}
			}
		}
		else printf("%s:%i - GetData failed.\n", _FL);
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
struct DropItemFlavor
{
	int					Index;
	PasteboardItemID	ItemId;
	CFStringRef			Flavor;
	GString				FlavorStr;
};

struct DragParams
{
	GdcPt2 Pt;
	List<char> Formats;
	GArray<GDragData> Data;
	int KeyState;
	
	DragParams(GViewI *v, DragRef Drag, const char *DropFormat)
	{
		KeyState = 0;
		
		// Get the mouse position
		Point mouse;
		Point globalPinnedMouse;
		OSErr err = GetDragMouse(Drag, &mouse, &globalPinnedMouse);
		if (err)
		{
			printf("%s:%i - GetDragMouse failed with %i\n", _FL, err);
		}
		else
		{
			Pt.x = mouse.h;
			Pt.y = mouse.v;
			v->PointToView(Pt);
		}
		
		// Get the keyboard state
		SInt16 modifiers = 0;
		SInt16 mouseDownModifiers = 0;
		SInt16 mouseUpModifiers = 0;
		err = GetDragModifiers(Drag, &modifiers, &mouseDownModifiers, &mouseUpModifiers);
		if (err)
		{
			printf("%s:%i - GetDragModifiers failed with %i\n", _FL, err);
		}
		else
		{
			if (modifiers & cmdKey)
				KeyState |= LGI_EF_SYSTEM;
			if (modifiers & shiftKey)
				KeyState |= LGI_EF_SHIFT;
			if (modifiers & optionKey)
				KeyState |= LGI_EF_ALT;
			if (modifiers & controlKey)
				KeyState |= LGI_EF_CTRL;
		}

		// Get the data formats
		PasteboardRef Pb;
		OSStatus e = GetDragPasteboard(Drag, &Pb);
		GArray<DropItemFlavor> ItemFlavors;
		if (e)
		{
			printf("%s:%i - GetDragPasteboard failed with %li\n", _FL, e);
		}
		else
		{
			GHashTbl<char*, int> Map(32, false, NULL, -1);
			ItemCount Items = 0;
			PasteboardGetItemCount(Pb, &Items);
			
			if (DropFormat)
				printf("Items=%li\n", Items);
			
			for (CFIndex i=1; i<=Items; i++)
			{
				PasteboardItemID Item;
				e = PasteboardGetItemIdentifier(Pb, i, &Item);
				if (e)
				{
					printf("%s:%i - PasteboardGetItemIdentifier[%li]=%li\n", _FL, i-1, e);
					continue;
				}
				
				CFArrayRef FlavorTypes;
				e = PasteboardCopyItemFlavors(Pb, Item, &FlavorTypes);
				if (e)
				{
					printf("%s:%i - PasteboardCopyItemFlavors[%li]=%li\n", _FL, i-1, e);
					continue;
				}
				
				CFIndex Types = CFArrayGetCount(FlavorTypes);
				if (DropFormat)
					printf("[%li] FlavorTypes=%li\n", i, Types);
				for (CFIndex t=0; t<Types; t++)
				{
					CFStringRef flavor = (CFStringRef)CFArrayGetValueAtIndex(FlavorTypes, t);
					if (flavor)
					{
						GString n = flavor;

						if (DropFormat)
						{
							int CurIdx = Map.Find(n);
							if (CurIdx < 0)
							{
								CurIdx = Data.Length();
								Map.Add(n, CurIdx);
							}
							
							printf("[%li][%li]='%s'\n", i, t, n.Get());
							DropItemFlavor &Fl = ItemFlavors.New();
							Fl.Index = CurIdx;
							Fl.ItemId = Item;
							Fl.Flavor = flavor;
							Fl.FlavorStr = n;
						}
						else
						{
							Formats.Insert(NewStr(n));
						}
					}
				}
				
				if (ItemFlavors.Length())
				{
					for (unsigned i=0; i<ItemFlavors.Length(); i++)
					{
						CFDataRef Ref;
						DropItemFlavor &Fl = ItemFlavors[i];
						e = PasteboardCopyItemFlavorData(Pb, Fl.ItemId, Fl.Flavor, &Ref);
						if (e)
						{
							printf("%s:%i - PasteboardCopyItemFlavorData failed with %lu.\n", _FL, e);
						}
						else
						{
							GDragData &dd = Data[Fl.Index];
							if (!dd.Format)
								dd.Format = Fl.FlavorStr;
							
							CFIndex Len = CFDataGetLength(Ref);
							const UInt8 *Ptr = CFDataGetBytePtr(Ref);
							if (Len > 0 && Ptr != NULL)
							{
								uint8 *Cp = new uint8[Len+1];
								if (Cp)
								{
									memcpy(Cp, Ptr, Len);
									Cp[Len] = 0;
									
									GVariant *v = &dd.Data[i];
									if (!_stricmp(LGI_LgiDropFormat, DropFormat))
									{
										GDragDropSource *Src = NULL;
										if (Len == sizeof(Src))
										{
											Src = *((GDragDropSource**)Ptr);
											v->Empty();
											v->Type = GV_VOID_PTR;
											v->Value.Ptr = Src;
										}
										else LgiAssert(!"Wrong pointer size");
									}
									else
									{
										v->SetBinary(Len, Cp, true);
									}
								}
							}
							else
							{
								printf("%s:%i - Pasteboard data error: %p %li.\n", _FL, Ptr, Len);
							}
							
							CFRelease(Ref);
						}
					}
				}
				ItemFlavors.Length(0);
				CFRelease(FlavorTypes);
			}
		}
	}
	
	~DragParams()
	{
		Formats.DeleteArrays();
	}
	
	DragActions Map(int Accept)
	{
		DragActions a = 0;
		if (Accept & DROPEFFECT_COPY)
			a |= kDragActionCopy;
		if (Accept & DROPEFFECT_MOVE)
			a |= kDragActionMove;
		if (Accept & DROPEFFECT_LINK)
			a |= kDragActionAlias;
		return a;
	}
};

////////////////////////////////////////////////////////////////////////////////////////////
GDragDropTarget::GDragDropTarget()
{
	To = 0;
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
		if (To->WindowHandle())
		{
			OnDragInit(Status);
		}
		else
		{
			printf("%s:%i - Error\n", _FL);
		}
	}
}

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

OSStatus GDragDropTarget::OnDragWithin(GView *v, DragRef Drag)
{
	GDragDropTarget *Target = this;
	GAutoPtr<DragParams> param(new DragParams(v, Drag, NULL));

	// Call the handler
	int Accept = Target->WillAccept(param->Formats, param->Pt, param->KeyState);
	for (GViewI *p = v->GetParent(); param && !Accept && p; p = p->GetParent())
	{
		GDragDropTarget *pt = p->DropTarget();
		if (pt)
		{
			param.Reset(new DragParams(p, Drag, NULL));
			Accept = pt->WillAccept(param->Formats, param->Pt, param->KeyState);
			if (Accept)
			{
				v = p->GetGView();
				Target = pt;
				break;
			}
		}
	}
	if (Accept)
	{
		v->d->AcceptedDropFormat.Reset(NewStr(param->Formats.First()));
		LgiAssert(v->d->AcceptedDropFormat.Get());
	}

	printf("kEventControlDragWithin %ix%i accept=%i class=%s\n",
		param->Pt.x, param->Pt.y,
		Accept,
		v->GetClass());
	SetDragDropAction(Drag, param->Map(Accept));

	return noErr;
}

OSStatus GDragDropTarget::OnDragReceive(GView *v, DragRef Drag)
{
	int Accept = 0;
	GView *DropView = NULL;
	for (GView *p = v; p; p = p->GetParent() ? p->GetParent()->GetGView() : NULL)
	{
		if (p->d->AcceptedDropFormat)
		{
			DropView = p;
			break;
		}
	}
	if (DropView &&
		DropView->d->AcceptedDropFormat)
	{
		GDragDropTarget *pt = DropView->DropTarget();
		if (pt)
		{
			DragParams p(DropView, Drag, DropView->d->AcceptedDropFormat);
			int Accept = pt->OnDrop(p.Data,
									p.Pt,
									p.KeyState);
			SetDragDropAction(Drag, p.Map(Accept));
		}
	}
	else
	{
		printf("%s:%i - No accepted drop format. (view=%s)\n", _FL, v->GetClass());
		SetDragDropAction(Drag, kDragActionNothing);
	}
	
	return noErr;
}

