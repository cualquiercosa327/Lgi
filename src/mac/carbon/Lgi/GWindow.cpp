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
	uint64 LastMinimize;
	bool CloseRequestDone;

	GMenu *EmptyMenu;
	
	GViewI *Focus;

	GWindowPrivate(GWindow *wnd)
	{
		Focus = NULL;
		InitVisible = false;
		LastMinimize = 0;
		Wnd = wnd;
		TrackingHandler = 0;
		ReceiveHandler = 0;
		DeleteWhenDone = false;
		ChildDlg = 0;
		EventUPP = 0;
		Sx = Sy = -1;
		Dynamic = true;
		SnapToEdge = false;
		EmptyMenu = 0;
		CloseRequestDone = false;
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
	_Default = 0;
	_Window = this;
	WndFlags |= GWND_CREATE;
	GView::Visible(false);

    _Lock = new GMutex;

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
		printf("%s:%i - CreateNewWindow failed (e=%i).\n", _FL, (int)e); 
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

bool &GWindow::CloseRequestDone()
{
	return d->CloseRequestDone;
}

GViewI *GWindow::GetFocus()
{
	return d->Focus;
}

void GWindow::SetFocus(GViewI *ctrl)
{
	if (d->Focus == ctrl)
		return;

	if (d->Focus)
	{
		GView *v = d->Focus->GetGView();
		if (v)
			v->WndFlags &= ~GWF_FOCUS;
		d->Focus->OnFocus(false);
	}
	
	d->Focus = ctrl;

	if (d->Focus)
	{
		GView *v = d->Focus->GetGView();
		if (v)
			v->WndFlags |= GWF_FOCUS;
		d->Focus->OnFocus(true);
	}
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

// #define DND_TRACKING_DEBUG	1

OSErr GWindow::HandlerCallback(DragTrackingMessage *tracking, DragRef theDrag)
{
	Point Ms, Pin;
	OSErr e = GetDragMouse(theDrag, &Ms, &Pin);
#if DND_TRACKING_DEBUG
printf("DragTrack GetDragMouse=%i tracking=%p (%i)\n", e, tracking, tracking ? *tracking : 0);
#endif
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

			HIPoint Offset = { 16, 16 };
			if (Img)
			{
				e = SetDragImageWithCGImage(theDrag, *Img, &Offset, kDragDarkerTranslucency);
				if (e) printf("%s:%i - SetDragImageWithCGImage failed with %i\n", _FL, e);
			}
		}
	}
	
	GViewI *v = WindowFromPoint(p.x, p.y);
#if DND_TRACKING_DEBUG
printf("DragTrack WindowFromPoint=%p (%s)\n", v, v ? v->GetClass() : "(none)");
#endif
	if (!v)
		printf("%s:%i - no window from point: %i,%i (%i,%i).\n", _FL, p.x, p.y, (int)Ms.h, (int)Ms.v);
	else
	{
		GView *gv = v->GetGView();
#if DND_TRACKING_DEBUG
printf("\tGView=%p\n", gv);
#endif
		if (gv)
		{
			GDragDropTarget *Target = 0, *MatchingTarget = 0;
			GDragDropSource *Src = 0;
			List<char> Formats;

			UInt16 Items = 0;
			e = CountDragItems(theDrag, &Items);
			if (e) printf("CountDragItems=%i Items=%i\n", e, Items);
			DragItemRef ItemRef = 0;
			e = GetDragItemReferenceNumber(theDrag, 1, &ItemRef);
			if (e) printf("GetDragItemReferenceNumber=%i Ref=%i\n", e, (int)ItemRef);
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
							printf("%s:%i - GetFlavorData('%4.4s') failed with %i\n", _FL, (char*)&Type, (int)e);
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

			while (gv)
			{
				Target = gv->DropTargetPtr();
				if (Target)
				{
					// See if this target is accepting the data...
					List<char> TmpFormats;
					for (char *s = Formats.First(); s; s = Formats.Next())
					{
						TmpFormats.Insert(NewStr(s));
					}
					
					if (Target->WillAccept(TmpFormats, Pt, 0))
					{
						TmpFormats.DeleteArrays();
						MatchingTarget = Target;
						break;
					}
					else
					{
#if DND_TRACKING_DEBUG
printf("\tTarget(%s) not accepting these formats.\n", gv?gv->GetClass():"(none)");
#endif
					}

					TmpFormats.DeleteArrays();
				}

				// Walk up parent chain till we find the next target
				GViewI *p = gv->GetParent();
				gv = p ? p->GetGView() : 0;
			}
			
			if (MatchingTarget)
			{
				if (tracking)
				{
					// printf("Tracking = %04.4s\n", tracking);
					if (*tracking == kDragTrackingLeaveHandler)
					{
						// noop
					}
					else if (*tracking == kDragTrackingLeaveWindow)
					{
						MatchingTarget->OnDragExit();
						// printf("OnDragExit\n");
					}
					else
					{											
						if (*tracking == kDragTrackingEnterWindow)
						{
							MatchingTarget->OnDragEnter();
							// printf("OnDragEnter\n");
						}

						MatchingTarget->WillAccept(Formats, Pt, 0);
						// printf("WillAccept=%i\n", Effect);
					}
				}
				else // drop
				{
					GVariant Data;
					
					#if 1
					for (char *r=Formats.First(); r; r=Formats.Next())
						printf("'%s'\n", r);
					#endif
					
					int Effect = MatchingTarget->WillAccept(Formats, Pt, 0);
					if (Effect)
					{
						char *Format = Formats.First();
						
						if (Src)
						{
							// Lgi -> Lgi drop
							if (Src->GetData(&Data, Format))
							{
								MatchingTarget->OnDrop(	Format,
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
							
							GVariant v;

							for (int i=0; i<Items; i++)
							{
								Size sz;

								e = GetDragItemReferenceNumber(theDrag, 1+i, &ItemRef);
								if (e)
								{
									printf("%s:%i - GetDragItemReferenceNumber failed %i\n", _FL, e);
									break;
								}

								if (!GetFlavorDataSize(theDrag, ItemRef, f, &sz))
								{
									char *Data = new char[sz+1];
									e = GetFlavorData(	theDrag,
														ItemRef,
														f,
														Data,
														&sz,
														0);
									
									if (f == typeFileURL)
									{
										Data[sz] = 0;
										if (Items > 1)
										{
											if (i == 0)
												v.SetList();
											v.Value.Lst->Insert(new GVariant(Data));
										}
										else v = Data;
									}
									#if 0
									else if (f == kDragFlavorTypeHFS)
									{
										HFSFlavor *f = (HFSFlavor*)Data;
										printf("HFS %i\n", (int) Items);
										
										FSRef Ref;
										OSErr e = FSpMakeFSRef(&f->fileSpec, &Ref);
										if (e)
										{
											printf("%s:%i - FSpMakeFSRef failed with %i\n", _FL, e);
										}
										else
										{
											UInt8 Path[MAX_PATH];
											e = FSRefMakePath(&Ref, Path, sizeof(Path));
											if (e)
											{
												printf("%s:%i - FSRefMakePath failed with %i\n", _FL, e);
											}
											else
											{
												v = (char*)Path;
											}
										}
									}
									#endif
									else
									{
										v.SetBinary(sz, Data);
									}
								}
								else LgiTrace("%s:%i - GetFlavorDataSize failed.\n", _FL);
							}

							int Effect = MatchingTarget->OnDrop(Format,
																&v,
																Pt,
																modifiers & 0x800 ? LGI_EF_CTRL : 0);
							if (!Effect)
							{
								Status = dragNotAcceptedErr;
							}
						}
					}

					MatchingTarget->OnDragExit();
				}
			}

			#if 0
			// When there is no drop target
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
		else printf("%s:%i - No view.\n", _FL);
	}
	
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

static void _ClearChildHandles(GViewI *v)
{
	GViewIterator *it = v->IterateViews();
	if (it)
	{
		for (GViewI *v = it->First(); v; v = it->Next())
		{
			_ClearChildHandles(v);
			v->Detach();			
		}
	}
	DeleteObj(it);
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
		_View = 0;
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
			uint64 Now = LgiCurrentTime();
			if (Now - d->LastMinimize < 1000)
			{
				// printf("%s:%i - CollapseWindow ignored via timeout\n", _FL);
			}
			else
			{
				// printf("%s:%i - CollapseWindow "LGI_PrintfInt64","LGI_PrintfInt64"\n", _FL, Now, d->LastMinimize);
				CollapseWindow(WindowHandle(), false);
			}
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
			printf("%s:%i - CreateEvent failed with %i\n", _FL, (int)e);
		}
		else
		{
			EventTargetRef t = GetWindowEventTarget(Wnd);
			
			e = SetEventParameter(Ev, kEventParamLgiEvent, typeUInt32, sizeof(Event), &Event);
			if (e) printf("%s:%i - error %i\n", _FL, (int)e);
			e = SetEventParameter(Ev, kEventParamLgiA, typeUInt32, sizeof(a), &a);
			if (e) printf("%s:%i - error %i\n", _FL, (int)e);
			e = SetEventParameter(Ev, kEventParamLgiB, typeUInt32, sizeof(b), &b);
			if (e) printf("%s:%i - error %i\n", _FL, (int)e);
			
			// printf("Sending event to window %i,%i,%i\n", Event, a, b);

			#if 1
			
			e = SetEventParameter(Ev, kEventParamPostTarget, typeEventTargetRef, sizeof(t), &t);
			if (e) printf("%s:%i - error %i\n", _FL, (int)e);
			e = PostEventToQueue(GetMainEventQueue(), Ev, kEventPriorityStandard);
			if (e) printf("%s:%i - error %i\n", _FL, (int)e);
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

	#if 0
	UInt32 ev = LgiSwap32(eventClass);
	printf("WndProc %4.4s-%i\n", (char*)&ev, eventKind);
	#endif

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
						#if 1
						uint32 c = command.commandID;
						#ifndef __BIG_ENDIAN__
						c = LgiSwap32(c);
						#endif
						if (c != '0000')
							printf("%s:%i - Cmd='%4.4s'\n", _FL, (char*)&c);
						#endif

						extern int *ReturnFloatCommand;
						if (ReturnFloatCommand)
						{
							*ReturnFloatCommand = command.commandID;
						}
						else if (command.commandID == kHICommandQuit)
						{
							LgiCloseApp();
						}
						else if (command.commandID == kHICommandMinimizeWindow ||
								 command.commandID == kHICommandMinimizeAll)
						{
							w->d->LastMinimize = LgiCurrentTime();
							CollapseWindow(w->WindowHandle(), true);
						}
						else if (command.commandID == kHICommandClose)
						{
							GWindow *w = dynamic_cast<GWindow*>(v);
							if (w && (w->CloseRequestDone() || w->OnRequestClose(false)))
							{
								w->CloseRequestDone() = true;
								DeleteObj(v);
							}
						}
						else if (command.commandID == kHICommandHide)
						{
							ProcessSerialNumber PSN;
							OSErr e;

							e = MacGetCurrentProcess(&PSN);
							if (e)
								printf("MacGetCurrentProcess failed with %i\n", e);
							else
							{
								e = ShowHideProcess(&PSN, false);
								if (e)
									printf("ShowHideProcess failed with %i\n", e);
							}
						}
						else
						{
							w->OnCommand(command.commandID, 0, 0);
						}

						result = noErr;
					}
				}
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
					GWindow *w = dynamic_cast<GWindow*>(v);
					if (w && (w->CloseRequestDone() || w->OnRequestClose(false)))
					{
						w->CloseRequestDone() = true;
						DeleteObj(v);
					}
					
					result = noErr;
					break;
				}
				case kEventWindowCollapsing:
				{
					GWindow *w = v->GetWindow();
					if (w)
						w->d->LastMinimize = LgiCurrentTime();
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
								printf("%s:%i - SetRootMenu failed (e=%i)\n", _FL, (int)e);
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
									printf("%s:%i - SetRootMenu failed (e=%i)\n", _FL, (int)e);
								}
							}
						}
					}
					break;
				}
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
					if (Wnd && !Wnd->d->ChildDlg)
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
						break;				
					}
					if (!InRect(Client, Pt.h, Pt.v))
					{
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
					
					m.Target = v->WindowFromPoint(Cx, Cy);
					if (m.Target)
					{
						if (v->GetWindow()->HandleViewMouse(v, m))
						{
							m.ToView();
							GView *v = m.Target->GetGView();
							if (v)
							{
								if (v->_Mouse(m, false))
									result = noErr;
							}
							else printf("%s:%i - Not a GView\n", _FL);
						}
					}
					else printf("%s:%i - No target window for mouse event.\n", _FL);
					
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
					if (status) printf("error(%i) getting kEventParamMouseLocation\n", (int)status);
					status = GetEventParameter(inEvent, kEventParamKeyModifiers, typeUInt32, NULL, sizeof(modifierKeys), NULL, &modifierKeys);
					if (status) printf("error(%i) getting kEventParamKeyModifiers\n", (int)status);
					
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
					if ((m.Target = v->WindowFromPoint(m.x - Client.left, m.y - Client.top)))
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
					if (status) printf("error(%i) getting kEventParamMouseLocation\n", (int)status);
					status = GetEventParameter(inEvent, kEventParamKeyModifiers, typeUInt32, NULL, sizeof(modifierKeys), NULL, &modifierKeys);
					if (status) printf("error(%i) getting kEventParamKeyModifiers\n", (int)status);
					status = GetEventParameter(inEvent, kEventParamMouseWheelDelta, typeSInt32, NULL, sizeof(Delta), NULL, &Delta);
					if (status) printf("error(%i) getting kEventParamMouseWheelDelta\n", (int)status);
					
					if ((Target = v->WindowFromPoint(Pt.h - Client.left, Pt.v - Client.top)))
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
		if (GBase::Name())
			Name(GBase::Name());
		
		EventTypeSpec	WndEvents[] =
		{
			{ kEventClassCommand, kEventProcessCommand },
			
			{ kEventClassWindow, kEventWindowClose },
			{ kEventClassWindow, kEventWindowInit },
			{ kEventClassWindow, kEventWindowDispose },
			{ kEventClassWindow, kEventWindowBoundsChanged },
			{ kEventClassWindow, kEventWindowActivated },
			{ kEventClassWindow, kEventWindowShown },
			{ kEventClassWindow, kEventWindowCollapsing },
			
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
		if (e) LgiTrace("%s:%i - InstallEventHandler failed (%i)\n", _FL, e);

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
	#ifdef MAC
	if (m.Down())
	{
		GAutoPtr<GViewIterator> it(IterateViews());
		for (GViewI *n = it->Last(); n; n = it->Prev())
		{
			GPopup *p = dynamic_cast<GPopup*>(n);
			if (p)
			{
				GRect pos = p->GetPos();
				if (!pos.Overlap(m.x, m.y))
				{
					printf("Closing Popup, m=%i,%i not over pos=%s\n", m.x, m.y, pos.GetStr());
					p->Visible(false);
				}
			}
			else break;
		}
	}
	#endif

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
		if (d->CloseRequestDone || OnRequestClose(false))
		{
			d->CloseRequestDone = true;
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
	if (Wnd)
	{
		bool c = IsWindowCollapsed(Wnd);
		// printf("IsWindowCollapsed=%i\n", c);
		if (c)
			return GZoomMin;
		
		c = IsWindowInStandardState(Wnd, NULL, NULL);
		// printf("IsWindowInStandardState=%i\n", c);
		if (!c)
			return GZoomMax;
	}

	return GZoomNormal;
}

void GWindow::SetZoom(GWindowZoom i)
{
	OSStatus e = 0;
	switch (i)
	{
		case GZoomMin:
		{
			e = CollapseWindow(Wnd, true);
			if (e) printf("%s:%i - CollapseWindow failed with %i\n", _FL, (int)e);
			// else printf("GZoomMin ok.\n");
			break;
		}
		default:
		case GZoomNormal:
		{
			e = CollapseWindow(Wnd, false);
			if (e) printf("%s:%i - [Un]CollapseWindow failed with %i\n", _FL, (int)e);
			// else printf("GZoomNormal ok.\n");
			break;
		}
	}
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

bool GWindow::Name(const char *n)
{
	bool Status = GBase::Name(n);

	if (Wnd)
	{	
		CFStringRef s = CFStringCreateWithBytes(NULL, (UInt8*)n, strlen(n), kCFStringEncodingUTF8, false);
		if (s)
		{
			OSStatus e = SetWindowTitleWithCFString(Wnd, s);
			if (e)
			{
				printf("%s:%i - SetWindowTitleWithCFString failed (e=%i)\n", _FL, (int)e);
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
	return GBase::Name();
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

bool GWindow::SerializeState(GDom *Store, const char *FieldName, bool Load)
{
	if (!Store || !FieldName)
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
			printf("%s:%i - GetWindowBounds failed (e=%i)\n", _FL, (int)e);
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
		if (e) printf("%s:%i - SetWindowBounds error e=%i\n", _FL, (int)e);
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

	if (d->Sx != X() ||	d->Sy != Y())
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
		if (!stricmp(f, LGI_FileDropFormat))
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

	if (Format && Data)
	{
		if (!stricmp(Format, LGI_FileDropFormat))
		{
			GArray<char*> Files;
			GArray< GAutoString > Uri;

			if (Data->IsBinary())
			{
				Uri[0].Reset( NewStr((char*)Data->Value.Binary.Data, Data->Value.Binary.Length) );
			}
			else if (Data->Str())
			{
				Uri[0].Reset( NewStr(Data->Str()) );
			}
			else if (Data->Type == GV_LIST)
			{
				for (GVariant *v=Data->Value.Lst->First(); v; v=Data->Value.Lst->Next())
				{
					char *f = v->Str();
					Uri.New().Reset(NewStr(f));
				}
			}

			for (int i=0; i<Uri.Length(); i++)
			{
				char *File = Uri[i].Get();
				if (strnicmp(File, "file:", 5) == 0)
					File += 5;
				if (strnicmp(File, "//localhost", 11) == 0)
					File += 11;
				
				char *in = File, *out = File;
				while (*in)
				{
					if (in[0] == '%' &&
						in[1] &&
						in[2])
					{
						char h[3] = { in[1], in[2], 0 };
						*out++ = htoi(h);
						i += 3;
					}
					else
					{
						*out++ = *in++;
					}
				}
				*out++ = 0;
				
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
			if (d->CloseRequestDone || OnRequestClose(false))
			{
				d->CloseRequestDone = true;
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

void GWindow::OnTrayClick(GMouse &m)
{
	if (m.Down() || m.IsContextMenu())
	{
		GSubMenu RClick;
		OnTrayMenu(RClick);
		if (GetMouse(m, true))
		{
			#if WIN32NATIVE
			SetForegroundWindow(Handle());
			#endif
			int Result = RClick.Float(this, m.x, m.y);
			#if WIN32NATIVE
			PostMessage(Handle(), WM_NULL, 0, 0);
			#endif
			OnTrayMenuResult(Result);
		}
	}
}

bool GWindow::Obscured()
{
	#warning Impl me sometime.
	return false;
}