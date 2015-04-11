/*hdr
**      FILE:           GView.cpp
**      AUTHOR:         Matthew Allen
**      DATE:           23/4/98
**      DESCRIPTION:    Linux GView Implementation
**
**      Copyright (C) 1998-2003, Matthew Allen
**              fret@memecode.com
*/

#include <stdio.h>
#include <stdlib.h>

#include "Lgi.h"
#include "GDragAndDrop.h"
#include "GScrollBar.h"
#include "GEdit.h"
#include "GViewPriv.h"
#include "GUtf8.h"
#include "GCss.h"
#include "GPopup.h"

#define ADJ_LEFT					1
#define ADJ_RIGHT					2
#define ADJ_UP						3
#define ADJ_DOWN					4

struct X11_INVALIDATE_PARAMS
{
	GView *View;
	GRect Rgn;
	bool Repaint;
};

// CursorData is a bitmap in an array of uint32's. This is generated from a graphics file:
// ./Code/cursors.png
//
// The pixel values are turned into C code by a program called i.Mage:
//	http://www.memecode.com/image.php
//
// Load the graphic into i.Mage and then go Edit->CopyAsCode
// Then paste the text into the CursorData variable at the bottom of this file.
//
// This saves a lot of time finding and loading an external resouce, and even having to
// bundle extra files with your application. Which have a tendancy to get lost along the
// way etc.
extern uint32 CursorData[];
GInlineBmp Cursors =
{
	300, 20, 8, CursorData
};

struct LgiCursorInfo
{
public:
	GRect Pos;
	GdcPt2 HotSpot;
}
CursorMetrics[] =
{
	// up arrow
	{ GRect(0, 0, 8, 15),			GdcPt2(4, 0) },
	// cross hair
	{ GRect(20, 0, 38, 18),			GdcPt2(29, 9) },
	// hourglass
	{ GRect(40, 0, 51, 15),			GdcPt2(45, 8) },
	// I beam
	{ GRect(60, 0, 66, 17),			GdcPt2(63, 8) },
	// N-S arrow
	{ GRect(80, 0, 91, 16),			GdcPt2(85, 8) },
	// E-W arrow
	{ GRect(100, 0, 116, 11),		GdcPt2(108, 5) },
	// NW-SE arrow
	{ GRect(120, 0, 132, 12),		GdcPt2(126, 6) },
	// NE-SW arrow
	{ GRect(140, 0, 152, 12),		GdcPt2(146, 6) },
	// 4 way arrow
	{ GRect(160, 0, 178, 18),		GdcPt2(169, 9) },
	// Blank
	{ GRect(0, 0, 0, 0),			GdcPt2(0, 0) },
	// Vertical split
	{ GRect(180, 0, 197, 16),		GdcPt2(188, 8) },
	// Horizontal split
	{ GRect(200, 0, 216, 17),		GdcPt2(208, 8) },
	// Hand
	{ GRect(220, 0, 233, 13),		GdcPt2(225, 0) },
	// No drop
	{ GRect(240, 0, 258, 18),		GdcPt2(249, 9) },
	// Copy drop
	{ GRect(260, 0, 279, 19),		GdcPt2(260, 0) },
	// Move drop
	{ GRect(280, 0, 299, 19),		GdcPt2(280, 0) },
};

////////////////////////////////////////////////////////////////////////////
LgiFunc void _Dump(int idx, GViewI *root, HIViewRef v, int ox, int oy, int depth);
LgiFunc void _Dump2(GViewI *root, HIViewRef v, int depth);

void DumpHnd(HIViewRef v, int depth)
{
	for (int i=0; i<depth*4; i++)
		printf(" ");

	HIRect r;
	HIViewGetFrame(v, &r);
	HIViewFeatures f;
	HIViewGetFeatures(v, &f);
	bool op = (f & kHIViewIsOpaque) != 0;
	GView *Ptr = GWindowFromHandle(v);
	
	char Buf[256];
	const char *Class = 0;
	if (!Ptr)
	{
		if (!GetControlProperty(v, 'meme', 'type', sizeof(Buf), 0, Buf))
			Class = Buf;
		else
			Class = "[Unknown]";
	}
	else Class = Ptr->GetClass();
	
	printf("%s hnd=%p p=%i,%i-%ix%i vis=%i opaque=%i\n",
		Class,
		v,
		(int)r.origin.x, (int)r.origin.y, (int)r.size.width, (int)r.size.height,
		IsControlVisible(v),
		op);
	
	for (HIViewRef c = HIViewGetFirstSubview(v); c; c = HIViewGetNextView(c))
	{
		DumpHnd(c, depth + 1);
	}
}

GView *GWindowFromHandle(OsView h)
{
	if (!h)
		return 0;
	
	GViewI *Ptr = 0;
	OSStatus e = GetControlProperty(h, 'meme', 'view', sizeof(Ptr), 0, &Ptr);
	if (e) ; //printf("%s:%i - GetControlProperty failed with %i\n", _FL, e);
	else if (Ptr) return Ptr->GetGView();
	return 0;
}

GKey::GKey(int Vkey, int flags)
{
	c16 = vkey = Vkey;
	Flags = flags;
	Data = 0;
	IsChar = false;
}

////////////////////////////////////////////////////////////////////////////
HIRect &ConvertRect(GRect &g)
{
	static HIRect r;
	r.origin.x = (float) g.x1;
	r.origin.y = (float) g.y1;
	r.size.width = (float) g.X();
	r.size.height = (float) g.Y();
	return r;
}

////////////////////////////////////////////////////////////////////////////
void _lgi_yield()
{
	LgiApp->Run(false);
}

bool LgiIsKeyDown(int Key)
{
	LgiAssert(!"Not impl.");
	return false;
}

bool LgiIsMounted(char *Name)
{
	LgiAssert(!"Not impl.");
	return false;
}

bool LgiMountVolume(char *Name)
{
	LgiAssert(!"Not impl.");
	return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
pascal
OSStatus
GViewProc
(
	EventHandlerCallRef nextHandler, 
	EventRef inEvent,
	void * userData
)
{
	OSStatus Status = eventNotHandledErr;

	GView *v = (GView*)userData;
	if (!v)
	{
		printf("%s:%i - No GView ptr.\n", __FILE__, __LINE__);
		return 0;
	}
	UInt32 eventClass = GetEventClass( inEvent );
	UInt32 eventKind = GetEventKind( inEvent );
	
	switch (eventKind)
	{
		case kEventControlDraw:
		{
			GScreenDC dc(v);
			v->_Paint(&dc);
			Status = noErr;
			break;
		}
	}
	
	return Status;
}

#define GViewSubClassName			CFSTR("com.memecode.lgiview")

HIObjectClassRef GViewPrivate::BaseClass = 0;

GViewPrivate::GViewPrivate()
{
	TabStop = false;
	// CursorId = 0;
	Parent = NULL;
	ParentI = NULL;
	Notify = NULL;
	CtrlId = -1;
	DropTarget = 0;
	Font = NULL;
	FontOwn = false;
	Popup = NULL;
	Pulse = 0;
	DndHandler = NULL;
		
	static bool First = true;
	if (First)
	{
		First = false;
		EventTypeSpec Events[] =
		{
			{ kEventClassHIObject, kEventHIObjectConstruct },
			{ kEventClassHIObject, kEventHIObjectInitialize },
			{ kEventClassHIObject, kEventHIObjectDestruct },
			{ kEventClassControl, kEventControlHitTest },
			{ kEventClassControl, kEventControlTrack },
			{ kEventClassControl, kEventControlDraw }
		};

		OSStatus e = HIObjectRegisterSubclass
			(
				GViewSubClassName,
				0, // base class, 0 = HIObject
				0,
				NewEventHandlerUPP(GViewProc),
				GetEventTypeCount(Events),
				Events,
				0,
				&BaseClass
			);
		if (e)
			printf("%s:%i - HIObjectRegisterSubclass failed (e=%i).\n", __FILE__, __LINE__, (int)e);
	}
}
	
GViewPrivate::~GViewPrivate()
{
	DeleteObj(Popup);
	LgiAssert(Pulse == 0);
}

struct DropItemFlavor
{
	PasteboardItemID	ItemId;
	CFStringRef			Flavor;
};

struct DragParams
{
	GdcPt2 Pt;
	List<char> Formats;
	GVariant Data;
	int KeyState;
	
	DragParams(GView *v, DragRef Drag, const char *Drop)
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
			ItemCount Items = 0;
			PasteboardGetItemCount(Pb, &Items);
			if (Drop)
			{
				printf("Items=%li\n", Items);
			}
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
				if (Drop)
					printf("[%li] FlavorTypes=%li\n", i, Types);
				for (CFIndex t=0; t<Types; t++)
				{
					CFStringRef flavor = (CFStringRef)CFArrayGetValueAtIndex(FlavorTypes, t);
					if (flavor)
					{
						char n[256];
						if (CFStringGetCString(flavor, n, sizeof(n), kCFStringEncodingUTF8))
						{
							if (Drop)
							{
								printf("[%li][%li]='%s'\n", i, t, n);
								if (!_stricmp(n, Drop))
								{
									if (Formats.Length() == 0)
										Formats.Insert(NewStr(Drop));
										
									DropItemFlavor &Fl = ItemFlavors.New();
									Fl.ItemId = Item;
									Fl.Flavor = flavor;
								}
							}
							else
							{
								Formats.Insert(NewStr(n));
							}
						}
					}
				}
				
				if (ItemFlavors.Length())
				{
					if (ItemFlavors.Length() > 1)
						Data.SetList();
					
					for (unsigned i=0; i<ItemFlavors.Length(); i++)
					{
						CFDataRef Ref;
						e = PasteboardCopyItemFlavorData(Pb, ItemFlavors[i].ItemId, ItemFlavors[i].Flavor, &Ref);
						if (e)
						{
							printf("%s:%i - PasteboardCopyItemFlavorData failed with %lu.\n", _FL, e);
						}
						else
						{
							CFIndex Len = CFDataGetLength(Ref);
							const UInt8 *Ptr = CFDataGetBytePtr(Ref);
							if (Len > 0 && Ptr != NULL)
							{
								uint8 *Cp = new uint8[Len+1];
								if (Cp)
								{
									memcpy(Cp, Ptr, Len);
									Cp[Len] = 0;
									
									if (ItemFlavors.Length() > 1)
									{
										GVariant *v = new GVariant;
										if (v)
										{
											v->SetBinary(Len, Cp, true);
											Data.Value.Lst->Insert(v);
										}
									}
									else if (!_stricmp(LGI_LgiDropFormat, Drop))
									{
										GDragDropSource *Src = NULL;
										if (Len == sizeof(Src))
										{
											Src = *((GDragDropSource**)Ptr);
											Data.Empty();
											Data.Type = GV_VOID_PTR;
											Data.Value.Ptr = Src;
										}
										else LgiAssert(!"Wrong pointer size");
									}
									else
									{
										Data.SetBinary(Len, Cp, true);
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

pascal OSStatus LgiViewDndHandler(EventHandlerCallRef inHandlerCallRef, EventRef inEvent, void *inUserData)
{
	OSStatus result = eventNotHandledErr;

	GView *v = (GView*)inUserData;
	if (!v) return result;
	
	UInt32 eventClass = GetEventClass( inEvent );
	UInt32 eventKind = GetEventKind( inEvent );

	if (eventClass != kEventClassControl)
		return result;

	DragRef Drag;
	OSStatus e = GetEventParameter(inEvent, kEventParamDragRef, typeDragRef, NULL, sizeof(Drag), NULL, &Drag);
	if (e)
	{
		printf("%s:%i - GetEventParameter failed with %li\n", _FL, e);
		return result;
	}
	
	GDragDropTarget *Target = v->DropTarget();
	if (!Target)
	{
		printf("%s:%i - %s doesn't have Target\n", _FL, v->GetClass());
		return result;
	}

	switch (eventKind)
	{
		case kEventControlDragEnter:
		{
			printf("kEventControlDragEnter\n");
			bool acceptDrop = true;
			
			SetEventParameter(	inEvent,
								kEventParamControlWouldAcceptDrop,
								typeBoolean,
								sizeof(acceptDrop),
								&acceptDrop);
			
			
			Target->OnDragEnter();
			result = noErr;
			break;
		}
		case kEventControlDragWithin:
		{
			DragParams p(v, Drag, NULL);

			// Call the handler
			int Accept = Target->WillAccept(p.Formats, p.Pt, p.KeyState);
			if (Accept)
			{
				v->d->AcceptedDropFormat.Reset(NewStr(p.Formats.First()));
				LgiAssert(v->d->AcceptedDropFormat.Get());
			}
			printf("kEventControlDragWithin %ix%i accept=%i class=%s\n",
				p.Pt.x, p.Pt.y,
				Accept,
				v->GetClass());
			SetDragDropAction(Drag, p.Map(Accept));
			result = noErr;
			break;
		}
		case kEventControlDragLeave:
		{
			printf("kEventControlDragLeave\n");

			Target->OnDragExit();
			result = noErr;
			break;
		}
		case kEventControlDragReceive:
		{
			printf("kEventControlDragReceive\n");
			
			int Accept = 0;
			if (v->d->AcceptedDropFormat)
			{
				DragParams p(v, Drag, v->d->AcceptedDropFormat);
				int Accept = Target->OnDrop(v->d->AcceptedDropFormat,
											&p.Data,
											p.Pt,
											p.KeyState);
				SetDragDropAction(Drag, p.Map(Accept));
			}
			else
			{
				printf("%s:%i - No accepted drop format. (view=%s)\n", _FL, v->GetClass());
				SetDragDropAction(Drag, kDragActionNothing);
			}
			result = noErr;
			break;
		}
	}
	
	return result;
}

void GView::_Delete()
{
	if (_Over == this) _Over = 0;
	if (_Capturing == this) _Capturing = 0;

	GWindow *w = GetWindow();
	if (w && w->GetFocus() == this)
		w->SetFocus(this, GWindow::ViewDelete);

	if (LgiApp && LgiApp->AppWnd == this)
	{
		LgiApp->AppWnd = 0;
	}

	SetPulse();
	Pos.ZOff(-1, -1);

	GViewI *c;
	while ((c = Children.First()))
	{
		if (c->GetParent() != (GViewI*)this)
		{
			printf("Error: ~GView, child not attached correctly: %p(%s) Parent: %p(%s)\n",
				c, c->Name(),
				c->GetParent(), c->GetParent() ? c->GetParent()->Name() : "");
			Children.Delete(c);
		}

		DeleteObj(c);
	}

	Detach();
	
	if (_View)
	{
		CFRelease(_View);
		_View = 0;
	}
}

GView *&GView::PopupChild()
{
	return d->Popup;
}

GViewI *_Find(GViewI *v, OsView h)
{
	if (v->Handle() == h)
		return v;
	
	GViewI *r = 0;
	GViewIterator *i = v->IterateViews();
	if (i)
	{
		for (GViewI *c = i->First(); c && !r; c = i->Next())
		{
			r = _Find(c, h);
		}
		DeleteObj(i);
	}
	
	return r;
}

int _Index = 0;

void _Dump(int idx, GViewI *root, HIViewRef v, int ox, int oy, int depth)
{
	for (int i=0; i<depth*4; i++)
		printf(" ");

	int MyIdx = _Index++;
	GViewI *c = _Find(root, v);
	HIRect r;
	HIViewGetFrame(v, &r);
	int rx = (int)r.origin.x+ox;
	int ry = (int)r.origin.y+oy;
	HIViewFeatures f;
	HIViewGetFeatures(v, &f);
	bool op = (f & kHIViewIsOpaque) != 0;
	
	#if 1
	printf("%p - %i,%i,%i,%i, opaque=%i class=%s vis=%i\n",
		v, (int)r.origin.x, (int)r.origin.y, (int)r.size.width, (int)r.size.height,
		op, c ? c->GetClass() : "",
		IsControlVisible(v));
	#else	
	printf("View *v%i = new View(v%i->Hnd, %i, %i, %i, %i, 128, 128, 128 + k(%i), \"%s\");\n",
		MyIdx, idx,
		(int)r.origin.x, (int)r.origin.y, (int)r.size.width, (int)r.size.height,
		MyIdx,
		c ? c->GetClassName() : "");
	#endif
	
	for (HIViewRef c = HIViewGetFirstSubview(v); c; c = HIViewGetNextView(c))
	{
		_Dump(MyIdx, root, c, rx, ry, depth + 1);
	}
}

void _Dump2(GViewI *root, HIViewRef v, int depth)
{
	for (int i=0; i<depth*4; i++)
		printf(" ");

	GViewI *c = _Find(root, v);
	HIRect r;
	HIViewGetFrame(v, &r);
	HIViewFeatures f;
	HIViewGetFeatures(v, &f);
	bool op = (f & kHIViewIsOpaque) != 0;
	
	printf("%s - view=%p hnd=%p p=%i,%i,%i,%i vis=%i opaque=%i\n",
		c && c->GetClass() ? c->GetClass() : (char*)"",
		c, v,
		(int)r.origin.x, (int)r.origin.y, (int)r.size.width, (int)r.size.height,
		IsControlVisible(v),
		op);
	
	for (HIViewRef c = HIViewGetFirstSubview(v); c; c = HIViewGetNextView(c))
	{
		_Dump2(root, c, depth + 1);
	}
}

LgiCursor GView::GetCursor(int x, int y)
{
	return LCUR_Normal;
}

static bool SetCarbonCursor(LgiCursor CursorId)
{
	ThemeCursor MacCursor = kThemeArrowCursor;
	switch (CursorId)
	{
		default:
		case LCUR_SizeBDiag:
		case LCUR_SizeFDiag:
		case LCUR_SizeAll:
		case LCUR_Blank:
		case LCUR_SplitV:
		case LCUR_SplitH:
		case LCUR_Normal:
			MacCursor = kThemeArrowCursor; break;
		case LCUR_UpArrow:
			MacCursor = kThemeResizeUpCursor; break;
		case LCUR_Cross:
			MacCursor = kThemeCrossCursor; break;
		case LCUR_Wait:
			MacCursor = kThemeSpinningCursor; break;
		case LCUR_Ibeam:
			MacCursor = kThemeIBeamCursor; break;
		case LCUR_SizeVer:
			MacCursor = kThemeResizeUpDownCursor; break;
		case LCUR_SizeHor:
			MacCursor = kThemeResizeLeftRightCursor; break;
		case LCUR_PointingHand:
			MacCursor = kThemePointingHandCursor; break;
		case LCUR_Forbidden:
			MacCursor = kThemeNotAllowedCursor; break;
		case LCUR_DropCopy:
			MacCursor = kThemeCopyArrowCursor; break;
		case LCUR_DropMove:
			MacCursor = kThemeAliasArrowCursor; break;
	}
	
	SetThemeCursor(MacCursor);
	return true;
}

bool GView::_Mouse(GMouse &m, bool Move)
{
	#if 0
	if (!Move)
	{
		static bool First = true;
		if (First)
		{
			First = false;
			//_DumpHeirarchy(GetWindow());
			//_Dump(0, GetWindow(), HIViewGetRoot(GetWindow()->WindowHandle()));
		}
		
		printf("_Mouse %p,%s %i,%i down=%i move=%i\n",
			this, GetClass(),
			m.x, m.y,
			m.Down(), Move);
	}
	#endif

	GWindow *Wnd = GetWindow();

	if (_Capturing)
	{
		if (_Capturing != m.Target)
		{
			m.ToScreen();
			m.Target = _Capturing;
			m.ToView();
		}
		
		if (Move)
		{
			GViewI *c = _Capturing;
			SetCarbonCursor(c->GetCursor(m.x, m.y));
			c->OnMouseMove(m);
		}
		else
		{
			if (!Wnd || Wnd->HandleViewMouse(dynamic_cast<GView*>(m.Target), m))
			{
				m.Target->OnMouseClick(m);
			}
		}
	}
	else
	{
		if (_Over != this)
		{
			if (_Over)
			{
				_Over->OnMouseExit(m);
			}

			_Over = this;

			if (_Over)
			{
				_Over->OnMouseEnter(m);
			}
		}
			
		GView *Target = dynamic_cast<GView*>(_Over ? _Over : this);
		GLayout *Lo = dynamic_cast<GLayout*>(Target);
		GRect Client = Lo ? Lo->GetClient(false) : Target->GView::GetClient(false);

		if (!Client.Valid() || Client.Overlap(m.x, m.y))
		{
			if (Move)
			{
				SetCarbonCursor(Target->GetCursor(m.x, m.y));
				Target->OnMouseMove(m);
			}
			else
			{
				Target->OnMouseClick(m);
			}
		}
		else return false;
	}
	
	return true;
}

GRect &GView::GetClient(bool ClientSpace)
{
	int Edge = (Sunken() || Raised()) ? _BorderSize : 0;

	static GRect c;

	c = Pos;
	c.Offset(-c.x1, -c.y1);
	if (ClientSpace)
	{
		c.x2 -= Edge << 1;
		c.y2 -= Edge << 1;
	}
	else
	{
		c.Size(Edge, Edge);
	}
	
	return c;
}

void GView::Quit(bool DontDelete)
{
	if (DontDelete)
	{
		Visible(false);
	}
	else
	{
		Detach();
		delete this;
	}
}

bool GView::SetPos(GRect &p, bool Repaint)
{
	Pos = p;

	if (_View)
	{
		int o = 0;
		
		if (d->Parent && (d->Parent->Sunken() || d->Parent->Raised()))
		{
			GView *p = dynamic_cast<GView*>(d->Parent);
			if (p)
				o = p->_BorderSize;
		}
		
		MoveControl(_View, Pos.x1 + o, Pos.y1 + o);
		SizeControl(_View, Pos.X(), Pos.Y());
	}
	else if (GetParent())
		OnPosChange();

	return true;
}

bool GView::Invalidate(GRect *r, bool Repaint, bool Frame)
{
	for (GViewI *v = this; v; v = v->GetParent())
	{
		if (!v->Visible())
			return true;
	}
	
	if (r && !r->Valid())
		return false;

	#if 1
	
	if (_View)
	{
		HIViewSetNeedsDisplay(_View, true);
		return true;
	}
	
	#else
	
// printf("Inval %p,%s,%p r=%s repaint=%i\n", this, GetClassName(), _View, r?r->GetStr():0, Repaint);
	if (_View || WindowHandle())
	{
		GRect Client = Frame ? GView::GetPos() : GView::GetClient();
		HIViewRef ViewRef = 0;
		if (WindowHandle())
		{
			OSStatus e = HIViewFindByID(HIViewGetRoot(WindowHandle()), kHIViewWindowContentID, &ViewRef);
			if (e)
			{
				LgiTrace("%s:%i - Getting content view failed (e=%i)\n", __FILE__, __LINE__, e);
			}
		}
		else
		{
			ViewRef = _View;
		}
		
		if (Repaint)
		{
			static bool Repainting = false;
			if (!Repainting)
			{
				Repainting = true;
				if (r)
				{
					GRect cr = *r;
					cr.Offset(Client.x1, Client.y1);
					
					RgnHandle a = NewRgn();
					SetRectRgn(a, cr.x1, cr.y1, cr.x2, cr.y2);
					OSStatus e = HIViewSetNeedsDisplayInRegion(ViewRef, a, true);
					DisposeRgn(a);
					if (e)
					{
						LgiTrace("%s:%i - HIViewSetNeedsDisplay failed (%i)\n", __FILE__, __LINE__, e);
					}
					else
					{
						printf("HIViewSetNeedsDisplayInRegion(%s) on %s\n", r->GetStr(), GetClassName());
					}
				}
				else
				{
					Rect a = GetClient();
					OSStatus e = HIViewSetNeedsDisplay(ViewRef, true);
					if (e)
					{
						LgiTrace("%s:%i - HIViewSetNeedsDisplay failed (%i)\n", __FILE__, __LINE__, e);
					}
					else
					{
						printf("HIViewSetNeedsDisplay() on %s\n", GetClassName());
					}
				}
				
				// HiViewRender(ViewRef);
				
				Repainting = false;
			}
		}
		else
		{
			if (r)
			{
				GRect cr = *r;
				cr.Offset(Client.x1, Client.y1);
				Rect a = cr;
				OSStatus e = HIViewSetNeedsDisplay(ViewRef, true);

				if (!e)
				{
					printf("HIViewSetNeedsDisplay() on %s\n", GetClassName());
				}
			}
			else
			{
				OSStatus e = HIViewSetNeedsDisplay(ViewRef, true);

				if (!e)
				{
					printf("HIViewSetNeedsDisplay() on %s\n", GetClassName());
				}
			}
		}
		
		return true;
	}
	#endif

	else
	{
		GRect Up;
		GViewI *p = this;

		if (r)
		{
			Up = *r;
		}
		else
		{
			Up.Set(0, 0, Pos.X()-1, Pos.Y()-1);
		}

		while (p && !dynamic_cast<GWindow*>(p) && !p->Handle())
		{
			GRect r = p->GetPos();
			Up.Offset(r.x1, r.y1);
			p = p->GetParent();
		}

		if (p && p->Handle())
		{
			LgiAssert(p != this);
				
			return p->Invalidate(&Up, Repaint);
		}
	}

	return false;
}

void GView::SetPulse(int Length)
{
	if (d->Pulse)
	{
		d->Pulse->Delete();
		d->Pulse = 0;
	}
	
	if (Length > 0)
	{
		d->Pulse = new GPulseThread(this, Length);
	}
}

int GView::OnEvent(GMessage *Msg)
{
	switch (Msg->m)
	{
		case M_PULSE:
		{
			OnPulse();
			break;
		}
		case M_CHANGE:
		{
			GViewI *Ctrl;
			if (GetViewById(MsgA(Msg), Ctrl))
			{
				return OnNotify(Ctrl, MsgB(Msg));
			}
			break;
		}
		case M_COMMAND:
		{
			return OnCommand(MsgA(Msg), 0, (OsView) MsgB(Msg));
		}
		default:
		{
			break;
		}
	}

	return 0;
}

void GView::PointToScreen(GdcPt2 &p)
{
	GViewI *c = this;
	int Ox = 0, Oy = 0;

	// Find offset to window
	while (c && c != c->GetWindow())
	{
		#if 0
		GRect cli = c->GetClient(false);
		GRect pos = c->GetPos(); 
		Ox += pos.x1 + cli.x1;
		Oy += pos.y1 + cli.y1;
		#else
		GRect pos = c->GetPos(); 
		Ox += pos.x1;
		Oy += pos.y1;
		#endif
		
		c = c->GetParent();
	}
	
	if (c && c->WindowHandle())
	{
		Rect r;
		GetWindowBounds(c->WindowHandle(), kWindowContentRgn, &r);
		p.x += r.left + Ox;
		p.y += r.top + Oy;
	}
	else
	{
		printf("%s:%i - No window handle to map to screen. c=%p\n", __FILE__, __LINE__, c);
	}
}

void GView::PointToView(GdcPt2 &p)
{
	GViewI *c = this;
	int Ox = 0, Oy = 0;

	// Find offset to window
	while (c && c != c->GetWindow())
	{
		// GView *gv = c->GetGView();
		// GRect cli = gv ? gv->GView::GetClient(false) : c->GetClient(false);
		GRect cli = c->GetClient(false);
		GRect pos = c->GetPos();

		Ox += pos.x1 + cli.x1;
		Oy += pos.y1 + cli.y1;

		c = c->GetParent();
	}
	
	// Apply window position
	if (c && c->WindowHandle())
	{
		Rect r;
		GetWindowBounds(c->WindowHandle(), kWindowContentRgn, &r);
		p.x -= r.left + Ox;
		p.y -= r.top + Oy;
		
		// printf("Pt2View o=%i,%i r=%i,%i\n", Ox, Oy, (int)r.left, (int)r.top);
	}
	else
	{
		printf("%s:%i - No window handle to map to view. c=%p\n", __FILE__, __LINE__, c);
	}
}

bool GView::GetMouse(GMouse &m, bool ScreenCoords)
{
	if (_View || !GetParent())
	{
        HIPoint p;
        HIGetMousePosition(kHICoordSpaceScreenPixel, NULL, &p);
        int MouseX = (int)p.x;
        int MouseY = (int)p.y;
		
		if (ScreenCoords)
		{
			m.x = MouseX;
			m.y	= MouseY;
		}
		else
		{
			GdcPt2 n(MouseX, MouseY);
			PointToView(n);
			m.x = n.x;
			m.y = n.y;
		}

		m.SetModifer(GetCurrentKeyModifiers());
		m.SetButton(GetCurrentButtonState());

		m.ViewCoords = !ScreenCoords;
		m.Target = this;

		return true;
	}
	else if (GetParent())
	{
		bool s = GetParent()->GetMouse(m, ScreenCoords);
		if (s)
		{
			if (!ScreenCoords)
			{
				m.x -= Pos.x1;
				m.y -= Pos.y1;
			}
			return true;
		}
	}
	
	return false;
}

bool GView::IsAttached()
{
	if (GetWindow() == this)
	{
		return WindowHandle() != 0;
	}

	if (Handle() && d->Parent)
	{
		HIViewRef p = HIViewGetSuperview(Handle());
		
		if (p && d->Parent->Handle())
		{
			if (d->Parent->Handle() != p)
			{
				LgiTrace("Error: parent handle mismatch for %s (%p vs %p::%s)\n",
					GetClass(), p, d->Parent->Handle(), d->Parent->GetClass());
				return false;
			}
		}
		
		return p != 0;
	}
	
	return false;
}

/*
void GView::OnNcPaint(GSurface *pDC, GRect &r)
{
	int Border = Sunken() || Raised() ? _BorderSize : 0;
	if (Border == 2)
	{
		LgiWideBorder(pDC, r, Sunken() ? SUNKEN : RAISED);
	}
	else if (Border == 1)
	{
		LgiThinBorder(pDC, r, Sunken() ? SUNKEN : RAISED);
	}
	// r.Offset(Border, Border);
}
*/

void BuildTabStops(GArray<GViewI*> &Stops, GViewI *v)
{
	if (v && v->Visible())
	{
		if (v->GetTabStop() && v->Enabled())
			Stops.Add(v);

		GViewIterator *it = v->IterateViews();
		if (it)
		{
			for (GViewI *c = it->First(); c; c = it->Next())
			{
				BuildTabStops(Stops, c);
			}
			DeleteObj(it);
		}
	}
}

void NextTabStop(GViewI *v, int dir)
{
	GArray<GViewI*> Stops;
	BuildTabStops(Stops, v->GetWindow());
	int Idx = Stops.IndexOf(v);
	if (Idx >= 0)
		Idx += dir;
	else
		Idx = 0;
		
	if (Idx < 0) Idx = Stops.Length() - 1;
	else if (Idx >= Stops.Length()) Idx = 0;
	if (Idx < Stops.Length())
		Stops[Idx]->Focus(true);
}

void SetDefaultFocus(GViewI *v)
{
	GArray<GViewI*> Stops;
	BuildTabStops(Stops, v->GetWindow());
	
	if (Stops.Length())
	{
		int Set = -1;
		for (int i=0; i<Stops.Length(); i++)
		{
			if (Stops[i]->Focus())
			{
				Set = i;
				break;
			}
		}
		
		if (Set < 0)
			Set = 0;
	
		Stops[Set]->Focus(true);
	}
}

int VirtualKeyToLgi(UInt32 Virt)
{
	switch (Virt)
	{
		// various
		case 122: return VK_F1;
		case 120: return VK_F2;
		case 99: return VK_F3;
		case 118: return VK_F4;
		case 96: return VK_F5;
		case 97: return VK_F6;
		case 98: return VK_F7;
		case 100: return VK_F8;
		case 101: return VK_F9;
		case 109: return VK_F10;
		case 103: return VK_F11;
		case 110: return VK_APPS;
		case 111: return VK_F12;
		
		case 123: return VK_LEFT;
		case 124: return VK_RIGHT;
		case 125: return VK_DOWN;
		case 126: return VK_UP;
		case 114: return VK_INSERT;
		case 116: return VK_PAGEUP;
		case 121: return VK_PAGEDOWN;

		case 53: return VK_ESCAPE;
		case 51: return VK_BACKSPACE;
		case 117: return VK_DELETE;
		
		case 115: return VK_HOME;
		case 119: return VK_END;
		
		// whitespace
		case 76: return VK_RETURN;
		case 36: return '\r';
		case 48: return '\t';
		case 49: return ' ';
		
		// delimiters
		case 27: return '-';
		case 24: return '=';
		case 42: return '\\';
		case 47: return '.';
		case 50: return '`';
		case 43: return ',';
		
		// digits
		case 18: return '1';
		case 19: return '2';
		case 20: return '3';
		case 21: return '4';
		case 23: return '5';
		case 22: return '6';
		case 26: return '7';
		case 28: return '8';
		case 25: return '9';
		case 29: return '0';
		
		// alpha
		case 0: return 'a';
		case 11: return 'b';
		case 8: return 'c';
		case 2: return 'd';
		case 14: return 'e';
		case 3: return 'f';
		case 5: return 'g';
		case 4: return 'h';
		case 34: return 'i';
		case 38: return 'j';
		case 40: return 'k';
		case 37: return 'l';
		case 46: return 'm';
		case 45: return 'n';
		case 31: return 'o';
		case 35: return 'p';
		case 12: return 'q';
		case 15: return 'r';
		case 1: return 's';
		case 17: return 't';
		case 32: return 'u';
		case 9: return 'v';
		case 13: return 'w';
		case 7: return 'x';
		case 16: return 'y';
		case 6: return 'z';
		
		default:
			printf("%s:%i - unimplemented virt->lgi code mapping: %d\n", _FL, (unsigned)Virt);
			break;
	}
	
	return 0;
}

static int GetIsChar(GKey &k, int mods)
{
	k.IsChar =	(mods & 0x100) == 0 &&
	(
		k.c16 >= ' ' ||
		k.c16 == VK_RETURN ||
		k.c16 == VK_TAB ||
		k.c16 == VK_BACKSPACE
	);
}

struct WasteOfSpace
{
	HIViewRef Obj;
	GView *v;
};

pascal
OSStatus
CarbonControlProc
(
	EventHandlerCallRef nextHandler, 
	EventRef inEvent,
	void * userData
)
{
	OSStatus Status = eventNotHandledErr;
	UInt32 eventClass = GetEventClass(inEvent);
	UInt32 eventKind = GetEventKind(inEvent);
	
	WasteOfSpace *d = (WasteOfSpace*) userData;
	GView *v = d ? d->v : 0;

	switch (eventClass)
	{
		case kEventClassHIObject:
		{
			switch (eventKind)
			{
				case kEventHIObjectInitialize:
				{
					OSStatus e = GetEventParameter(inEvent, GViewThisPtr, typeVoidPtr, NULL, sizeof(v), NULL, &v);
					if (e)
					{
						printf("%s:%i - No GViewThisPtr prop.\n", _FL);
						return eventNotHandledErr;
					}
					
					d->v = v;
					v->_View = (OsView)d->Obj;
					
					int Offset = 0;
					GViewI *Par = v->GetParent();
					if (Par && (Par->Sunken() || Par->Raised()))
					{
						GView *View = dynamic_cast<GView*>(Par);
						if (View)
						{
							Offset = View->_BorderSize;
						}
					}
					
					GRect r = v->GetPos();
					MoveControl(v->Handle(), r.x1 + Offset, r.y1 + Offset);
					SizeControl(v->Handle(), r.X(), r.Y());
					break;
				}
				case kEventHIObjectConstruct:
				{
					d = new WasteOfSpace;
					if (d)
					{
						d->v = 0;

						OSStatus e = GetEventParameter(	inEvent,
														kEventParamHIObjectInstance,
														typeHIObjectRef,
														NULL,
														sizeof(HIObjectRef),
														NULL,
														&d->Obj);
						if (e)
						{
							printf("%s:%i - failed to get view ptr (e=%i).\n", _FL, (int)e);
							d->Obj = 0;
						}

						Status = SetEventParameter(inEvent, kEventParamHIObjectInstance, typeVoidPtr, sizeof(d), &d); 
					}
					break;
				}
				case kEventHIObjectDestruct:
				{
					v->_View = 0;
					d->v = 0;
					DeleteObj(d);
					Status = noErr;
					break;
				}
			}
			break;
		}		
		case kEventClassControl:
		{
			if (!v)
			{
				LgiTrace("%s:%i - No view ptr.\n", __FILE__, __LINE__);
				return Status;
			}

			switch (eventKind)
			{
				case kEventControlBoundsChanged:
				{
					v->OnPosChange();
					break;
				}
				case kEventControlInitialize:
				{
					UInt32 features = 0;
					Status = GetEventParameter(inEvent, kEventParamControlFeatures, typeUInt32, NULL, sizeof(features), NULL, &features);
					if (Status == noErr)
						features |= kControlSupportsEmbedding;
					else
						features = kControlSupportsEmbedding;
					
					//features |= kControlSupportsGetRegion;

					Status = SetEventParameter(inEvent, kEventParamControlFeatures, typeUInt32, sizeof features, &features);
					break;
				}
				case kEventControlSetCursor:
				{
					break;
				}
		  		case kEventControlDraw:
				{
					CGContextRef Ctx;
					
					Status = GetEventParameter(	inEvent,
												kEventParamCGContextRef,
												typeCGContextRef,
												NULL,
												sizeof(CGContextRef),
												NULL,
												&Ctx);
					if (!Status)
					{
						GScreenDC dc(v, Ctx);
						v->_Paint(&dc);
					}
					break;
				}
				case kEventControlHitTest:
				{
					HIPoint  pt;
					HIRect  bounds;

					Status = noErr;

					// the point parameter is in view-local coords.
					OSStatus status = GetEventParameter(inEvent, kEventParamMouseLocation, typeHIPoint, NULL, sizeof(pt), NULL, &pt);
					HIViewGetBounds(v->Handle(), &bounds);
					if (CGRectContainsPoint(bounds, pt))
					{
						ControlPartCode part = kControlButtonPart;
						status = SetEventParameter(inEvent, kEventParamControlPart, typeControlPartCode, sizeof(part), &part);
					}
					break;
				}
				case kEventControlHiliteChanged:
				{
					HIViewSetNeedsDisplay(v->Handle(), true);
					break;
				}
				case kEventControlGetFocusPart:
				{
					printf("kEventControlGetFocusPart ??\n");
					break;
				}
				case kEventControlSetFocusPart:
				{
					ControlPartCode Part;
					OSStatus e = GetEventParameter(inEvent, kEventParamControlPart, typeControlPartCode, NULL, sizeof(Part), NULL, &Part);
					if (e) printf("%s:%i - error (%i)\n", __FILE__, __LINE__, (int)e);
					
					bool f = Part != kControlFocusNoPart;

					/*
					GView *p = 0;
					if (dynamic_cast<GEdit*>(v))
					{
						GViewI *i = v->Children.First();
						p = i ? i->GetGView() : v;
					}
					else p = v;
					*/

					#if 0
					printf("Focus change id=%i %s (%s) %i -> %i\n",
						v->GetId(), v->GetClass(), p->GetClass(), TestFlag(p->WndFlags, GWF_FOCUS), f);
					#endif
					
					if (f) SetFlag(v->WndFlags, GWF_FOCUS);
					else ClearFlag(v->WndFlags, GWF_FOCUS);
					v->OnFocus(f);
					
					Status = noErr;
					break;
				}
				case kEventControlGetSizeConstraints:
				{
					HISize min, max;
					min.width = max.width = v->X();
					min.height = max.height = v->Y();

					Status = SetEventParameter( inEvent, kEventParamMinimumSize, typeHISize,
												sizeof( HISize ), &min );

					Status = SetEventParameter( inEvent, kEventParamMaximumSize, typeHISize,
												sizeof( HISize ), &max );
					Status = noErr ;
					break;
				}
			}
			break;
		}
		case kEventClassKeyboard:
		{
			/*
			kEventClassKeyboard quick reference:
			
				kEventRawKeyDown
				kEventRawKeyRepeat
				kEventRawKeyUp
				kEventRawKeyModifiersChanged
				kEventHotKeyPressed
				kEventHotKeyReleased
			*/

			switch (eventKind)
			{
				case kEventRawKeyDown:
				{
					UInt32		key = 0;
					UInt32		mods = 0;

					OSStatus e = GetEventParameter(inEvent, kEventParamKeyCode, typeUInt32, NULL, sizeof(UInt32), NULL, &key);
					if (e) printf("%s:%i - error %i\n", __FILE__, __LINE__, (int)e);
					e = GetEventParameter(inEvent, kEventParamKeyModifiers, typeUInt32, NULL, sizeof(mods), NULL, &mods);
					if (e) printf("%s:%i - error %i\n", __FILE__, __LINE__, (int)e);
					
					GKey k;
					if ((k.c16 = VirtualKeyToLgi(key)))
					{
						k.vkey = k.c16;
						k.Down(true);
						if (mods & 0x200) k.Shift(true);
						if (mods & 0x1000) k.Ctrl(true);
						if (mods & 0x800) k.Alt(true);
						if (mods & 0x100) k.System(true);

						if
						(
							k.c16 == VK_APPS ||
							k.c16 == VK_DELETE
						)
						{
							// printf("RawDown key=%i sh=%i,alt=%i,ctrl=%i v=%p\n", k.c16, k.Shift(), k.Alt(), k.Ctrl(), v->Handle());

							GWindow *Wnd = v->GetWindow();
							if (Wnd) Wnd->HandleViewKey(v, k);
							else v->OnKey(k);
						}
					}
					
					// Status = noErr;
					break;
				}
				case kEventRawKeyRepeat:
				{
					break;
				}
				case kEventRawKeyUp:
				{
					UInt32		key = 0;
					UInt32		mods = 0;

					OSStatus e = GetEventParameter(inEvent, kEventParamKeyCode, typeUInt32, NULL, sizeof(UInt32), NULL, &key);
					if (e) printf("%s:%i - error %i\n", __FILE__, __LINE__, (int)e);
					e = GetEventParameter(inEvent, kEventParamKeyModifiers, typeUInt32, NULL, sizeof(mods), NULL, &mods);
					if (e) printf("%s:%i - error %i\n", __FILE__, __LINE__, (int)e);
					
					GKey k;
					if ((k.c16 = VirtualKeyToLgi(key)))
					{
						k.vkey = k.c16;
						k.Down(false);
						if (mods & 0x200) k.Shift(true);
						if (mods & 0x1000) k.Ctrl(true);
						if (mods & 0x800) k.Alt(true);
						if (mods & 0x100) k.System(true);
						GetIsChar(k, mods);

						// printf("RawUp key=%i sh=%i,alt=%i,ctrl=%i\n", k.c16, k.Shift(), k.Alt(), k.Ctrl());
						
						GWindow *Wnd = v->GetWindow();
						if (Wnd) Wnd->HandleViewKey(v, k);
						else v->OnKey(k);
					}
					
					Status = noErr;
					break;
				}
			}
			
			break;
		}
		case kEventClassTextInput:
		{
			/*
			kEventClassTextInput quick reference:
				
				kEventTextInputUpdateActiveInputArea
				kEventTextInputUnicodeForKeyEvent
				kEventTextInputOffsetToPos
				kEventTextInputPosToOffset
				kEventTextInputShowHideBottomWindow
				kEventTextInputGetSelectedText
				kEventTextInputUnicodeText
				kEventTextInputFilterText
			*/
		
			switch (eventKind)
			{
				case kEventTextInputUnicodeForKeyEvent:
				{
					UniChar		*text;
					UInt32		actualSize;
					EventRef	src;
					UInt32		mods = 0;
					UInt32		key = 0;
	 
					OSStatus e = GetEventParameter(inEvent, kEventParamTextInputSendText, typeUnicodeText, NULL, 0, &actualSize, NULL);
					text = new UniChar[actualSize+1];
					e = GetEventParameter(inEvent, kEventParamTextInputSendText, typeUnicodeText, NULL, actualSize, NULL, text);
					e = GetEventParameter(inEvent, kEventParamTextInputSendKeyboardEvent, typeEventRef, NULL, sizeof(src), NULL, &src);
					if (e) printf("%s:%i - error %i\n", __FILE__, __LINE__, (int)e);
					else
					{
						e = GetEventParameter(src, kEventParamKeyModifiers, typeUInt32, NULL, sizeof(mods), NULL, &mods);
						if (e) printf("%s:%i - error %i\n", __FILE__, __LINE__, (int)e);
						e = GetEventParameter(src, kEventParamKeyCode, typeUInt32, NULL, sizeof(UInt32), NULL, &key);
						if (e) printf("%s:%i - error %i\n", __FILE__, __LINE__, (int)e);
					}
					
					GKey k;

					UniChar *utf = text;
					int size = actualSize;
					k.c16 = k.vkey = LgiUtf16To32(utf, size);
					// printf("char=%i\n", k.c16);
					
					switch (k.c16)
					{
						#define MapKey(v, kc) case v: k.c16 = kc; break
						MapKey(1, VK_HOME);
						MapKey(3, VK_RETURN);
						MapKey(4, VK_END);
						MapKey(5, VK_INSERT);
						MapKey(11, VK_PAGEUP);				
						MapKey(12, VK_PAGEDOWN);			
						MapKey(127, VK_DELETE);
						case 16:
						{
							int c = VirtualKeyToLgi(key);
							if (c) k.c16 = c;
							break;
						}
					}

					GetIsChar(k, mods);
					k.Down(true);
					if (mods & 0x200) k.Shift(true);
					if (mods & 0x1000) k.Ctrl(true);
					if (mods & 0x800) k.Alt(true);
					if (mods & 0x100) k.System(true);

					// sprintf("Key Txt=%i Len=%i Modifiers=%x\n", k.c16, actualSize, mods);

					bool Processed;
					GWindow *Wnd = v->GetWindow();
					if (Wnd) Processed = Wnd->HandleViewKey(v, k);
					else Processed = v->OnKey(k);
					DeleteArray(text);
					
					if (!Processed && k.c16 == VK_TAB)
					{
						NextTabStop(v, k.Shift() ? -1 : 1);
					}
					
					Status = noErr;
					break;
				}
			}
			break;
		}
		case kEventClassToolbarItem:
		{
			printf("got kEventClassToolbarItem\n");
			if (eventKind == kEventToolbarItemCreateCustomView)
			{
				printf("got kEventToolbarItemCreateCustomView\n");
				EventTargetRef myButtonEventTarget; 
				HIViewRef myButton; 
				Rect myButtonRect = {0,0,20,80}; 

				CreatePushButtonControl(NULL, &myButtonRect, 
					 CFSTR("Push!"), &myButton); 
				SetEventParameter (inEvent, kEventParamControlRef, 
                             typeControlRef, sizeof(myButton), &myButton); 
				myButtonEventTarget = GetControlEventTarget(myButton); 
				/*
				InstallEventHandler (myButtonEventTarget, 
						MyButtonEventHandler, 
						GetEventTypeCount(pushButtonEvents), 
						pushButtonEvents, NULL, NULL);
						*/
			}
			break;
		}
		case kEventClassScrollable:
		{
			if (eventKind == kEventScrollableGetInfo)
			{
				HISize size = {0, 0};
				HISize line = {20, 20};
				HIRect bounds = {{0, 0}, {0, 0}};
				HIPoint origin = {0, 0};

				if (v->_OnGetInfo(size, line, bounds, origin))
				{
					OSStatus e = SetEventParameter(inEvent, kEventParamImageSize,
						 typeHISize, sizeof( size ), &size );
					if (e) printf("%s:%i - SetEventParameter failed with %i\n", _FL, (int)e);
					/*
					e = SetEventParameter(inEvent, kEventParamViewSize,
						 typeHISize, sizeof( size ), &size );
					if (e) printf("%s:%i - SetEventParameter failed with %e\n", _FL, e);
					*/

					e = SetEventParameter(inEvent, kEventParamLineSize,
						 typeHISize, sizeof( line ), &line );
					if (e) printf("%s:%i - SetEventParameter failed with %i\n", _FL, (int)e);
					HIViewGetBounds(v->Handle(), &bounds);
					SetEventParameter(inEvent, kEventParamViewSize, typeHISize, sizeof(bounds.size), &bounds.size);
					SetEventParameter(inEvent, kEventParamOrigin, typeHIPoint, sizeof(origin), &origin);

					/*
					printf("%s:%i - got kEventScrollableGetInfo size=%g,%g bounds=%g,%g-%g,%g\n",
						_FL,
						size.width, size.height,
						bounds.origin.x, bounds.origin.y,
						bounds.size.width, bounds.size.height);
					*/
					Status = noErr;
				}
			}
			else if (eventKind == kEventScrollableScrollTo)
			{				
				if (v->GetGView())
				{
					HIPoint origin = {0, 0};
					GetEventParameter(inEvent, kEventParamOrigin, typeHIPoint, 0, sizeof(origin), 0, &origin);
					v->GetGView()->_OnScroll(origin);					
				}
				else printf("%s:%i - %s doesn't have a parent GView for _OnScroll.\n", _FL, v->GetClass());
				
				Status = noErr;
			}
			else
			{
				printf("%s:%i - Unknown scroll event '%4.4s'\n", _FL, (char*)&eventKind);
			}
			break;
		}
		case kEventClassUser:
		{
			if (eventKind == kEventUser)
			{
				GMessage m;
				
				OSStatus status = GetEventParameter(inEvent, kEventParamLgiEvent, typeUInt32, NULL, sizeof(UInt32), NULL, &m.m);
				status = GetEventParameter(inEvent, kEventParamLgiA, typeUInt32, NULL, sizeof(UInt32), NULL, &m.a);
				status = GetEventParameter(inEvent, kEventParamLgiB, typeUInt32, NULL, sizeof(UInt32), NULL, &m.b);

				// printf("Received event %x,%x,%x (%s:%i)\n", m.m, m.a, m.b, _FL);
				
				v->OnEvent(&m);
				
				Status = noErr;
			}
			break;
		}
		default:
		{
			printf("%s:%i - Unhandled event %d,%d\n", __FILE__, __LINE__, (unsigned)eventClass, (unsigned)eventKind);
			break;
		}
	}
	
	return Status;
}

HIObjectClassRef ViewClass = 0;
#define kLgiGViewClassID CFSTR("com.memecode.lgi.GView") 

OsView GView::_CreateCustomView()
{
	OsView Hnd = 0;
	OSStatus e;
	
	if (!ViewClass)
	{
		static EventTypeSpec LgiViewEvents[] =
		{
			{ kEventClassHIObject, kEventHIObjectConstruct },
			{ kEventClassHIObject, kEventHIObjectInitialize },
			{ kEventClassHIObject, kEventHIObjectDestruct },
			{ kEventClassControl, kEventControlInitialize },
			{ kEventClassControl, kEventControlHitTest },
			{ kEventClassControl, kEventControlHiliteChanged },
			{ kEventClassControl, kEventControlDraw },
			// { kEventClassControl, kEventControlGetPartRegion },
			// { kEventClassControl, kEventControlGetFocusPart },
			{ kEventClassControl, kEventControlSetFocusPart },
			{ kEventClassControl, kEventControlBoundsChanged },
			// { kEventClassControl, kEventControlSetCursor },
			{ kEventClassControl, kEventControlGetSizeConstraints },

			{ kEventClassTextInput, kEventTextInputUnicodeForKeyEvent },
			{ kEventClassKeyboard, kEventRawKeyUp },
			{ kEventClassKeyboard, kEventRawKeyDown },
			{ kEventClassUser, kEventUser },
			{ kEventClassScrollable, kEventScrollableGetInfo },
			{ kEventClassScrollable, kEventScrollableScrollTo }
		};
		
		e = HIObjectRegisterSubclass(	kLgiGViewClassID,
										kHIViewClassID,
										0,
										CarbonControlProc,
										GetEventTypeCount(LgiViewEvents),
										LgiViewEvents,
										0,
										&ViewClass);
		if (e)
		{
			LgiTrace("%s:%i - HIObjectRegisterSubclass failed with '%i'.\n", _FL, e);
			LgiExitApp();
		}
	}
	
	if (ViewClass)
	{
		EventRef Ev;
		e = CreateEvent(NULL,
						kEventClassHIObject,
						kEventHIObjectInitialize,
						GetCurrentEventTime(),
						0,
						&Ev);
		GView *Myself = this;
		e = SetEventParameter(Ev, GViewThisPtr, typeVoidPtr, sizeof(Myself), &Myself); 
											
		if ((e = HIObjectCreate(kLgiGViewClassID, Ev, (HIObjectRef*)&Hnd)))
		{
			printf("%s:%i - HIObjectCreate failed with %i\n", _FL, (int)e);
			Hnd = 0;
		}
		else
		{
			#if defined(kHIViewFeatureIsOpaque)
			HIViewChangeFeatures(Hnd, kHIViewFeatureIsOpaque, 0);
			#else
			HIViewChangeFeatures(Hnd, kHIViewIsOpaque, 0);
			#endif
		}
	}
	
	return Hnd;
}

bool GView::_Attach(GViewI *parent)
{
	bool Status = false;

	d->Parent = (d->ParentI = parent) ? parent->GetGView() : 0;

	LgiAssert(!_InLock);
	_Window = d->GetParent() ? d->GetParent()->GetWindow() : 0;
	if (_Window)
		_Lock = _Window->_Lock;
	
	if (d->GetParent() &&
		d->GetParent()->IsAttached())
	{
		GWindow *w = GetWindow();
		if (w)
		{
			OSStatus e;

			if (!_View)
				_View = _CreateCustomView();
				
			if (_View)
			{
				// Set the view position
				SetPos(Pos);				
				
				// Set the view id
				SetControlCommandID(_View, GetId());
				GViewI *Ptr = this;
				SetControlProperty(_View, 'meme', 'view', sizeof(Ptr), &Ptr);
								
				// Get the parent viewref
				HIViewRef Par = d->Parent->Handle();
				if (!Par)
				{
					// This view will connect to a top-level window
					WindowRef Wnd = d->Parent->WindowHandle();
					if (Wnd)
					{
						HIViewRef ViewRef = HIViewGetRoot(Wnd);
						if (ViewRef)
						{
							e = HIViewFindByID(ViewRef, kHIViewWindowContentID, &Par);
							if (e) LgiTrace("%s:%i - HIViewFindByID(%p,%p) failed with '%i'.\n", __FILE__, __LINE__, Par, _View, e);
						}
						else LgiTrace("%s:%i - HIViewGetRoot failed.\n", __FILE__, __LINE__);
					}
					else LgiTrace("%s:%i - No window handle.\n", __FILE__, __LINE__);
				}
				
				if (Par)
				{
					// Attach the view to the parent view
					e = HIViewAddSubview(Par, _View);
					if (e) LgiTrace("%s:%i - HIViewAddSubview(%p,%p) failed with '%i' (name=%s).\n",
									_FL, Par, _View, e, Name());
					else
					{
						OnCreate();
						OnAttach();

						// Move the view into position
						// Set flags and show it
						if (TestFlag(WndFlags, GWF_DISABLED))
						{
							DisableControl(_View);
						}
						if (TestFlag(WndFlags, GWF_VISIBLE))
						{
							OSErr e = HIViewSetVisible(_View, true);
							if (e) printf("%s:%i - SetControlVisibility failed %i\n", _FL, e);
						}

						Status = true;
					}
				}
				else printf("%s:%i - No parent to attach to\n", _FL);
			}
			else printf("%s:%i - HIObjectCreate failed (%i)\n", _FL, (int)e);
		}
		else printf("%s:%i - No window to attach to.\n", _FL);
	}
	else
	{
		// Virtual attach
        /*
		printf("Warning: Virtual Attach %s->%s\n", d->Parent ? d->Parent->GetClass() : 0, GetClass());
        */
		Status = true;
	}

	return Status;
}

bool GView::Attach(GViewI *parent)
{
	if (_Attach(parent))
	{
		if (d->Parent && !d->Parent->HasView(this))
		{
			d->Parent->AddView(this);
		}
		
        if (!dynamic_cast<GPopup*>(this))
			d->Parent->OnChildrenChanged(this, true);
		return true;
	}
	
	LgiTrace("%s:%i - Attaching '%s' failed.\n", _FL, GetClass());
	return false;
}

void DetachChildren(HIViewRef v)
{
	for (HIViewRef c = HIViewGetFirstSubview(v); c; c = HIViewGetNextView(c))
	{
		DetachChildren(c);
		HIViewRemoveFromSuperview(c);
	}
}

bool GView::Detach()
{
	bool Status = false;

	// Detach view
	if (d->Parent)
	{
		GWindow *wnd = GetWindow();
		if (wnd && wnd->GetFocus() == this)
			wnd->SetFocus(this, GWindow::ViewDelete);
		_Window = NULL;
		
		// Remove the view from the parent
		DetachChildren(_View);
		HIViewRemoveFromSuperview(_View);
		
		// Events
		int D = 0;
		while (d->Parent->HasView(this))
		{
			d->Parent->DelView(this);
			D++;
		}

		if (D > 1)
		{
			printf("%s:%i - Error: View %s(%p) was in %s(%p) list %i times.\n",
				_FL, GetClass(), this, d->Parent->GetClass(), d->Parent, D);
		}

        if (!dynamic_cast<GPopup*>(this))
			d->Parent->OnChildrenChanged(this, false);
		d->Parent = 0;
	}

	Status = true;

	return Status;
}

GViewI *GView::FindControl(OsView hCtrl)
{
	if (Handle() == hCtrl)
	{
		return this;
	}

	List<GViewI>::I it = Children.Start();
	for (GViewI *c = *it; c; c = *++it)
	{
		GViewI *Ctrl = c->FindControl(hCtrl);
		if (Ctrl)
		{
			return Ctrl;
		}
	}
	return 0;
}

void GView::DrawThemeBorder(GSurface *pDC, GRect &r)
{
	LgiAssert(0);
}

////////////////////////////////
uint32 CursorData[] = {
0x02020202, 0x02020201, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020102, 0x02020202, 0x02020202, 0x01010101, 0x01010101, 0x01010101, 0x02020202, 0x02020202, 0x02010101, 0x02010101, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02010102, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020102, 0x01020202, 0x02020202, 0x02020202, 0x02020202, 0x01010101, 0x01010101, 0x02020201, 0x02020202, 0x01010101, 0x01010101, 0x02020201, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020102, 0x02020202, 0x02020202, 0x02020202, 0x01020202, 0x02010101, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020201, 0x02020202, 0x02020202, 0x02020202, 0x02010102, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x01020202, 0x01010101, 0x02020202, 0x02020202, 0x02020101, 0x02020202, 
0x02020202, 0x02020202, 0x02020202, 0x02020101, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x01020202, 0x02020100, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02010001, 0x02020202, 0x02020202, 0x00000001, 0x00000000, 0x01000000, 0x02020202, 0x02020202, 0x01000001, 0x02010000, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x01000001, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020101, 0x01020202, 0x02020201, 0x02020202, 0x02020202, 0x00000102, 0x00000000, 0x02020201, 0x02020202, 0x00000001, 0x01000000, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02010001, 0x02020202, 0x02020202, 0x02020202, 0x01020202, 0x02010000, 0x02020202, 0x02020202, 0x02020202, 0x01020202, 0x02020100, 0x02020202, 0x02020202, 0x02020202, 0x01000001, 0x02020202, 0x02020202, 
0x02020202, 0x02020202, 0x00010102, 0x00000000, 0x02020101, 0x02020202, 0x02010001, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02010001, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x01020202, 0x02020100, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02010001, 0x02020202, 0x02020202, 0x00010001, 0x00010001, 0x01000001, 0x02020202, 0x02020202, 0x00010102, 0x02020101, 0x02020202, 0x02020202, 0x02020202, 0x01020202, 0x00000000, 0x02020201, 0x02020202, 0x02020202, 0x01020202, 0x02020100, 0x01020202, 0x02020100, 0x02020202, 0x02020202, 0x00010202, 0x00000000, 0x02020201, 0x02020202, 0x00000001, 0x02010000, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x01020202, 0x01000000, 0x02020202, 0x02020202, 0x02020202, 0x01020202, 0x02010000, 0x02020202, 0x02020202, 0x02020202, 
0x00010202, 0x02010000, 0x02020202, 0x02020202, 0x02020202, 0x01000001, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x00000001, 0x00000000, 0x02010000, 0x02020202, 0x01000001, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x01000001, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x00010202, 0x02010000, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02010001, 0x02020202, 0x02020202, 0x01000102, 0x00000100, 0x02010000, 0x02020202, 0x02020202, 0x00010202, 0x02020201, 0x02020202, 0x02020202, 0x02020202, 0x00010202, 0x00000000, 0x02020100, 0x02020202, 0x02020202, 0x00010202, 0x02020100, 0x01020202, 0x02010000, 0x02020202, 0x02020202, 0x01020202, 0x00000000, 0x02020201, 0x02020202, 0x00000001, 0x02020100, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x00010202, 0x00000000, 
0x02020201, 0x02020202, 0x02020202, 0x01020202, 0x02010000, 0x02020202, 0x02020202, 0x02020202, 0x00000102, 0x01000000, 0x02020202, 0x02020202, 0x02020202, 0x01000001, 0x02020202, 0x02020202, 0x02020202, 0x01020202, 0x00000000, 0x00010101, 0x01000000, 0x02020202, 0x00000001, 0x02020201, 0x02020202, 0x02020202, 0x02020202, 0x00000001, 0x02020201, 0x02020202, 0x02020202, 0x02020202, 0x00010202, 0x02010000, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02010001, 0x02020202, 0x02020202, 0x00010202, 0x00000001, 0x02020100, 0x02020202, 0x02020202, 0x00010202, 0x02020201, 0x02020202, 0x02020202, 0x02020202, 0x00000102, 0x00000000, 0x02010000, 0x02020202, 0x02020202, 0x00000102, 0x01010100, 0x01010101, 0x01000000, 0x02020202, 0x02020201, 0x00010202, 0x00000000, 0x02020201, 0x02020202, 
0x00000001, 0x02010000, 0x02020202, 0x02020201, 0x02020202, 0x02020202, 0x01010102, 0x01010001, 0x02020101, 0x02020202, 0x02020202, 0x01020201, 0x02010000, 0x02020102, 0x02020202, 0x02020202, 0x01010101, 0x01010100, 0x02020201, 0x02020202, 0x02020202, 0x01000001, 0x02020101, 0x02020202, 0x02020202, 0x00010202, 0x01010000, 0x01020202, 0x00000001, 0x02020201, 0x00000001, 0x02020100, 0x02020202, 0x02020202, 0x02020202, 0x00000001, 0x02020100, 0x02020202, 0x02020202, 0x02020202, 0x00000102, 0x01000000, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02010001, 0x02020202, 0x02020202, 0x01020202, 0x00000000, 0x02020201, 0x02020202, 0x02020202, 0x00010202, 0x02020201, 0x02020202, 0x02020202, 0x02020202, 0x01010101, 0x01000001, 0x01010101, 0x02020202, 0x02020202, 0x00000001, 0x00000000, 
0x00000000, 0x00000000, 0x02020201, 0x02020101, 0x00000102, 0x00000100, 0x02020201, 0x02020202, 0x01000001, 0x01000000, 0x01020202, 0x02020201, 0x02020202, 0x02020202, 0x02020201, 0x02010001, 0x02010202, 0x02020202, 0x01020202, 0x01020201, 0x02010000, 0x02010102, 0x02020202, 0x02020202, 0x01020202, 0x02020100, 0x02020202, 0x02020202, 0x02020202, 0x00000001, 0x01010000, 0x02020101, 0x02020202, 0x00000102, 0x01000000, 0x02020202, 0x00000102, 0x02020100, 0x00000001, 0x02010000, 0x02020202, 0x02020202, 0x02020202, 0x00000001, 0x02010000, 0x02020202, 0x02020202, 0x02020202, 0x00000102, 0x01000000, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02010001, 0x02020202, 0x02020202, 0x02020202, 0x01000001, 0x02020202, 0x02020202, 0x02020202, 0x00010202, 0x02020201, 0x02020202, 0x02020202, 
0x02020202, 0x02020202, 0x01000001, 0x02020202, 0x02020202, 0x02020202, 0x00000001, 0x00000000, 0x00000000, 0x00000000, 0x02020201, 0x02010001, 0x00000001, 0x00010201, 0x02020201, 0x02020202, 0x02010001, 0x00000001, 0x00010201, 0x02020201, 0x02020202, 0x01020202, 0x02020201, 0x02010001, 0x01010202, 0x02020202, 0x00010202, 0x01020201, 0x02010000, 0x01000102, 0x02020202, 0x02020202, 0x01020202, 0x02020100, 0x02020202, 0x02020202, 0x02010102, 0x00000001, 0x00000000, 0x02010000, 0x02020202, 0x00000102, 0x00000001, 0x02020201, 0x00010202, 0x02020100, 0x00000001, 0x01000000, 0x02020202, 0x02020202, 0x02020202, 0x00000001, 0x01000000, 0x02020202, 0x02020202, 0x02020202, 0x00000001, 0x00000000, 0x02020201, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02010001, 0x02020202, 0x02020202, 0x02020202, 
0x01000001, 0x02020202, 0x02020202, 0x02020202, 0x00010202, 0x02020201, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x01000001, 0x02020202, 0x02020202, 0x02020202, 0x00000102, 0x01010100, 0x01010101, 0x01000000, 0x02020202, 0x01000001, 0x01000000, 0x01020202, 0x02020201, 0x02020202, 0x02020101, 0x00000102, 0x00000100, 0x02020201, 0x02020202, 0x00010202, 0x02020201, 0x02010001, 0x00010202, 0x02020201, 0x00000102, 0x01010101, 0x01010000, 0x00000101, 0x02020201, 0x01010101, 0x01010101, 0x01010100, 0x01010101, 0x02020201, 0x01000001, 0x00000001, 0x00000000, 0x02010000, 0x02020202, 0x00000001, 0x00000101, 0x02020100, 0x00010202, 0x02010000, 0x00000001, 0x00000000, 0x02020201, 0x02020202, 0x02020202, 0x00000001, 0x00000000, 0x02020201, 0x02020202, 0x02020202, 0x00000001, 0x00000000, 0x02020201, 
0x02020202, 0x02020202, 0x01010102, 0x01010101, 0x01010001, 0x01010101, 0x02020101, 0x02020202, 0x01000001, 0x02020202, 0x02020202, 0x02020202, 0x00010202, 0x02020201, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x01000001, 0x02020202, 0x02020202, 0x02020202, 0x00010202, 0x02020100, 0x01020202, 0x02010000, 0x02020202, 0x00000001, 0x02010000, 0x02020202, 0x02020201, 0x02020202, 0x02020201, 0x00010202, 0x00000000, 0x02020201, 0x02020202, 0x00000102, 0x01010101, 0x01010001, 0x00010101, 0x02020100, 0x00000001, 0x00000000, 0x00000000, 0x00000000, 0x02020100, 0x00000001, 0x00000000, 0x00000000, 0x00000000, 0x02020201, 0x00000001, 0x00000000, 0x00000000, 0x02010000, 0x02020202, 0x01000001, 0x00010202, 0x02010000, 0x01020202, 0x02010000, 0x00000001, 0x00000000, 0x02020100, 0x02020202, 0x02020202, 
0x00000001, 0x00000000, 0x02020100, 0x02020202, 0x02020202, 0x01010101, 0x01010100, 0x02020201, 0x02020202, 0x02020202, 0x00000001, 0x00000000, 0x00000000, 0x00000000, 0x02010000, 0x02020202, 0x01000001, 0x02020202, 0x02020202, 0x02020202, 0x00010202, 0x02020201, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x01000001, 0x02020202, 0x02020202, 0x02020202, 0x01020202, 0x02020100, 0x01020202, 0x02020100, 0x02020202, 0x00000001, 0x02020100, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x01020202, 0x00000000, 0x02020201, 0x02020202, 0x00000001, 0x00000000, 0x00000000, 0x00000000, 0x02010000, 0x00000102, 0x01010101, 0x01010000, 0x00000101, 0x02020201, 0x00000001, 0x00000000, 0x00000000, 0x00000000, 0x02020201, 0x00000102, 0x00000000, 0x00000000, 0x02010000, 0x02020202, 0x01000001, 0x01020202, 
0x01000000, 0x01020202, 0x02010000, 0x00000001, 0x00000000, 0x02010000, 0x02020202, 0x02020202, 0x00000001, 0x00000000, 0x02010000, 0x02020202, 0x02020202, 0x01020202, 0x02020100, 0x02020202, 0x02020202, 0x02020202, 0x01010102, 0x01010101, 0x01010001, 0x01010101, 0x02020101, 0x01020202, 0x00000000, 0x02020201, 0x02020202, 0x02020202, 0x00010202, 0x02020201, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x01000001, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020101, 0x01020202, 0x02020201, 0x02020202, 0x00000001, 0x02010000, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x00010202, 0x00000000, 0x02020201, 0x02020202, 0x00000102, 0x01010101, 0x01010001, 0x00010101, 0x02020100, 0x00010202, 0x01020201, 0x02010000, 0x01000102, 0x02020202, 0x01010101, 0x01010101, 0x01010100, 0x01010101, 
0x02020201, 0x00010202, 0x00000000, 0x00000000, 0x02010000, 0x02020202, 0x01000001, 0x02020202, 0x00000001, 0x01020201, 0x02010000, 0x00000001, 0x01000000, 0x02010101, 0x02020202, 0x02020202, 0x00000001, 0x01000000, 0x02010101, 0x02020202, 0x02020202, 0x01020202, 0x02020100, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02010001, 0x02020202, 0x02020202, 0x00010202, 0x01000001, 0x02020100, 0x02020202, 0x02020202, 0x00010202, 0x02020201, 0x02020202, 0x02020202, 0x02020202, 0x01010101, 0x01000001, 0x01010101, 0x02020202, 0x02020202, 0x02020202, 0x02020102, 0x01020202, 0x02020202, 0x02020202, 0x00000001, 0x01000000, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x00000102, 0x00000000, 0x02020201, 0x02020202, 0x00010202, 0x02020201, 0x02010001, 0x00010202, 0x02020201, 0x01020202, 
0x01020201, 0x02010000, 0x02010102, 0x02020202, 0x02020202, 0x01020202, 0x02020100, 0x02020202, 0x02020202, 0x01020202, 0x00000000, 0x00000000, 0x02010000, 0x02020202, 0x00000001, 0x02020201, 0x00000102, 0x00010100, 0x02010000, 0x00000001, 0x01000001, 0x01020202, 0x01010101, 0x01010101, 0x00000001, 0x01000001, 0x01020202, 0x01010101, 0x01010101, 0x01020202, 0x02020100, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02010001, 0x02020202, 0x02020202, 0x01000102, 0x01000001, 0x02010001, 0x02020202, 0x02020202, 0x00010202, 0x02020201, 0x02020202, 0x02020202, 0x02020202, 0x00000102, 0x00000000, 0x02010000, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x01010101, 0x01010101, 0x02020201, 0x02020202, 0x02020202, 0x02020202, 0x01010101, 0x01010101, 
0x02020201, 0x02020202, 0x01020202, 0x02020201, 0x02010001, 0x01010202, 0x02020202, 0x02020202, 0x01020201, 0x02010000, 0x02020102, 0x02020202, 0x02020202, 0x01020202, 0x02020100, 0x02020202, 0x02020202, 0x02020202, 0x00000001, 0x00000000, 0x02020100, 0x02020202, 0x00000102, 0x02020201, 0x00010202, 0x00010000, 0x02020100, 0x01000001, 0x01000001, 0x01020202, 0x00000000, 0x01000000, 0x01000001, 0x01000001, 0x01020202, 0x00000000, 0x01000000, 0x01020202, 0x02020100, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02010001, 0x02020202, 0x02020202, 0x00010001, 0x00000000, 0x01000100, 0x02020202, 0x02020202, 0x00010202, 0x02020201, 0x02020202, 0x02020202, 0x02020202, 0x00010202, 0x00000000, 0x02020100, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 
0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020201, 0x02010001, 0x02010202, 0x02020202, 0x02020202, 0x01020202, 0x02010000, 0x02020202, 0x02020202, 0x02020202, 0x01010101, 0x01010100, 0x02020201, 0x02020202, 0x02020202, 0x00000102, 0x00000000, 0x02020201, 0x02020202, 0x00000102, 0x02020100, 0x01020202, 0x00000000, 0x02020100, 0x02010001, 0x00000102, 0x01020201, 0x01010000, 0x01000001, 0x02010001, 0x00000102, 0x01020201, 0x01010100, 0x01000001, 0x01020202, 0x02020100, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02010001, 0x02020202, 0x02020202, 0x00000001, 0x00000000, 0x01000000, 0x02020202, 0x02020202, 0x00010202, 0x02020201, 0x02020202, 0x02020202, 0x02020202, 0x01020202, 0x00000000, 
0x02020201, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x01010102, 0x01010001, 0x02020101, 0x02020202, 0x02020202, 0x01020202, 0x02010000, 0x02020202, 0x02020202, 0x02020202, 0x00000102, 0x01000000, 0x02020202, 0x02020202, 0x02020202, 0x01010202, 0x01010101, 0x02020202, 0x02020202, 0x00010202, 0x01010000, 0x01020202, 0x00000001, 0x02020201, 0x02020101, 0x00000102, 0x01020201, 0x00000100, 0x01000100, 0x02020101, 0x00000102, 0x01020201, 0x01000100, 0x01000100, 0x01020202, 0x02020101, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02010001, 0x02020202, 0x02020202, 0x01010101, 0x01010101, 0x01010101, 0x02020202, 
0x02020202, 0x00010102, 0x02020101, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x01000001, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x00010202, 0x00000000, 0x02020201, 0x02020202, 0x02020202, 0x01020202, 0x02010000, 0x02020202, 0x02020202, 0x02020202, 0x00010202, 0x02010000, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x01020202, 0x00000000, 0x00010101, 0x01000000, 0x02020202, 0x02020202, 0x00010202, 0x01020100, 0x00000100, 0x01000000, 0x02020202, 0x00010202, 0x01020100, 0x01000100, 0x01000100, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 
0x02020202, 0x02010001, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x01000001, 0x02010000, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02010102, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x01020202, 0x01000000, 0x02020202, 0x02020202, 0x02020202, 0x01020202, 0x02010101, 0x02020202, 0x02020202, 0x02020202, 0x01020202, 0x02020100, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x00000001, 0x00000000, 0x02010000, 0x02020202, 0x02020202, 0x00010202, 0x01020100, 0x00000100, 0x01000100, 0x02020202, 0x00010202, 0x01020100, 
0x01000100, 0x01000100, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02010001, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02010101, 0x02010101, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02010001, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020201, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x00010102, 0x00000000, 0x02020101, 0x02020202, 
0x02020202, 0x01020202, 0x01020201, 0x01010000, 0x01000001, 0x02020202, 0x01020202, 0x01020201, 0x01000100, 0x01000100, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020102, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020102, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 
0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x01020202, 0x01010101, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x01020202, 0x00000000, 0x01000000, 0x02020202, 0x02020202, 0x01020202, 0x00000000, 0x01000000, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 
0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x01020202, 0x01010101, 0x01010101, 0x02020202, 0x02020202, 0x01020202, 0x01010101, 0x01010101, 
};
