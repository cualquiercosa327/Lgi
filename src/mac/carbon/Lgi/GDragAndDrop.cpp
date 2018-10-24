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

/*
bool GDragDropSource::GetData(GArray<GDragData> &DragData)
{
	if (DragData.Length() == 0)
		return false;

	// Call the deprecated version of 'GetData'
	GVariant *v = &DragData[0].Data[0];
	char *fmt = DragData[0].Format;
	return GetData(v, fmt);
}
*/

bool GDragDropSource::SetIcon(GSurface *Img, GRect *SubRgn)
{
	d->ExternImg = Img;
	if (SubRgn)
		d->ExternSubRgn = *SubRgn;
	else
		d->ExternSubRgn.ZOff(-1, -1);

	return true;
}

bool GDragDropSource::CreateFileDrop(GDragData *OutputData, GMouse &m, GString::Array &Files)
{
	if (OutputData && Files.First())
	{
		for (auto f : Files)
		{
			// GString ef = LgiEscapeString(" ", f, -1);
			OutputData->Data.New() = f.Get();
		}

		OutputData->Format = LGI_FileDropFormat;
		return true;
	}

	return false;
}

/*
static
OSStatus
LgiDragSendDataFunction
(
	PasteboardRef Pb,
	PasteboardItemID Item,
	CFStringRef flavorType,
	void *context
)
{
	CFURLRef Loc;
	OSStatus e = PasteboardCopyPasteLocation(Pb, &Loc);
	if (e) printf("%s:%i - PasteboardCopyPasteLocation failed with %i\n", _FL, (int)e);
	
	return noErr;
}
*/

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

	// Setup the formats in the GDragData array
	GArray<GDragData> Data;
	for (int n=0; n<Formats.Length(); n++)
	{
		Data[n].Format = Formats[n];
	}
	
	// Get the data for each format...
	GetData(Data);
	
	// Setup a promise keeper...
	// OSStatus e = PasteboardSetPromiseKeeper(Pb, LgiDragSendDataFunction, this);
	// if (e) printf("%s:%i - PasteboardSetPromiseKeeper failed with %i\n", _FL, (int)e);
	
	// Now push the data into the pasteboard
	for (unsigned i=0; i<Data.Length(); i++)
	{
		GDragData &dd = Data[i];
		PasteboardFlavorFlags Flags = kPasteboardFlavorNoFlags;
		PasteboardItemID Id = (PasteboardItemID) (i + 1);

		for (int i=0; i<dd.Data.Length(); i++)
		{
			void *Ptr = NULL;
			int Size = 0;
			GVariant *v = &dd.Data[i];
			
			if (dd.IsFileDrop())
			{
				// Special handling for file drops...
				GString sPath = dd.Data[i].Str();
				if (sPath)
				{
					CFStringRef Path = sPath.CreateStringRef();
					if (Path)
					{
						CFURLRef Url = CFURLCreateWithFileSystemPath(NULL, Path, kCFURLPOSIXPathStyle, false);
						if (Url)
						{
							#if 0
							// NSFilenamesPboardType
							CFErrorRef Err;
							CFDataRef Data = CFURLCreateBookmarkData(NULL,
																	Url,
																	kCFURLBookmarkCreationSuitableForBookmarkFile /*| kCFURLBookmarkCreationWithSecurityScope*/, // CFURLBookmarkCreationOptions,
																	NULL, // CFArrayRef resourcePropertiesToInclude,
																	NULL, // CFURLRef relativeToURL,
																	&Err);
							#else
							CFDataRef Data = CFURLCreateData(NULL, Url, kCFStringEncodingUTF8, true);
							#endif
							if (Data)
							{
								// printf("PasteboardPutItemFlavor(%i, %s)\n", (int)Id, dd.Format.Get());
								status = PasteboardPutItemFlavor(Pb, Id, kUTTypeFileURL, Data, Flags);
								if (status) printf("%s:%i - PasteboardPutItemFlavor=%li\n", _FL, status);
							}
							else
							{
								#if 0
								CFIndex Code = CFErrorGetCode(Err);
								CFStringRef ErrStr = CFErrorCopyDescription(Err);
								GString ErrStr2 = ErrStr;
								CFRelease(ErrStr);
								LgiTrace("%s:%i - CFURLCreateBookmarkData error: %i, %s\n", _FL, Code, ErrStr2.Get());
								#else
								LgiTrace("%s:%i - CFURLCreateData failed\n", _FL);
								#endif
							}

							CFRelease(Url);
						}
						else LgiTrace("%s:%i - Failed to create URL for file drag.\n", _FL);

						CFRelease(Path);
					}
					else LgiTrace("%s:%i - Failed to create strref for file drag.\n", _FL);
				}
				else LgiTrace("%s:%i - No path for file drag.\n", _FL);
			}
			else if (dd.IsFormat(LGI_StreamDropFormat))
			{
				// Placeholder for file promise...
				// https://web.archive.org/web/20080725134839/http://developer.apple.com/technotes/tn/tn1085.html
				
				// kPasteboardTypeFilePromiseContent is the cocoa equiv
				
				CFStringRef Fmt;
				PromiseHFSFlavor flavor;
				flavor.fileType       = '****';
				flavor.fileCreator    = '****';
				flavor.fdFlags        = 0;
				flavor.promisedFlavor = kDragPromisedFlavor;
				
				GString sFmt, sId;
				CFDataRef data = CFDataCreate(0, (const UInt8*)&flavor, sizeof(flavor));
				sFmt = Fmt = UTCreateStringForOSType(kDragFlavorTypePromiseHFS);
				CFStringRef PrefId = UTTypeCreatePreferredIdentifierForTag(	kUTTagClassNSPboardType,
																			Fmt,
																			NULL);
				sId = PrefId;
				OSStatus e = PasteboardPutItemFlavor(Pb, Id, PrefId, data, flavorNotSaved);
				if (e) LgiTrace("%s:%i - PasteboardPutItemFlavor failed with %i\n", _FL, (int)e);
				else printf("PasteboardPutItemFlavor(%i, %s/%s)\n", (int)Id, sFmt.Get(), sId.Get());
				
				sFmt = Fmt = UTCreateStringForOSType(flavor.promisedFlavor);
				#if 0
				sId = PrefId = UTTypeCreatePreferredIdentifierForTag(	kUTTagClassNSPboardType,
																			Fmt,
																			NULL);
				#endif
				e = PasteboardPutItemFlavor(Pb, Id, Fmt, 0, flavorNotSaved);
				if (e) LgiTrace("%s:%i - PasteboardPutItemFlavor failed with %i\n", _FL, (int)e);
				else printf("PasteboardPutItemFlavor(%i, %s)\n", (int)Id, sFmt.Get());
			}
			else if (v->Type == GV_STRING)
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
			else if (v->Type == GV_VOID_PTR)
			{
				Ptr = &v->Value.Ptr;
				Size = sizeof(v->Value.Ptr);
			}
			else
			{
				printf("%s:%i - Unsupported drag flavour %i\n", _FL, v->Type);
				LgiAssert(0);
			}
			
			if (Ptr)
			{
				CFStringRef FlavorType = dd.Format.CreateStringRef();
				if (FlavorType)
				{
					CFDataRef Data = CFDataCreate(NULL, (const UInt8 *)Ptr, Size);
					if (Data)
					{
						// printf("PasteboardPutItemFlavor(%i, %s)\n", (int)Id, dd.Format.Get());
						status = PasteboardPutItemFlavor(Pb, Id, FlavorType, Data, Flags);
						if (status) printf("%s:%i - PasteboardPutItemFlavor=%li\n", _FL, status);
						CFRelease(Data);
					}
					else LgiTrace("%s:%i - Failed to create drop data.\n", _FL);
					CFRelease(FlavorType);
				}
				else LgiTrace("%s:%i - Failed to create flavour type.\n", _FL);
			}
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
		GDisplayString s(SysFont, "+");

		if (m.Create(s.X() + 12, s.Y() + 2, System32BitColourSpace))
		{
			m.Colour(Rgb32(0x30, 0, 0xff));
			m.Rectangle();

			SysFont->Fore(GColour::White);
			SysFont->Transparent(true);
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
			LHashTbl<StrKey<char,false>, int> Map(32, -1);
			ItemCount Items = 0;
			PasteboardGetItemCount(Pb, &Items);
			
			/*
			if (DropFormat)
				printf("Items=%li\n", Items);
			*/
			
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
								CurIdx = Map.Length();
								Map.Add(n, CurIdx);
							}
							
							// printf("[%li][%li]='%s' = %i\n", i, t, n.Get(), CurIdx);
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
					for (unsigned f=0; f<ItemFlavors.Length(); f++)
					{
						CFDataRef Ref;
						DropItemFlavor &Fl = ItemFlavors[f];
						e = PasteboardCopyItemFlavorData(Pb, Fl.ItemId, Fl.Flavor, &Ref);
						if (e)
						{
							GString Flavor = Fl.Flavor;
							printf("%s:%i - PasteboardCopyItemFlavorData(%s) failed with %lu.\n",
								_FL, Flavor.Get(), e);
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
									
									GVariant *v = &dd.Data.New();
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
									else if (!_stricmp(DropFormat, "public.file-url"))
									{
										GUri u((char*) Cp);
										Boolean ret = false;
										if (u.Protocol &&
											!_stricmp(u.Protocol, "file") &&
											u.Path &&
											!_strnicmp(u.Path, "/.file/", 7))
										{
											// Decode File reference URL
											CFURLRef url = CFURLCreateWithBytes(NULL, Cp, Len, kCFStringEncodingUTF8, NULL);
											if (url)
											{
												UInt8 buffer[MAX_PATH];
												ret = CFURLGetFileSystemRepresentation(url, true, buffer, sizeof(buffer));
												if (ret)
												{
													*v = (char*)buffer;
												}
											}
										}
										
										if (!ret) // Otherwise just pass the string along...
											*v = (char*) Cp;
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
		
		for (unsigned i=0; i<Data.Length(); i++)
		{
			if (Data[i].Format.Get() == NULL)
			{
				Data.DeleteAt(i--);
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

OSStatus GDragDropTarget::OnDragWithin(GView *v, DragRef Drag)
{
	GDragDropTarget *Target = this;
	GAutoPtr<DragParams> param(new DragParams(v, Drag, NULL));

	// Call the handler
	int Accept = Target->WillAccept(param->Formats, param->Pt, param->KeyState);
	for (GViewI *p = v->GetParent(); param && !Accept && p; p = p->GetParent())
	{
		GDragDropTarget *pt = p->DropTarget();
		const char *Cls = p->GetClass();
		if (pt)
		{
			param.Reset(new DragParams(p, Drag, NULL));
			Accept = pt->WillAccept(param->Formats, param->Pt, param->KeyState);
			printf("Cls=%s Accept=%i\n", Cls, Accept);
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

	#if 0
	printf("kEventControlDragWithin %ix%i accept=%i class=%s (",
		param->Pt.x, param->Pt.y,
		Accept,
		v->GetClass());
	for (char *f = param->Formats.First(); f; f = param->Formats.Next())
		printf("%s ", f);
	printf(")\n");
	#endif

	SetDragDropAction(Drag, param->Map(Accept));

	return noErr;
}

OSStatus GDragDropTarget::OnDragReceive(GView *v, DragRef Drag)
{
	GView *DropView = NULL;
	for (GView *p = v; p; p = p->GetParent() ? p->GetParent()->GetGView() : NULL)
	{
		// const char *Cls = p->GetClass();
		// printf("Cls=%s\n", Cls);
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
		printf("%s:%i - No accepted drop format. (view=%s, DropView=%p)\n",
			_FL,
			v->GetClass(),
			DropView);
		SetDragDropAction(Drag, kDragActionNothing);
	}
	
	return noErr;
}

