#include <stdio.h>

#include "Lgi.h"
#include "GDragAndDrop.h"
#include "GToken.h"
#include "GPopup.h"

extern void NextTabStop(GViewI *v, int dir);
extern void SetDefaultFocus(GViewI *v);

///////////////////////////////////////////////////////////////////////
class HookInfo
{
public:
	int Flags;
	GView *Target;
};

class GWindowPrivate
{
public:
	GWindow *Wnd;
	int Sx, Sy;
	bool Dynamic;
	GKey LastKey;
	GArray<HookInfo> Hooks;
	bool SnapToEdge;
	EventHandlerUPP EventUPP;
	GDialog *ChildDlg;
	bool DeleteWhenDone;
	bool InitVisible;
	DragTrackingHandlerUPP TrackingHandler;
	DragReceiveHandlerUPP ReceiveHandler;

	GMenu *EmptyMenu;

	GWindowPrivate(GWindow *wnd)
	{
		InitVisible = false;
		Wnd = wnd;
		TrackingHandler = 0;
		ReceiveHandler = 0;
		DeleteWhenDone = false;
		ChildDlg = 0;
		EventUPP = 0;
		Sx = Sy = -1;
		Dynamic = true;
		SnapToEdge = false;
		ZeroObj(LastKey);
		EmptyMenu = 0;
	}
	
	~GWindowPrivate()
	{
		if (EventUPP)
		{
			DisposeEventHandlerUPP(EventUPP);
		}
		DeleteObj(EmptyMenu);
	}
	
	int GetHookIndex(GView *Target, bool Create = false)
	{
		for (int i=0; i<Hooks.Length(); i++)
		{
			if (Hooks[i].Target == Target) return i;
		}
		
		if (Create)
		{
			HookInfo *n = &Hooks[Hooks.Length()];
			if (n)
			{
				n->Target = Target;
				n->Flags = 0;
				return Hooks.Length() - 1;
			}
		}
		
		return -1;
	}
};

///////////////////////////////////////////////////////////////////////
#define GWND_CREATE		0x0010000

GWindow::GWindow() :
	GView(0)
{
	d = new GWindowPrivate(this);
	_QuitOnClose = false;
	Wnd = 0;
	Menu = 0;
	VirtualFocusId = -1;
	_Default = 0;
	_Window = this;
	WndFlags |= GWND_CREATE;
	GView::Visible(false);

    _Lock = new GSemaphore;

	GRect pos(0, 50, 200, 100);
	Rect r = pos;
	
	OSStatus e = CreateNewWindow
		(
			kDocumentWindowClass,
			kWindowStandardDocumentAttributes |
				kWindowStandardHandlerAttribute |
				kWindowCompositingAttribute |
				kWindowLiveResizeAttribute,
			&r,
			&Wnd
		);
	if (e)
	{
		printf("%s:%i - CreateNewWindow failed (e=%i).\n", __FILE__, __LINE__, e); 
	}
}

GWindow::~GWindow()
{
	SetDragHandlers(false);
	
	if (LgiApp->AppWnd == this)
	{
		LgiApp->AppWnd = 0;
	}

	_Delete();
	
	if (Wnd)
	{
		DisposeWindow(Wnd);
		Wnd = 0;
	}

	DeleteObj(Menu);
	DeleteObj(d);
	DeleteObj(_Lock);
}

OSErr GWindowTrackingHandler(	DragTrackingMessage message,
								WindowRef theWindow,
								void * handlerRefCon,
								DragRef theDrag)
{
	GWindow *Wnd = (GWindow*) handlerRefCon;
	if (Wnd)
		return Wnd->HandlerCallback(&message, theDrag);
	
	return 0;
}

OSErr GWindowReceiveHandler(    WindowRef theWindow,
								void * handlerRefCon,
								DragRef theDrag)
{
	GWindow *Wnd = (GWindow*) handlerRefCon;
	if (Wnd)
	{
		return Wnd->HandlerCallback(0, theDrag);
	}
	
	return 0;
}

OSErr GWindow::HandlerCallback(DragTrackingMessage *tracking, DragRef theDrag)
{
	Point Ms, Pin;
	OSErr e = GetDragMouse(theDrag, &Ms, &Pin);
	if (e) return e;

	GdcPt2 p(Ms.h, Ms.v);
	PointToView(p);

	SInt16 modifiers = 0, mouseDownModifiers = 0, mouseUpModifiers = 0;
	GetDragModifiers(theDrag, &modifiers, &mouseDownModifiers, &mouseUpModifiers);

	OSErr Status = noErr;
	static bool LastCopy = false;
	bool IsCopy = (modifiers & 0x800) != 0;
	if ((tracking && *tracking == kDragTrackingEnterHandler) || IsCopy != LastCopy)
	{
		LastCopy = IsCopy;
		
		GAutoPtr<CGImg> Img;
		SysFont->Fore(Rgb24(0xff, 0xff, 0xff));
		SysFont->Transparent(true);
		GDisplayString s(SysFont, IsCopy?(char*)"Copy":(char*)"Move");

		GMemDC m;
		if (m.Create(s.X() + 12, s.Y() + 2, 32))
		{
			m.Colour(Rgb32(0x30, 0, 0xff));
			m.Rectangle();
			Img.Reset(new CGImg(&m));
			s.Draw(&m, 6, 0);

			/*
			m.Colour(0, 32);
			m.Rectangle();
			Img.Reset(new CGImg(&m));
			*/

			HIPoint Offset = { 16, 16 };
			if (Img)
			{
				e = SetDragImageWithCGImage(theDrag, *Img, &Offset, kDragDarkerTranslucency);
				if (e) printf("%s:%i - SetDragImageWithCGImage failed with %i\n", _FL, e);
			}
		}
	}
	
	GViewI *v = WindowFromPoint(p.x, p.y);
	// printf("%s:%o - Dropping... %i,%i v=%p, tracking=%i, modifiers=%x\n", _FL, p.x, p.y, v, *tracking, modifiers);
	if (v)
	{
		GView *gv = v->GetGView();
		if (gv)
		{
			GDragDropTarget *Target = 0;
			
			while (gv)
			{
				Target = gv->DropTargetPtr();
				if (!Target)
				{
					GViewI *p = gv->GetParent();
					gv = p ? p->GetGView() : 0;
				}
				else break;
			}

			if (Target)
			{
				#if 0
				printf("Got target:\n");
				gv = v->GetGView();
				while (gv)
				{
					Target = gv->DropTargetPtr();
					printf("\ttarget=%p class=%s name=%.20s\n", Target, gv->GetClass(), gv->Name());
					if (!Target)
					{
						GViewI *p = gv->GetParent();
						gv = p ? p->GetGView() : 0;
					}
					else break;
				}
				#endif

				GDragDropSource *Src = 0;
				List<char> Formats;

				UInt16 Items = 0;
				e = CountDragItems(theDrag, &Items);
				if (e) printf("CountDragItems=%i Items=%i\n", e, Items);
				DragItemRef ItemRef = 0;
				e = GetDragItemReferenceNumber(theDrag, 1, &ItemRef);
				if (e) printf("GetDragItemReferenceNumber=%i Ref=%i\n", e, ItemRef);
				UInt16 numFlavors = 0;
				e = CountDragItemFlavors(theDrag, ItemRef, &numFlavors);
				if (e) printf("CountDragItemFlavors=%i numFlavors=%i\n", e, numFlavors);
				for (int f=0; f<numFlavors; f++)
				{
					FlavorType Type = 0, LgiType;
					memcpy(&LgiType, LGI_LgiDropFormat, 4);
					
					e = GetFlavorType(theDrag, ItemRef, 1+f, &Type);
					if (!e)
					{
						#ifndef __BIG_ENDIAN__
						Type = LgiSwap32(Type);
						#endif
						
						/*
						char *sType = (char*) &Type;
						char *sLgiType = (char*) &LgiType;
						char *sTest = (char*) &Test;
						*/
						
						if (Type == LgiType)
						{
							Size sz = sizeof(Src);
							FlavorType t = Type;
							#ifndef __BIG_ENDIAN__
							t = LgiSwap32(t);
							#endif
							
							e = GetFlavorData(	theDrag,
												ItemRef,
												t,
												&Src,
												&sz,
												0);
							if (e)
							{
								Src = 0;
								printf("%s:%i - GetFlavorData('%04.4s') failed with %i\n", _FL, &Type, e);
							}
							else
							{
								Src->GetFormats(Formats);
								break;
							}
						}
						else
						{
							Formats.Insert(NewStr((char*)&Type, 4));
						}
					}
					else
					{
						printf("%s:%i - GetFlavorType failed with %i\n", __FILE__, __LINE__, e);
						break;
					}
				}

				GdcPt2 Pt(Ms.h, Ms.v);
				v->PointToView(Pt);

				if (tracking)
				{
					// printf("Tracking = %04.4s\n", tracking);
					if (*tracking == kDragTrackingLeaveHandler)
					{
						// noop
					}
					else if (*tracking == kDragTrackingLeaveWindow)
					{
						Target->OnDragExit();
						// printf("OnDragExit\n");
					}
					else
					{											
						if (*tracking == kDragTrackingEnterWindow)
						{
							Target->OnDragEnter();
							// printf("OnDragEnter\n");
						}

						Target->WillAccept(Formats, Pt, 0);
						// printf("WillAccept=%i\n", Effect);
					}
				}
				else // drop
				{
					GVariant Data;
					
					#if 0
					for (char *r=Formats.First(); r; r=Formats.Next())
						printf("'%s'\n", r);
					#endif
					
					int Effect = Target->WillAccept(Formats, Pt, 0);
					if (Effect)
					{
						char *Format = Formats.First();
						
						if (Src)
						{
							// Lgi -> Lgi drop
							if (Src->GetData(&Data, Format))
							{
								Target->OnDrop(	Format,
												&Data,
												Pt,
												modifiers & 0x800 ? LGI_EF_CTRL : 0);
							}
							else printf("%s:%i - GetData failed.\n", _FL);
						}
						else if (strlen(Format) == 4)
						{
							// System -> Lgi drop
							FlavorType f = (FlavorType) *((uint32*)Format);
							#ifndef __BIG_ENDIAN__
							f = LgiSwap32(f);
							#endif
							
							Size sz;
							if (!GetFlavorDataSize(theDrag, ItemRef, f, &sz))
							{
								char *Data = new char[sz+1];
								e = GetFlavorData(	theDrag,
													ItemRef,
													f,
													Data,
													&sz,
													0);
								GVariant v;
								
								if (f == typeFileURL)
								{
									Data[sz] = 0;
									v = Data;
								}
								else
								{
									v.SetBinary(sz, Data);
								}
								
								int Effect = Target->OnDrop(Format,
															&v,
															Pt,
															modifiers & 0x800 ? LGI_EF_CTRL : 0);
								if (!Effect)
								{
									Status = dragNotAcceptedErr;
								}
							}
							else LgiTrace("%s:%i - GetFlavorDataSize failed.\n", _FL);
						}
					}

					Target->OnDragExit();
				}
			}
			else
			{
				// printf("%s:%i - No drop target:\n", _FL);
				
				#if 0
				if (gv = v->GetGView())
				{
					static GView *Last = 0;
					if (gv != Last)
					{
						Last = gv;
						while (gv)
						{
							Target = gv->DropTargetPtr();
							if (!Target)
							{
								printf("\t%p %s %.20s\n", gv, gv->GetClassName(), gv->Name());
								GViewI *p = gv->GetParent();
								gv = p ? p->GetGView() : 0;
							}
							else break;
						}
					}
				}
				#endif
			}
		}
		else printf("%s:%i - No view.\n", _FL);
	}
	else printf("%s:%i - no window from point.\n", _FL);
	
	return Status;
}

void GWindow::SetDragHandlers(bool On)
{
	if (On)
	{
		if (Wnd)
		{
			OSErr e;
			if (!d->TrackingHandler)
			{
				e = InstallTrackingHandler
				(
					d->TrackingHandler = NewDragTrackingHandlerUPP(GWindowTrackingHandler),
					Wnd,
					this
				);
			}
			if (!d->ReceiveHandler)
			{
				e = InstallReceiveHandler
				(
					d->ReceiveHandler = NewDragReceiveHandlerUPP(GWindowReceiveHandler),
					Wnd,
					this
				);
			}
		}
		else LgiAssert(!"Can't set drap handlers without handle");
	}
	else
	{
		if (d->TrackingHandler)
		{
			if (Wnd)
				RemoveTrackingHandler(d->TrackingHandler, Wnd);
			else
				LgiAssert(!"Shouldnt there be a Wnd here?");
			DisposeDragTrackingHandlerUPP(d->TrackingHandler);
			d->TrackingHandler = 0;
		}
		if (d->ReceiveHandler)
		{
			if (Wnd)
				RemoveReceiveHandler(d->ReceiveHandler, Wnd);
			else
				LgiAssert(!"Shouldnt there be a Wnd here?");
			DisposeDragReceiveHandlerUPP(d->ReceiveHandler);
			d->ReceiveHandler = 0;
		}
	}
}

void GWindow::Quit(bool DontDelete)
{
	if (_QuitOnClose)
	{
		LgiCloseApp();
	}
	
	d->DeleteWhenDone = !DontDelete;
	if (Wnd)
	{
		SetDragHandlers(false);
		OsWindow w = Wnd;
		Wnd = 0;
		DisposeWindow(w);
	}
}

void GWindow::SetChildDialog(GDialog *Dlg)
{
	d->ChildDlg = Dlg;
}

bool GWindow::GetSnapToEdge()
{
	return d->SnapToEdge;
}

void GWindow::SetSnapToEdge(bool s)
{
	d->SnapToEdge = s;
}

void GWindow::OnFrontSwitch(bool b)
{
	if (b && d->InitVisible)
	{
		if (!IsWindowVisible(WindowHandle()))
		{
			ShowWindow(WindowHandle());
			SelectWindow(WindowHandle());
		}
		else if (IsWindowCollapsed(WindowHandle()))
		{
			CollapseWindow(WindowHandle(), false);
		}
	}
}

bool GWindow::Visible()
{
	if (Wnd)
	{
		return IsWindowVisible(Wnd);
	}
	
	return false;
}

void GWindow::Visible(bool i)
{
	if (Wnd)
	{
		if (i)
		{
			d->InitVisible = true;
			Pour();
			ShowWindow(Wnd);
			SetDefaultFocus(this);
		}
		else
		{
			HideWindow(Wnd);
		}
		
		OnPosChange();
	}
}

void GWindow::_SetDynamic(bool i)
{
	d->Dynamic = i;
}

void GWindow::_OnViewDelete()
{
	if (d->Dynamic)
	{
		delete this;
	}
}

bool GWindow::PostEvent(int Event, int a, int b)
{
	bool Status = false;

	if (Wnd)
	{
		EventRef Ev;
		OSStatus e = CreateEvent(NULL,
								kEventClassUser,
								kEventUser,
								0, // EventTime 
								kEventAttributeUserEvent,
								&Ev);
		if (e)
		{
			printf("%s:%i - CreateEvent failed with %i\n", __FILE__, __LINE__, e);
		}
		else
		{
			EventTargetRef t = GetWindowEventTarget(Wnd);
			
			e = SetEventParameter(Ev, kEventParamLgiEvent, typeUInt32, sizeof(Event), &Event);
			if (e) printf("%s:%i - error %i\n", __FILE__, __LINE__, e);
			e = SetEventParameter(Ev, kEventParamLgiA, typeUInt32, sizeof(a), &a);
			if (e) printf("%s:%i - error %i\n", __FILE__, __LINE__, e);
			e = SetEventParameter(Ev, kEventParamLgiB, typeUInt32, sizeof(b), &b);
			if (e) printf("%s:%i - error %i\n", __FILE__, __LINE__, e);
			
			// printf("Sending event to window %i,%i,%i\n", Event, a, b);

			#if 1
			
			e = SetEventParameter(Ev, kEventParamPostTarget, typeEventTargetRef, sizeof(t), &t);
			if (e) printf("%s:%i - error %i\n", __FILE__, __LINE__, e);
			e = PostEventToQueue(GetMainEventQueue(), Ev, kEventPriorityStandard);
			if (e) printf("%s:%i - error %i\n", __FILE__, __LINE__, e);
			Status = e == 0;
			
			#else

			e = SendEventToEventTarget(Ev, GetControlEventTarget(Wnd));
			if (e) printf("%s:%i - error %i\n", _FL, e);
			
			#endif

			ReleaseEvent(Ev);
		}
	}
	
	return Status;
}

#define InRect(r, x, y) \
	( (x >= r.left) && (y >= r.top) && (x <= r.right) && (y <= r.bottom) )

pascal OSStatus LgiWindowProc(EventHandlerCallRef inHandlerCallRef, EventRef inEvent, void *inUserData)
{
	OSStatus result = eventNotHandledErr;
	
	GView *v = (GView*)inUserData;
	if (!v) return result;
	
	UInt32 eventClass = GetEventClass( inEvent );
	UInt32 eventKind = GetEventKind( inEvent );

	switch (eventClass)
	{
		case kEventClassCommand:
		{
			if (eventKind == kEventProcessCommand)
			{
				GWindow *w = v->GetWindow();
				if (w)
				{
					HICommand command;
					GetEventParameter(inEvent, kEventParamDirectObject, typeHICommand, NULL, sizeof(command), NULL, &command);
					if (command.commandID != kHICommandSelectWindow)
					{
						// char *Cmd = (char*) &command.commandID;
						
						#if 1
						uint32 c = command.commandID;
						#ifndef __BIG_ENDIAN__
						c = LgiSwap32(c);
						#endif
						if (c != '0000')
							printf("%s:%i - Cmd='%04.4s'\n", _FL, &c);
						#endif

						extern int *ReturnFloatCommand;
						if (ReturnFloatCommand)
						{
							*ReturnFloatCommand = command.commandID;
							result = noErr;
						}
						else if (command.commandID == kHICommandQuit)
						{
							LgiCloseApp();
							result = noErr;
						}
						/*
						else if (command.commandID == kHICommandHide)
						{
							HideWindow(w->Wnd);
						}
						else if (command.commandID == kHICommandMinimizeWindow)
						{
							CollapseWindow(w->Wnd, true);
						}
						*/
						else if (command.commandID == kHICommandClose)
						{
							if (v->OnRequestClose(false))
							{
								DeleteObj(v);
							}
							result = noErr;
						}
						else
						{
							w->OnCommand(command.commandID, 0, 0);
						}
					}
				}
				// result = noErr;
			}
			break;
		}
		case kEventClassWindow:
		{
			switch (eventKind)
			{
				case kEventWindowDispose:
				{
					GWindow *w = v->GetWindow();
					v->OnDestroy();
					
					if (w->d->DeleteWhenDone)
					{
						w->Wnd = 0;
						DeleteObj(v);
					}
					
					result = noErr;
					break;
				}
				case kEventWindowClose:
				{
					if (v->OnRequestClose(false))
					{
						DeleteObj(v);
					}
					
					result = noErr;
					break;
				}
				case kEventWindowActivated:
				{
					GWindow *w = v->GetWindow();
					if (w)
					{
						GMenu *m = w->GetMenu();
						if (m)
						{
							OSStatus e = SetRootMenu(m->Handle());
							if (e)
							{
								printf("%s:%i - SetRootMenu failed (e=%i)\n", __FILE__, __LINE__, e);
							}
						}
						else
						{
							if (!w->d->EmptyMenu)
							{
								w->d->EmptyMenu = new GMenu;
							}

							if (w->d->EmptyMenu)
							{
								OSStatus e = SetRootMenu(w->d->EmptyMenu->Handle());
								if (e)
								{
									printf("%s:%i - SetRootMenu failed (e=%i)\n", __FILE__, __LINE__, e);
								}
							}
						}
					}
					break;
				}
				/*
				case kEventWindowShown:
				{
					printf("kEventWindowShown\n");
					CGrafPtr port = GetWindowPort(v->WindowHandle());
					CGContextRef Ctx;
					OSStatus e = QDBeginCGContext(port, &Ctx);
					if (e)
					{
						LgiTrace("%s:%i - QDBeginCGContext failed (%i)\n", __FILE__, __LINE__, e);
					}
					else
					{
						GScreenDC s(v->WindowHandle(), Ctx);
						v->_Paint(&s);
						CGContextSynchronize(Ctx);
						QDEndCGContext(port, &Ctx);
						result = noErr;
						
						printf("paint %s\n", v->GetClient().GetStr());
					}
					break;
				}
				*/
				case kEventWindowBoundsChanged:
				{
					// kEventParamCurrentBounds
					HIRect r;
					GetEventParameter(inEvent, kEventParamCurrentBounds, typeHIRect, NULL, sizeof(HIRect), NULL, &r);
					v->Pos.x1 = r.origin.x;
					v->Pos.y1 = r.origin.y;
					v->Pos.x2 = v->Pos.x1 + r.size.width - 1;
					v->Pos.y2 = v->Pos.y1 + r.size.height - 1;
					
					v->Invalidate();
					v->OnPosChange();
					result = noErr;
					break;
				}
				/*
				case kEventWindowCursorChange:
				{
					Point Mouse;
					UInt32 Modifiers;
					Rect Client;
					
					GetWindowBounds(v->WindowHandle(), kWindowContentRgn, &Client);
					GetEventParameter(inEvent, kEventParamMouseLocation, typeQDPoint, NULL, sizeof(Mouse), NULL, &Mouse);
					GetEventParameter(inEvent, kEventParamKeyModifiers, typeUInt32, NULL, sizeof(Modifiers), NULL, &Modifiers);

					if (!InRect(Client, Mouse.h, Mouse.v))
						break;

					Mouse.h -= Client.left;
					Mouse.v -= Client.top;
					GViewI *Over = v->WindowFromPoint(Mouse.h, Mouse.v);
					if (Over)
					{
						// printf("Initial %i,%i - ", Mouse.h, Mouse.v);
						for (GViewI *p = Over; p && p->GetParent(); p = p->GetParent())
						{
							GRect r = p->GetPos();
							Mouse.h -= r.x1;
							Mouse.v -= r.y1;
							// printf("%i,%i - ", Mouse.h, Mouse.v);
						}
						// printf("\n");
						
						int LgiCursor = Over->OnHitTest(Mouse.h, Mouse.v);
						ThemeCursor MacCursor = kThemeArrowCursor;
						switch (LgiCursor)
						{
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
						
						static ThemeCursor PrevCursor = kThemeArrowCursor;
						if (PrevCursor != MacCursor)
						{
							SetThemeCursor(PrevCursor = MacCursor);
						}
					}
					break;
				}
				*/
			}
			break;
		}
		case kEventClassMouse:
		{
			switch (eventKind)
			{
				case kEventMouseDown:
				{
					GWindow *Wnd = dynamic_cast<GWindow*>(v->GetWindow());
					if (Wnd AND !Wnd->d->ChildDlg)
					{
						OsWindow Hnd = Wnd->WindowHandle();
						if (!IsWindowActive(Hnd))
						{
							ProcessSerialNumber Psn;
							GetCurrentProcess(&Psn);
							SetFrontProcess(&Psn);
							
							SelectWindow(Hnd);
						}
					}
					// Fall thru
				}
				case kEventMouseUp:
				{
					UInt32		modifierKeys = 0;
					UInt32		Clicks = 0;
					Point		Pt;
					UInt16		Btn = 0;
					Rect		Client, Grow;
					
					GetWindowBounds(v->WindowHandle(), kWindowContentRgn, &Client);
					GetWindowBounds(v->WindowHandle(), kWindowGrowRgn, &Grow);
					
					// the point parameter is in view-local coords.
					OSStatus status = GetEventParameter (inEvent, kEventParamMouseLocation, typeQDPoint, NULL, sizeof(Point), NULL, &Pt);
					//if (status) printf("error(%i) getting kEventParamMouseLocation\n", status);
					status = GetEventParameter(inEvent, kEventParamKeyModifiers, typeUInt32, NULL, sizeof(modifierKeys), NULL, &modifierKeys);
					//if (status) printf("error(%i) getting kEventParamKeyModifiers\n", status);
					status = GetEventParameter(inEvent, kEventParamClickCount, typeUInt32, NULL, sizeof(Clicks), NULL, &Clicks);
					//if (status) printf("error(%i) getting kEventParamClickCount\n", status);
					status = GetEventParameter(inEvent, kEventParamMouseButton, typeMouseButton, NULL, sizeof(Btn), NULL, &Btn);
					//if (status) printf("error(%i) getting kEventParamMouseButton\n", status);

					if (InRect(Grow, Pt.h, Pt.v))
					{
						// printf("In resize box\n");
						break;				
					}
					if (!InRect(Client, Pt.h, Pt.v))
					{
						// printf("Not in client\n");
						break;				
					}
					
					GMouse m;
					m.ViewCoords = false;
					m.x = Pt.h; // - Client.left;
					m.y = Pt.v; // - Client.top;

					if (modifierKeys & 0x100) m.System(true);
					if (modifierKeys & 0x200) m.Shift(true);
					if (modifierKeys & 0x800) m.Alt(true);
					if (modifierKeys & 0x1000) m.Ctrl(true);

					m.Down(eventKind == kEventMouseDown);
					m.Double(m.Down() && Clicks > 1);
					if (Btn == kEventMouseButtonPrimary) m.Left(true);
					else if (Btn == kEventMouseButtonSecondary) m.Right(true);
					else if (Btn == kEventMouseButtonTertiary) m.Middle(true);
					
					#if 0
					printf("Client=%i,%i,%i,%i Pt=%i,%i\n",
						Client.left, Client.top, Client.right, Client.bottom,
						(int)Pt.h, (int)Pt.v);
					#endif
					
					#if 0
					printf("click %i,%i down=%i, left=%i right=%i middle=%i, ctrl=%i alt=%i shift=%i Double=%i\n",
						m.x, m.y,
						m.Down(), m.Left(), m.Right(), m.Middle(),
						m.Ctrl(), m.Alt(), m.Shift(), m.Double());
					#endif

					int Cx = m.x - Client.left;
					int Cy = m.y - Client.top;
					
					// extern bool Debug_WindowFromPoint;
					// Debug_WindowFromPoint = true;
					m.Target = v->WindowFromPoint(Cx, Cy);
					// Debug_WindowFromPoint = false;
					
					if (m.Target)
					{
						m.ToView();
						
						GView *v = m.Target->GetGView();
						if (v)
						{
							if (v->_Mouse(m, false))
								result = noErr;
						}
						else printf("%s:%i - Not a GView\n", __FILE__, __LINE__);
					}
					else printf("%s:%i - No target window for mouse event.\n", __FILE__, __LINE__);
					
					break;
				}
				case kEventMouseMoved:
				case kEventMouseDragged:
				{
					UInt32		modifierKeys;
					Point		Pt;
					Rect		Client;
					
					GetWindowBounds(v->WindowHandle(), kWindowContentRgn, &Client);

					// the point parameter is in view-local coords.
					OSStatus status = GetEventParameter(inEvent, kEventParamMouseLocation, typeQDPoint, NULL, sizeof(Point), NULL, &Pt);
					if (status) printf("error(%i) getting kEventParamMouseLocation\n", status);
					status = GetEventParameter(inEvent, kEventParamKeyModifiers, typeUInt32, NULL, sizeof(modifierKeys), NULL, &modifierKeys);
					if (status) printf("error(%i) getting kEventParamKeyModifiers\n", status);
					
					GMouse m;
					m.ViewCoords = false;
					m.x = Pt.h;
					m.y = Pt.v;

					if (modifierKeys & 0x100) m.System(true);
					if (modifierKeys & 0x200) m.Shift(true);
					if (modifierKeys & 0x800) m.Alt(true);
					if (modifierKeys & 0x1000) m.Ctrl(true);

					m.Down(eventKind == kEventMouseDragged);
					if (m.Down())
					{
						UInt32 Btn = GetCurrentEventButtonState();
						if (Btn == kEventMouseButtonPrimary) m.Left(true);
						else if (Btn == kEventMouseButtonSecondary) m.Right(true);
						else if (Btn == kEventMouseButtonTertiary) m.Middle(true);
					}
					
					#if 0
					printf("move %i,%i down=%i, left=%i right=%i middle=%i, ctrl=%i alt=%i shift=%i Double=%i\n",
						m.x, m.y,
						m.Down(), m.Left(), m.Right(), m.Middle(),
						m.Ctrl(), m.Alt(), m.Shift(), m.Double());
					#endif

					#if 1
					if (m.Target = v->WindowFromPoint(m.x - Client.left, m.y - Client.top))
					{
						m.ToView();

						GView *v = m.Target->GetGView();
						if (v)
						{
							v->_Mouse(m, true);
						}
					}
					#endif
					break;
				}
				case kEventMouseWheelMoved:
				{
					UInt32		modifierKeys;
					Point		Pt;
					Rect		Client;
					SInt32		Delta;
					GViewI		*Target;

					GetWindowBounds(v->WindowHandle(), kWindowContentRgn, &Client);
					
					OSStatus status = GetEventParameter(inEvent, kEventParamMouseLocation, typeQDPoint, NULL, sizeof(Point), NULL, &Pt);
					if (status) printf("error(%i) getting kEventParamMouseLocation\n", status);
					status = GetEventParameter(inEvent, kEventParamKeyModifiers, typeUInt32, NULL, sizeof(modifierKeys), NULL, &modifierKeys);
					if (status) printf("error(%i) getting kEventParamKeyModifiers\n", status);
					status = GetEventParameter(inEvent, kEventParamMouseWheelDelta, typeSInt32, NULL, sizeof(Delta), NULL, &Delta);
					if (status) printf("error(%i) getting kEventParamMouseWheelDelta\n", status);
					
					if (Target = v->WindowFromPoint(Pt.h - Client.left, Pt.v - Client.top))
					{
						GView *v = Target->GetGView();
						if (v)
						{
							v->OnMouseWheel(Delta < 0 ? 3 : -3);
						}
						else printf("%s:%i - no GView\n", __FILE__, __LINE__);
					}
					else printf("%s:%i - no target\n", __FILE__, __LINE__);
					break;
				}
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
				
				result = noErr;
			}
			break;
		}
	}
	
	return result;
}

pascal OSStatus LgiRootCtrlProc(EventHandlerCallRef inHandlerCallRef, EventRef inEvent, void *inUserData)
{
	OSStatus result = eventNotHandledErr;
	
	GView *v = (GView*)inUserData;
	if (!v)
	{
		LgiTrace("%s:%i - no gview\n", __FILE__, __LINE__);
		return result;
	}
	
	UInt32 eventClass = GetEventClass( inEvent );
	UInt32 eventKind = GetEventKind( inEvent );

	switch (eventClass)
	{
		case kEventClassControl:
		{
			switch (eventKind)
			{
				case kEventControlDraw:
				{
					CGContextRef Ctx = 0;
					result = GetEventParameter (inEvent,  
												kEventParamCGContextRef, 
												typeCGContextRef, 
												NULL, 
												sizeof(CGContextRef),
												NULL,
												&Ctx);
					if (Ctx)
					{
						GScreenDC s(v->GetWindow(), Ctx);
						v->_Paint(&s);
					}
					else
					{
						LgiTrace("%s:%i - No context.\n", __FILE__, __LINE__);
					}
					break;
				}
			}
			break;
		}
	}
	
	return result;
}

bool GWindow::Attach(GViewI *p)
{
	bool Status = false;
	
	if (Wnd)
	{
		if (GObject::Name())
			Name(GObject::Name());
		
		EventTypeSpec	WndEvents[] =
		{
			{ kEventClassCommand, kEventProcessCommand },
			
			{ kEventClassWindow, kEventWindowClose },
			{ kEventClassWindow, kEventWindowInit },
			{ kEventClassWindow, kEventWindowDispose },
			{ kEventClassWindow, kEventWindowBoundsChanged },
			{ kEventClassWindow, kEventWindowActivated },
			{ kEventClassWindow, kEventWindowShown },
			// { kEventClassWindow, kEventWindowCursorChange },
			
			{ kEventClassMouse, kEventMouseDown },
			{ kEventClassMouse, kEventMouseUp },
			{ kEventClassMouse, kEventMouseMoved },
			{ kEventClassMouse, kEventMouseDragged },
			{ kEventClassMouse, kEventMouseWheelMoved },
			
			{ kEventClassUser, kEventUser }

		};
		
		EventHandlerRef Handler = 0;
		OSStatus e = InstallWindowEventHandler(	Wnd,
												d->EventUPP = NewEventHandlerUPP(LgiWindowProc),
												GetEventTypeCount(WndEvents),
												WndEvents,
												(void*)this,
												&Handler);
		if (e)
		{
			LgiTrace("%s:%i - InstallEventHandler failed (%i)\n", _FL, e);
		}

		e = HIViewFindByID(HIViewGetRoot(Wnd), kHIViewWindowContentID, &_View);
		if (_View)
		{
			EventTypeSpec	CtrlEvents[] =
			{
				{ kEventClassControl, kEventControlDraw },
			};
			EventHandlerRef CtrlHandler = 0;
			e = InstallEventHandler(GetControlEventTarget(_View),
									NewEventHandlerUPP(LgiRootCtrlProc),
									GetEventTypeCount(CtrlEvents),
									CtrlEvents,
									(void*)this,
									&CtrlHandler);
			if (e)
			{
				LgiTrace("%s:%i - InstallEventHandler failed (%i)\n", _FL, e);
			}

			HIViewChangeFeatures(_View, kHIViewIsOpaque, 0);
		}

		Status = true;
		
		// Setup default button...
		if (!_Default)
		{
			_Default = FindControl(IDOK);
			if (_Default)
			{
				_Default->Invalidate();
			}
		}

		OnCreate();
		OnAttach();
		OnPosChange();

		// Set the first control as the focus...
		NextTabStop(this, 0);
	}
	
	return Status;
}

bool GWindow::OnRequestClose(bool OsShuttingDown)
{
	if (GetQuitOnClose())
	{
		LgiCloseApp();
	}

	return GView::OnRequestClose(OsShuttingDown);
}

bool GWindow::HandleViewMouse(GView *v, GMouse &m)
{
	for (int i=0; i<d->Hooks.Length(); i++)
	{
		if (d->Hooks[i].Flags & GMouseEvents)
		{
			if (!d->Hooks[i].Target->OnViewMouse(v, m))
			{
				return false;
			}
		}
	}
	
	return true;
}

#define DEBUG_KEYS			0

/*
	// Any window in a popup always gets the key...
	for (GView *p = v; p; p = p->GetParent())
	{
		GPopup *Popup;
		if (Popup = dynamic_cast<GPopup*>(p))
		{
			Status = v->OnKey(k);
			if (NOT Status)
			{
				if (k.c16 == VK_ESCAPE)
				{
					// Popup window (or child) didn't handle the key, and the user
					// pressed ESC, so do the default thing and close the popup.
					Popup->Cancelled = true;
					Popup->Visible(false);
				}
				else
				{
					#if DEBUG_KEYS
					printf("Popup ignores '%c' down=%i alt=%i ctrl=%i sh=%i\n", k.c16, k.Down(), k.Alt(), k.Ctrl(), k.Shift());
					#endif
					break;
				}
			}
			
			#if DEBUG_KEYS
			printf("Popup ate '%c' down=%i alt=%i ctrl=%i sh=%i\n", k.c16, k.Down(), k.Alt(), k.Ctrl(), k.Shift());
			#endif
			
			goto AllDone;
		}
	}
*/

bool GWindow::HandleViewKey(GView *v, GKey &k)
{
	bool Status = false;
	GViewI *Ctrl = 0;

	// Give key to popups
	if (LgiApp AND
		LgiApp->GetMouseHook() AND
		LgiApp->GetMouseHook()->OnViewKey(v, k))
	{
		goto AllDone;
	}

	// Allow any hooks to see the key...
	for (int i=0; i<d->Hooks.Length(); i++)
	{
		if (d->Hooks[i].Flags & GKeyEvents)
		{
			if (d->Hooks[i].Target->OnViewKey(v, k))
			{
				Status = true;
				
				#if DEBUG_KEYS
				printf("Hook ate '%c'(%i) down=%i alt=%i ctrl=%i sh=%i\n", k.c16, k.c16, k.Down(), k.Alt(), k.Ctrl(), k.Shift());
				#endif
				
				goto AllDone;
			}
		}
	}

	// Give the key to the window...
	if (v->OnKey(k))
	{
		#if DEBUG_KEYS
		printf("View ate '%c'(%i) down=%i alt=%i ctrl=%i sh=%i\n",
			k.c16, k.c16, k.Down(), k.Alt(), k.Ctrl(), k.Shift());
		#endif
		
		Status = true;
		goto AllDone;
	}
	
	// Window didn't want the key...
	switch (k.c16)
	{
		case VK_RETURN:
		{
			Ctrl = _Default;
			break;
		}
		case VK_ESCAPE:
		{
			Ctrl = FindControl(IDCANCEL);
			break;
		}
	}

	if (Ctrl AND Ctrl->Enabled())
	{
		if (Ctrl->OnKey(k))
		{
			Status = true;

			#if DEBUG_KEYS
			printf("Default Button ate '%c'(%i) down=%i alt=%i ctrl=%i sh=%i\n",
				k.c16, k.c16, k.Down(), k.Alt(), k.Ctrl(), k.Shift());
			#endif
			
			goto AllDone;
		}
	}

	if (Menu)
	{
		Status = Menu->OnKey(v, k);
		if (Status)
		{
			#if DEBUG_KEYS
			printf("Menu ate '%c' down=%i alt=%i ctrl=%i sh=%i\n", k.c16, k.Down(), k.Alt(), k.Ctrl(), k.Shift());
			#endif
		}
	}
	
	// Command+W closes the window... if it doesn't get nabbed earlier.
	if (k.Down() AND
		k.System() AND
		tolower(k.c16) == 'w')
	{
		// Close
		if (OnRequestClose(false))
		{
			delete this;
			return true;
		}
	}

AllDone:
	d->LastKey = k;

	return Status;
}


void GWindow::Raise()
{
	if (Wnd)
	{
		BringToFront(Wnd);
	}
}

GWindowZoom GWindow::GetZoom()
{
	return GZoomNormal;
}

void GWindow::SetZoom(GWindowZoom i)
{
}

GViewI *GWindow::GetDefault()
{
	return _Default;
}

void GWindow::SetDefault(GViewI *v)
{
	if (v AND
		v->GetWindow() == (GViewI*)this)
	{
		if (_Default != v)
		{
			GViewI *Old = _Default;
			_Default = v;

			if (Old) Old->Invalidate();
			if (_Default) _Default->Invalidate();
		}
	}
	else
	{
		_Default = 0;
	}
}

bool GWindow::Name(char *n)
{
	bool Status = GObject::Name(n);

	if (Wnd)
	{	
		CFStringRef s = CFStringCreateWithBytes(NULL, (UInt8*)n, strlen(n), kCFStringEncodingUTF8, false);
		if (s)
		{
			OSStatus e = SetWindowTitleWithCFString(Wnd, s);
			if (e)
			{
				printf("%s:%i - SetWindowTitleWithCFString failed (e=%i)\n", __FILE__, __LINE__, e);
			}
			else
			{
				Status = true;
			}
			
			CFRelease(s);
		}
		else
		{
			printf("%s:%i - CFStringCreateWithBytes failed.\n", __FILE__, __LINE__);
		}
	}

	return Status;
}

char *GWindow::Name()
{
	return GObject::Name();
}

GRect &GWindow::GetClient(bool ClientSpace)
{
	if (Wnd)
	{
		static GRect c;
		Rect r;
		OSStatus e = GetWindowBounds(Wnd, kWindowContentRgn, &r);
		if (!e)
		{
			c = r;
			c.Offset(-c.x1, -c.y1);
			return c;
		}
		else
		{
			printf("%s:%i - GetWindowBounds failed\n", __FILE__, __LINE__);
		}
	}
	
	return GView::GetClient(ClientSpace);
}

bool GWindow::SerializeState(GDom *Store, char *FieldName, bool Load)
{
	if (!Store OR !FieldName)
		return false;

	if (Load)
	{
		GVariant v;
		if (Store->GetValue(FieldName, v) AND v.Str())
		{
			GRect Position(0, 0, -1, -1);
			GWindowZoom State = GZoomNormal;

			GToken t(v.Str(), ";");
			for (int i=0; i<t.Length(); i++)
			{
				char *Var = t[i];
				char *Value = strchr(Var, '=');
				if (Value)
				{
					*Value++ = 0;

					if (stricmp(Var, "State") == 0)
						State = (GWindowZoom) atoi(Value);
					else if (stricmp(Var, "Pos") == 0)
						Position.SetStr(Value);
				}
				else return false;
			}
			
			if (Position.Valid())
			{
				int Sy = GdcD->Y();
				// Position.y2 = min(Position.y2, Sy - 50);
				SetPos(Position);
			}
			
			SetZoom(State);
		}
		else return false;
	}
	else
	{
		char s[256];
		GWindowZoom State = GetZoom();
		sprintf(s, "State=%i;Pos=%s", State, GetPos().GetStr());

		GVariant v = s;
		if (!Store->SetValue(FieldName, v))
			return false;
	}

	return true;
}

GRect &GWindow::GetPos()
{
	if (Wnd)
	{
		Rect r;
		OSStatus e = GetWindowBounds(Wnd, kWindowStructureRgn, &r);
		if (!e)
		{
			Pos = r;
		}
		else
		{
			printf("%s:%i - GetWindowBounds failed (e=%i)\n", __FILE__, __LINE__, e);
		}
	}

	return Pos;
}

bool GWindow::SetPos(GRect &p, bool Repaint)
{
	int x = GdcD->X();
	int y = GdcD->Y();

	GRect r = p;
	int MenuY = GetMBarHeight();

	if (r.y1 < MenuY)
		r.Offset(0, MenuY - r.y1);
	if (r.y2 > y)
		r.y2 = y - 1;
	if (r.X() > x)
		r.x2 = r.x1 + x - 1;

	Pos = r;
	if (Wnd)
	{
		Rect rc;
		rc = Pos;
		OSStatus e = SetWindowBounds(Wnd, kWindowStructureRgn, &rc);
		if (e) printf("%s:%i - SetWindowBounds error e=%i\n", _FL, e);
	}

	return true;
}

void GWindow::OnChildrenChanged(GViewI *Wnd, bool Attaching)
{
	Pour();
}

void GWindow::OnCreate()
{
}

void GWindow::OnPaint(GSurface *pDC)
{
	pDC->Colour(LC_MED, 24);
	pDC->Rectangle();
}

void GWindow::OnPosChange()
{
	GView::OnPosChange();

	if (d->Sx != X() OR	d->Sy != Y())
	{
		Pour();
		d->Sx = X();
		d->Sy = Y();
	}
}

#define IsTool(v) \
	( \
		dynamic_cast<GView*>(v) \
		AND \
		dynamic_cast<GView*>(v)->_IsToolBar \
	)

void GWindow::Pour()
{
	GRect r = GetClient();
	// printf("::Pour r=%s\n", r.GetStr());
	GRegion Client(r);
	
	GRegion Update(Client);
	bool HasTools = false;
	GViewI *v;
	List<GViewI>::I Lst = Children.Start();

	{
		GRegion Tools;
		
		for (v = *Lst; v; v = *++Lst)
		{
			if (IsTool(v))
			{
				GRect OldPos = v->GetPos();
				Update.Union(&OldPos);
				
				if (HasTools)
				{
					// 2nd and later toolbars
					if (v->Pour(Tools))
					{
						if (!v->Visible())
						{
							v->Visible(true);
						}

						if (OldPos != v->GetPos())
						{
							// position has changed update...
							v->Invalidate();
						}

						Tools.Subtract(&v->GetPos());
						Update.Subtract(&v->GetPos());
					}
				}
				else
				{
					// First toolbar
					if (v->Pour(Client))
					{
						HasTools = true;

						if (!v->Visible())
						{
							v->Visible(true);
						}

						if (OldPos != v->GetPos())
						{
							v->Invalidate();
						}

						GRect Bar(v->GetPos());
						Bar.x2 = GetClient().x2;

						Tools = Bar;
						Tools.Subtract(&v->GetPos());
						Client.Subtract(&Bar);
						Update.Subtract(&Bar);
					}
				}
			}
		}
	}

	Lst = Children.Start();
	for (GViewI *v = *Lst; v; v = *++Lst)
	{
		if (!IsTool(v))
		{
			GRect OldPos = v->GetPos();
			Update.Union(&OldPos);

			if (v->Pour(Client))
			{
				GRect p = v->GetPos();

				if (!v->Visible())
				{
					v->Visible(true);
				}

				v->Invalidate();

				Client.Subtract(&v->GetPos());
				Update.Subtract(&v->GetPos());
			}
			else
			{
				// non-pourable
			}
		}
	}
	
	for (int i=0; i<Update.Length(); i++)
	{
		Invalidate(Update[i]);
	}

}

int GWindow::WillAccept(List<char> &Formats, GdcPt2 Pt, int KeyState)
{
	int Status = DROPEFFECT_NONE;
	
	for (char *f=Formats.First(); f; )
	{
		if (stricmp(f, LGI_FileDropFormat) == 0)
		{
			f = Formats.Next();
			Status = DROPEFFECT_COPY;
		}
		else
		{
			Formats.Delete(f);
			DeleteArray(f);
			f = Formats.Current();
		}
	}
	
	return Status;
}

int GWindow::OnDrop(char *Format, GVariant *Data, GdcPt2 Pt, int KeyState)
{
	int Status = DROPEFFECT_NONE;

	if (Format AND Data)
	{
		if (stricmp(Format, LGI_FileDropFormat) == 0)
		{
			GArray<char*> Files;

			GToken Uri;

			if (Data->IsBinary())
			{
				Uri.Parse(	(char*)Data->Value.Binary.Data,
							"\r\n,",
							true,
							Data->Value.Binary.Length);
			}
			else if (Data->Str())
			{
				Uri.Parse(	Data->Str(),
							"\r\n,",
							true);
			}

			for (int i=0; i<Uri.Length(); i++)
			{
				char *File = Uri[i];
				if (strnicmp(File, "file:", 5) == 0)
					File += 5;
				if (strnicmp(File, "//localhost", 11) == 0)
					File += 11;
				
				char *i = File, *o = File;
				while (*i)
				{
					if (i[0] == '%' AND
						i[1] AND
						i[2])
					{
						char h[3] = { i[1], i[2], 0 };
						*o++ = htoi(h);
						i += 3;
					}
					else
					{
						*o++ = *i++;
					}
				}
				*o++ = 0;
				
				if (FileExists(File))
				{
					Files.Add(NewStr(File));
				}
			}
			
			if (Files.Length())
			{
				Status = DROPEFFECT_COPY;
				OnReceiveFiles(Files);
				Files.DeleteArrays();
			}
		}
	}
	
	return Status;
}

int GWindow::OnEvent(GMessage *m)
{
	switch (MsgCode(m))
	{
		case M_CLOSE:
		{
			if (OnRequestClose(false))
			{
				Quit();
				return 0;
			}
			break;
		}
	}

	return GView::OnEvent(m);
}

bool GWindow::RegisterHook(GView *Target, int EventType, int Priority)
{
	bool Status = false;
	
	if (Target AND EventType)
	{
		int i = d->GetHookIndex(Target, true);
		if (i >= 0)
		{
			d->Hooks[i].Flags = EventType;
			Status = true;
		}
	}
	
	return Status;
}

bool GWindow::UnregisterHook(GView *Target)
{
	int i = d->GetHookIndex(Target);
	if (i >= 0)
	{
		d->Hooks.DeleteAt(i);
		return true;
	}
	return false;
}

int GWindow::OnCommand(int Cmd, int Event, OsView SrcCtrl)
{
	OsView v;

	switch (Cmd)
	{
		case kHICommandCut:
		{
			OSErr e = GetKeyboardFocus(Wnd, (ControlRef*) &v);
			if (!e)
				LgiPostEvent(v, M_CUT);
			break;
		}
		case kHICommandCopy:
		{
			OSErr e = GetKeyboardFocus(Wnd, (ControlRef*) &v);
			if (!e)
				LgiPostEvent(v, M_COPY);
			break;
		}
		case kHICommandPaste:
		{
			OSErr e = GetKeyboardFocus(Wnd, (ControlRef*) &v);
			if (!e)
				LgiPostEvent(v, M_PASTE);
			break;
		}
		case 'dele':
		{
			OSErr e = GetKeyboardFocus(Wnd, (ControlRef*) &v);
			if (!e)
				LgiPostEvent(v, M_DELETE);
			break;
		}
	}
	
	return 0;
}


