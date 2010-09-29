/*hdr
**      FILE:           GuiDlg.cpp
**      AUTHOR:         Matthew Allen
**      DATE:           8/9/1998
**      DESCRIPTION:    Dialog components
**
**      Copyright (C) 1998 Matthew Allen
**              fret@memecode.com
*/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "Lgi.h"
#include "GButton.h"

#define TICKS_PER_SECOND					1000000
#define DOUBLE_CLICK_TIMEOUT				(TICKS_PER_SECOND/3)

#define DefaultViewColour()					SetViewColor(B_TRANSPARENT_COLOR); SetFlags(Flags() & ~B_FULL_UPDATE_ON_RESIZE)

///////////////////////////////////////////////////////////////////////////////////////////
// Standard sub-class of any BControl
template <class b>
class BSub : public b
{
	GView *View;

public:
	BSub(GView *v) : b(BRect(0, 0, 1, 1), "BSub")
	{
		View = v;
	}

	void AttachedToWindow()
	{
		if (View) View->OnCreate();
	}

	/*
	void DetachedFromWindow()
	{
		if (View) View->OnDestroy();
	}
	
	void Draw(BRect UpdateRect) {}
	void FrameMoved(BPoint Point) {}
	void FrameResized(float width, float height) {}
	void Pulse() {}
	void MessageReceived(BMessage *message) {}
	void MakeFocus(bool f = true) {}
	void KeyDown(const char *bytes, int32 numBytes) {}
	void KeyUp(const char *bytes, int32 numBytes) {}
	void MouseDown(BPoint point) {}
	void MouseMoved(BPoint point, uint32 transit, const BMessage *message) {}
	bool QuitRequested() {}
	*/
};


///////////////////////////////////////////////////////////////////////////////////////////
extern rgb_color BeRGB(uchar r, uchar g, uchar b);

rgb_color BeRGB(uchar r, uchar g, uchar b)
{
	rgb_color c;

	c.red = r;
	c.green = g;
	c.blue = b;

	return c;
}

rgb_color BeRGB(COLOUR c)
{
	rgb_color b;

	b.red = R24(c);
	b.green = G24(c);
	b.blue = B24(c);

	return b;
}

/////////////////////////////////////////////////////////////////////////////////////////
/* FIXME?
#define LGI_WATCHER_MSG			0x80000000

class GMouseWatcher : public GThread
{
	int TimeOut;
	GView *Notify;

public:
	GMouseWatcher(int timeout, GView *notify)
	{
		TimeOut = timeout;
		Notify = notify;
		if (Notify)
		{
			Run();
		}
	}
	
	void Convert(GdcPt2 &Pt)
	{
		BPoint BPt(Pt.x, Pt.y);
		Notify->Handle()->ConvertFromScreen(&BPt);
		Pt.x = BPt.x;
		Pt.y = BPt.y;
	}
	
	int Main()
	{
		GMouse m;
		
		Notify->GetMouse(m);
		m.Flags |= LGI_WATCHER_MSG;
	
		while (m.Left() OR m.Right())
		{
			LgiSleep(TimeOut);

			GMouse Ms;
			Notify->GetMouse(Ms);
		
			if (Ms.x != m.x OR
				Ms.y != m.y)
			{
				if (Notify->Handle()->LockLooper())
				{
					m.x = Ms.x;
					m.y = Ms.y;
					m.Flags |= LGI_WATCHER_MSG;
					Notify->OnMouseMove(m);

					Notify->Handle()->UnlockLooper();
				}
			}
		}
	
		return 0;
	}
};
*/

///////////////////////////////////////////////////////////////////////////////////////////
GDialog::GDialog() : ResObject(Res_Dialog)
{
	ModalSem = 0;
	Name("Dialog");

	if (Wnd)
	{
		Wnd->SetFlags(B_NOT_RESIZABLE);
		/*
		Wnd->SetType(B_MODAL_WINDOW);
		Wnd->SetLook(B_TITLED_WINDOW_LOOK);
		*/
	}
}

GDialog::~GDialog()
{
	SetParent(0);
}

void GDialog::OnPosChange()
{
}

bool GDialog::LoadFromResource(int Resource)
{
	char n[256];
	GRect p;
	bool Status = GLgiRes::LoadFromResource(Resource, Children, &p, n);
	if (Status)
	{
		Name(n);
		SetPos(p, true);
	}
	return Status;
}

bool GDialog::OnRequestClose(bool OsClose)
{
	if (IsModal)
	{
		EndModal(0);
		return false;
	}

	return true;
}

status_t WaitForDelete(sem_id blocker, BWindow *Wnd)
{
	status_t	result;
	thread_id	this_tid = find_thread(NULL);
	bool		Warned = false;

	// block until semaphore is deleted (modal is finished)
	do
	{
		if (Wnd)
		{
			if (NOT Warned AND Wnd->IsLocked())
			{
				printf("%p Locked! Count=%i Thread=%i Req=%i\n",
					Wnd, Wnd->CountLocks(), Wnd->LockingThread(),
					Wnd->CountLockRequests());
				Warned = true;
			}
			
			// update the parent window periodically
			Wnd->UpdateIfNeeded();
		}
		result = acquire_sem_etc(blocker, 1, B_TIMEOUT, 100000);
	}
	while (result != B_BAD_SEM_ID);
	
	return result;
}

int GDialog::DoModal(OsView ParentHnd)
{
	printf("DoModal() Me=%i Count=%i Thread=%i\n", LgiGetCurrentThread(), Wnd->CountLocks(), Wnd->LockingThread());

	IsModal = true;
	ModalRet = 0;
	ModalSem = create_sem(0, "ModalSem");
	
	if (NOT _Default)
	{
		GViewI *c = FindControl(IDOK);
		GButton *Def = c ? dynamic_cast<GButton*>(c) : 0;
		if (Def)
		{
			_Default = Def;
		}
		else
		{
			for (c=Children.First(); c; c=Children.Next())
			{
				_Default = dynamic_cast<GButton*>(c);
				if (_Default) break;
			}
		}
	}

	if (Wnd->Lock())
	{
		// Setup the BWindow
		Wnd->ResizeTo(Pos.X(), Pos.Y());
		Wnd->MoveTo(Pos.x1, Pos.y1);
		Wnd->SetTitle(GObject::Name());

		// Add BView here to move the "OnCreate" call to the correct place
		BRect r = Wnd->Bounds();
		Handle()->MoveTo(0, 0);
		Handle()->ResizeTo(r.Width(), r.Height());
		Wnd->AddChild(Handle());
		AttachChildren();

		Wnd->Show();
		Wnd->Unlock();

		// MFC NOTE: WaitForDelete is different from CWnd::RunModalLoop
		// because it doesn't process messages.
		WaitForDelete(ModalSem, GetParent()?GetParent()->WindowHandle():0);
		
		delete_sem(ModalSem);
	}
	else
	{
		printf("%s:%i - DoModal Wnd->Lock() failed.\n", __FILE__, __LINE__);
	}	
	
	return ModalRet;
}

int GDialog::DoModeless()
{
	if (Wnd->Lock())
	{
		// Setup the BWindow
		Wnd->ResizeTo(Pos.X(), Pos.Y());
		Wnd->MoveTo(Pos.x1, Pos.y1);
		Wnd->SetTitle(GObject::Name());

		// Add BView here to move the "OnCreate" call to the correct place
		BRect r = Wnd->Bounds();
		Handle()->MoveTo(0, 0);
		Handle()->ResizeTo(r.Width(), r.Height());
		Wnd->AddChild(Handle());
		AttachChildren();

		// Open the window..
		Wnd->Show();
		Wnd->Unlock();
		
		return true;
	}
	
	return false;
}

int GDialog::OnEvent(BMessage *Msg)
{
	int Status = 0;

	// printf("GDialog::OnEvent(%i)\n", Msg->what);

	return Status;
}

void GDialog::EndModal(int Code)
{
	ModalRet = Code;
	delete_sem(ModalSem); // causes DoModal to unblock
}

void GDialog::EndModeless(int Code)
{
	if (Wnd->Lock())
	{
		Handle()->RemoveSelf();
		Wnd->Hide();
		Wnd->Quit();
		Wnd = 0;
	}
	
	OnDestroy();
}

///////////////////////////////////////////////////////////////////////////////////////////
GControl::GControl(BView *view) : GView(view)
{
	Pos.ZOff(10, 10);
	SetId(0);
	Sys_LastClick = 0;
}

GControl::~GControl()
{
}

int GControl::OnEvent(BMessage *Msg)
{
	return 0;
}

GdcPt2 GControl::SizeOfStr(char *Str)
{
	GdcPt2 Pt(0, 0);
	if (Str)
	{
		BFont Font;
		_View->GetFont(&Font);
		
		font_height Ht;
		Font.GetHeight(&Ht);
		
		Pt.y = Ht.ascent + Ht.descent + Ht.leading + 0.9999;
		Pt.x = Font.StringWidth(Str);
	}

	return Pt;
}

void GControl::MouseClickEvent(bool Down)
{
	static int LastX = -1, LastY = -1;

	GMouse m;
	BPoint Pt;
	uint32 Buttons;
	if (Handle()->LockLooper())
	{
		Handle()->GetMouse(&Pt, &Buttons, FALSE);
		m.x = Pt.x;
		m.y = Pt.y;
		m.Flags = Buttons;

		if (Down)
		{
			int32 Clicks = 1;
			WindowHandle()->CurrentMessage()->FindInt32("clicks", &Clicks);
			m.Double(	(Clicks > 1) AND
						(abs(m.x-LastX) < DOUBLE_CLICK_THRESHOLD) AND
						(abs(m.y-LastY) < DOUBLE_CLICK_THRESHOLD));
			
			LastX = m.x;
			LastY = m.y;
		}
		else
		{
			m.Double(false);
		}
		
		m.Down(Down);
		OnMouseClick(m);
		Handle()->UnlockLooper();
	}
}

///////////////////////////////////////////////////////////////////////////////////////////
// Button
/*
class GButtonRedir : public BButton
{
	GButton *Obj;
	
public:
	GButtonRedir(GButton *obj) :
		BButton(BRect(0, 0, 1, 1), "Button", "", new BMessage(M_CHANGE))
	{
		Obj = obj;
	}
	
	status_t Invoke(BMessage *message = NULL)
	{
		status_t Ret = BButton::Invoke(message);
	
		if (Obj)
		{
			GView *n = (Obj->GetNotify()) ? Obj->GetNotify() : Obj->GetParent();
			if (n)
			{
				n->OnNotify(Obj, 0);
			}
		}
		
		return Ret;
	}
	
	void AttachedToWindow()
	{
		if (Obj) Obj->OnCreate();
	}
	
	void KeyDown(const char *bytes, int32 numBytes)
	{
		BButton::KeyDown(bytes, numBytes);
		if (Obj) Obj->Sys_KeyDown(bytes, numBytes);
	}
	
	void KeyUp(const char *bytes, int32 numBytes)
	{
		BButton::KeyUp(bytes, numBytes);
		if (Obj) Obj->Sys_KeyUp(bytes, numBytes);
	}
};

class GButtonPrivate
{
public:
	GButtonRedir *Button;
};

GButton::GButton(int id, int x, int y, int cx, int cy, char *name)
	: GControl(new GButtonRedir(this))
	, ResObject(Res_Button)
{
	d = new GButtonPrivate;
	d->Button = dynamic_cast<GButtonRedir*>(_View);
	Name(name);
	Pos.Set(x, y, x+cx, y+cy);
	SetId(id);
	if (Button)
	{
		Button->SetViewColor(216, 216, 216);
	}
}

GButton::~GButton()
{
}

bool GButton::Default()
{
	return (Button) ? Button->IsDefault() : false;
}

void GButton::Default(bool b)
{
	if (Button) Button->MakeDefault(b);
}

bool GButton::Name(char *s)
{
	bool Status = GObject::Name(s);
	if (Button)
	{
		bool Lock = Button->LockLooper();
		Button->SetLabel(s);
		if (Lock) Button->UnlockLooper();
	}
	return Status;
}

int GButton::OnEvent(GMessage *Msg)
{
	return 0;
}
*/

///////////////////////////////////////////////////////////////////////////////////////////
// Edit
#include "GEdit.h"

#if 0

class _OsEditView;
class _OsEditFrame : public BView
{
	friend class GEdit;
	friend class _OsEditView;

private:
	GEdit *Edit;
	_OsEditView *View;
	BScrollBar *X, *Y;

public:
	_OsEditFrame(GEdit *edit, const BRect &r);

	void Draw(BRect updateRect);
	void KeyDown(const char *bytes, int32 numBytes);
};

class _OsEditView : public BTextView
{
	friend class GEdit;
	friend class _OsEditFrame;

private:
	_OsEditFrame *Frame;
	GEdit *Edit;
	bool Notify;

public:
	_OsEditView(_OsEditFrame *frame, const BRect &r) :
		BTextView(	r,
					"_OsEditFrame",
					BRect(1, 1, 1000, 20),
					B_FOLLOW_ALL_SIDES,
					LGI_DRAW_VIEW_FLAGS)
	{
		Notify = true;
		Frame = frame;
		Edit = Frame->Edit;
		SetWordWrap(false);
		SetStylable(false);
	}

	void KeyDown(const char *bytes, int32 numBytes)
	{
		if (Edit->MultiLine())
		{
			BTextView::KeyDown(bytes, numBytes);
		}
		else
		{
			if (bytes[0] == '\t')
			{
				BView::KeyDown(bytes, numBytes);
			}
			else if (bytes[0] != '\n')
			{
				BTextView::KeyDown(bytes, numBytes);
			}
		}
		
		Frame->KeyDown(bytes, numBytes);
	}
	
	void OnChange()
	{
		if (Notify)
		{
			GViewI *n = Edit->GetNotify() ? Edit->GetNotify() : Edit->GetParent();
			if (n) n->OnNotify(Edit, 0);
		}
	}
	
	void InsertText(const char *text, int32 length, int32 offset, const text_run_array *runs)
	{
		BTextView::InsertText(text, length, offset, runs);
		OnChange();
	}
	
	void DeleteText(int32 start, int32 finish)
	{
		BTextView::DeleteText(start, finish);
		OnChange();
	}
};

_OsEditFrame::_OsEditFrame(GEdit *edit, const BRect &r) :
	BView(r, "_OsEditFrame", B_FOLLOW_LEFT | B_FOLLOW_TOP, B_WILL_DRAW | B_FRAME_EVENTS | B_FULL_UPDATE_ON_RESIZE)
{
	Edit = edit;
	DefaultViewColour();
	
	View = new _OsEditView(this, BRect(2, 2, 6, 6));
	if (View)
	{
		AddChild(View);
	}
}

void _OsEditFrame::Draw(BRect updateRect)
{
	GRect r(Bounds());
	GScreenDC Dc(this);
	LgiWideBorder(&Dc, r, SUNKEN);
}

void _OsEditFrame::KeyDown(const char *bytes, int32 numBytes)
{
	Edit->Sys_KeyDown(bytes, numBytes);
}

class GEditPrivate
{
public:
	bool MultiLine;
	bool Password;
	_OsEditFrame *Edit;

	GEditPrivate()
	{
		MultiLine = false;
		Password = false;
		Edit = 0;
	}
};

GEdit::GEdit(int id, int x, int y, int cx, int cy, char *name) :
	GControl(new _OsEditFrame(this, BRect(x, y, x+cx, y+cy))),
	ResObject(Res_EditBox)
{
	d = new GEditPrivate;
	d->Edit = dynamic_cast<_OsEditFrame*>(Handle());
	
	GdcPt2 Size = SizeOfStr((name)?name:(char*)"A");
	if (cx < 0) cx = Size.x + 6;
	if (cy < 0) cy = Size.y + 4;

	SetId(id);
	GRect r(x, y, x+cx, y+cy);
	SetPos(r);
	Name(name);
}

GEdit::~GEdit()
{
	DeleteObj(d);
}

void GEdit::Select(int start, int len)
{
}

void GEdit::Password(bool i)
{
	d->Password = i;
	if (d->Edit)
	{
		d->Edit->View->HideTyping(i);
	}
}

void GEdit::OnCreate()
{
}

bool GEdit::MultiLine()
{
	return false;
}

bool GEdit::Focus()
{
	return false;
}

void GEdit::Focus(bool i)
{
}

bool GEdit::Enabled()
{
	return GView::Enabled();
}

void GEdit::Enabled(bool e)
{
	if (e) SetFlag(WndFlags, GWF_ENABLED);
	else ClearFlag(WndFlags, GWF_ENABLED);
	
	if (d->Edit AND
		d->Edit->View)
	{
		bool Lock = d->Edit->View->LockLooper();
		
		rgb_color Black = BeRGB(LC_BLACK);
		rgb_color White = BeRGB(LC_WHITE);
		rgb_color DkGrey = BeRGB(LC_LOW);
		rgb_color LtGrey = BeRGB(LC_MED);
		
		d->Edit->View->MakeEditable(e);
		d->Edit->View->SetFontAndColor(0, 0, (e) ? &Black : &DkGrey);
		d->Edit->View->SetViewColor((e) ? White : LtGrey);
		d->Edit->View->Invalidate();
		
		if (Lock) d->Edit->View->UnlockLooper();
	}

	Invalidate();
}

bool GEdit::SetPos(GRect &p, bool Repaint)
{
	GControl::SetPos(p);
	if (d->Edit)
	{
		bool Lock = d->Edit->LockLooper();

		d->Edit->MoveTo(p.x1, p.y1);
		d->Edit->ResizeTo(p.X()-1, p.Y()-1);

		d->Edit->View->MoveTo(2, 2);
		d->Edit->View->ResizeTo(p.X() - 5, p.Y() - 5);

		if (Lock) d->Edit->UnlockLooper();
	}
	return true;
}

void GEdit::MultiLine(bool m)
{
	d->MultiLine = m;
}

void GEdit::Value(int64 i)
{
	char Str[32];
	sprintf(Str, "%li", i);
	Name(Str);
}

int64 GEdit::Value()
{
	char *n = Name();
	return (n) ? atoll(n) : 0;
}

char *GEdit::Name()
{
	return (char*) ((d->Edit) ? d->Edit->View->Text() : 0);
}

bool GEdit::Name(char *s)
{
	GObject::Name(s);
	if (d->Edit)
	{
		bool Lock = d->Edit->View->LockLooper();

		d->Edit->View->Notify = false;
		d->Edit->View->SetText(s);
		d->Edit->View->Notify = true;

		if (Lock) d->Edit->View->UnlockLooper();
	}
	return true;
}

char16 *GEdit::NameW()
{
	return 0;
}

bool GEdit::NameW(char16 *s)
{
	GObject::NameW(s);
	return true;
}

int GEdit::OnNotify(GViewI *c, int f)
{
	return 0;
}

int GEdit::OnEvent(GMessage *Msg)
{
	return 0;
}

bool GEdit::OnKey(GKey &k)
{
	if (k.Down() AND k.c16 == VK_RETURN)
	{
		GViewI *n = (GetNotify()) ? GetNotify() : GetParent();
		if (n) n->OnNotify(this, k.c16);
		return true;
	}
	
	return false;
}
#endif

///////////////////////////////////////////////////////////////////////////////////////////
// Check box
/*
class GCheckBoxRedir : public BCheckBox
{
	GCheckBox *Obj;
	bool Notify;
	
public:
	GCheckBoxRedir(GCheckBox *obj, int Init) :
		BCheckBox(BRect(0,0,1,1), "Check", "", new BMessage(M_CHANGE))
	{
		Obj = obj;
		Notify = false;
		SetValue(Init);
		Notify = true;
		DefaultViewColour();
	}

	void SetValue(int32 value)
	{
		BCheckBox::SetValue(value);
		
		if (Notify) // this is to protect the Obj being called before construction
		{
			GView *n = Obj->GetNotify() ? Obj->GetNotify() : Obj->GetParent();
			if (n)
			{
				n->OnNotify(Obj, 0);
			}
		}
	}
};

GCheckBox::GCheckBox(int id, int x, int y, int cx, int cy, char *name, int InitState)
	: GControl(Box = new GCheckBoxRedir(this, InitState)))
	, ResObject(Res_CheckBox)
{
	GdcPt2 Size = SizeOfStr(name);
	if (cx < 0) cx = Size.x + 26;
	if (cy < 0) cy = Size.y + 6;

	Val = InitState;
	Three = FALSE;
	Pos.Set(x, y, x+cx, y+cy);
	SetId(id);
	if (name) Name(name);
}

GCheckBox::~GCheckBox()
{
}

int GCheckBox::Value()
{
	if (Box)
	{
		Val = Box->Value();
	}
	return Val;
}

void GCheckBox::Value(int b)
{
	Val = b;
	if (Box)
	{
		Box->SetValue(Val);
	}
}

bool GCheckBox::Name(char *s)
{
	bool Status = GObject::Name(s);
	if (Box)
	{
		bool Lock = Box->LockLooper();
		Box->SetLabel(s);
		if (Lock) Box->UnlockLooper();
	}
	return Status;
}

int GCheckBox::OnEvent(BMessage *Msg)
{
	return 0;
}
*/

//////////////////////////////////////////////////////////////////////////////////////////////////
// Text
/*
GText::GText(int id, int x, int y, int cx, int cy, char *name) :
	GControl(new BViewRedir(this))
	, ResObject(Res_StaticText)
{
	GdcPt2 Size = SizeOfStr(name);
	if (cx < 0) cx = Size.x;
	if (cy < 0) cy = Size.y;

	_BorderSize = 0;
	Name(name);
	Pos.Set(x, y, x+cx, y+cy);
	SetId(id);
	Handle()->SetFlags(Handle()->Flags() & ~B_NAVIGABLE);
}

GText::~GText()
{
}

void GText::OnPaint(GSurface *pDC)
{
	pDC->Colour(LC_MED, 24);
	pDC->Rectangle();

	SysFont->Colour(LC_BLACK, LC_MED);
	SysFont->Transparent(true);

	char *c = Name();
	if (c)
	{
		int Line = SysFont->Y("A");
		int x = 0, y = 0;
		char *e;

		for (char *s = c; s AND *s; s = e ? e + 1 : 0)
		{
			e = strchr(s, '\n');
			int Len = (e) ? (int)e-(int)s : strlen(s);
			SysFont->Text(pDC, x, y, s, Len);
			y += Line;
		}
	}
}

int GText::OnEvent(BMessage *Msg)
{
	return GControl::OnEvent(Msg);
}

char *GText::Name()
{
	return GObject::Name();
}

bool GText::Name(char *n)
{
	bool Status = GObject::Name(n);
	Invalidate();
	return Status;
}
*/

///////////////////////////////////////////////////////////////////////////////////////////
// Radio group

/*
int GRadioGroup::NextId = 20000;
GRadioGroup::GRadioGroup(int id, int x, int y, int cx, int cy, char *name, int Init)
	: GControl(Box = new BSub<BBox>(this))
	, ResObject(Res_Group)
{
	Pos.Set(x, y, x+cx, y+cy);
	SetId(id);
	InitialValue = Init;
	if (name) Name(name);
}

GRadioGroup::~GRadioGroup()
{
}

bool GRadioGroup::Name(char *s)
{
	bool Status = GObject::Name(s);
	if (Box)
	{
		bool Lock = Box->LockLooper();
		Box->SetLabel(s);
		if (Lock) Box->UnlockLooper();
	}
	return Status;
}

void GRadioGroup::OnCreate()
{
	AttachChildren();
	Value(InitialValue);
}

int GRadioGroup::Value()
{
	if (Parent)
	{
		int i=0;
		for (	GView *w = Children.First();
				w;
				w = Children.Next(), i++)
		{
			GRadioButton *But = dynamic_cast<GRadioButton*>(w);
			if (But AND But->Value()) return i;
		}
	
		return -1;
	}

	return InitialValue;
}

void GRadioGroup::Value(int Which)
{
	if (Parent)
	{
		int i=0;
		for (	GView *w = Children.First();
				w;
				w = Children.Next(), i++)
		{
			GRadioButton *But = dynamic_cast<GRadioButton*>(w);
			if (But)
			{
				But->Value(Which == i);
			}
		}
	}
	else
	{
		InitialValue = Which;
	}
}

int GRadioGroup::OnNotify(GViewI *Ctrl, int Flags)
{
	GView *n = GetNotify() ? GetNotify() : GetParent();
	if (n)
	{
		if (dynamic_cast<GRadioButton*>(Ctrl))
		{
			n->OnNotify(this, Value());
		}
		else
		{
			n->OnNotify(Ctrl, Flags);
		}
	}
	return 0;
}

int GRadioGroup::OnEvent(BMessage *Msg)
{
	return 0;
}

GRadioButton *GRadioGroup::Append(int x, int y, char *name)
{
	GRadioButton *But = new GRadioButton(NextId++, x, y, -1, -1, name);
	if (But)
	{
		Children.Insert(But);
	}
	return But;
}

///////////////////////////////////////////////////////////////////////////////////////////
// Radio button
class GRadioButtonRedir : public BRadioButton
{
	GRadioButton *Obj;
	
public:
	GRadioButtonRedir(GRadioButton *obj) :
		BRadioButton(BRect(0,0,1,1), "RadioBtn", "", new BMessage(WM_CHANGE))
	{
		Obj = obj;
		DefaultViewColour();
	}
	
	void SetValue(int32 value)
	{
		BRadioButton::SetValue(value);
		if (value AND Obj)
		{
			GView *w = (Obj->GetNotify()) ? Obj->GetNotify() : Obj->GetParent();
			if (w)
			{
				w->OnNotify(Obj, 0);
			}
		}
	}
};

GRadioButton::GRadioButton(int id, int x, int y, int cx, int cy, char *name, bool first)
	: GControl(Button = new GRadioButtonRedir(this)))
	, ResObject(Res_RadioBox)
{
	GdcPt2 Size = SizeOfStr(name);
	if (cx < 0) cx = Size.x + 26;
	if (cy < 0) cy = Size.y + 6;

	Pos.Set(x, y, x+cx, y+cy);
	SetId(id);
	if (name) Name(name);
}

GRadioButton::~GRadioButton()
{
}

int GRadioButton::Value()
{
	int Status = 0;
	if (Button)
	{
		bool Lock = Button->LockLooper();
		Status = Button->Value();
		if (Lock) Button->UnlockLooper();
	}
	return Status;
}

void GRadioButton::Value(int i)
{
	if (Button)
	{
		bool Lock = Button->LockLooper();
		Button->SetValue(i);
		if (Lock) Button->UnlockLooper();
	}
}

bool GRadioButton::Name(char *s)
{
	bool Status = GObject::Name(s);
	if (Button)
	{
		bool Lock = Button->LockLooper();
		Button->SetLabel(s);
		if (Lock) Button->UnlockLooper();
	}
	return Status;
}

int GRadioButton::OnEvent(BMessage *Msg)
{
	return 0;
}
*/

//////////////////////////////////////////////////////////////////////////////////
// Slider control
#include "GSlider.h"

GSlider::GSlider(int id, int x, int y, int cx, int cy, char *name, bool vert) :
	GControl(new BViewRedir(this)),
	ResObject(Res_Slider)
{
	SetId(id);
	
	if (vert)
	{
		GRect r(x, y, x+13, y+cy);
		SetPos(r);
	}
	else
	{
		GRect r(x, y, x+cx, y+13);
		SetPos(r);
	}
	
	Name(name);
	Vertical = vert;
	Min = 0;
	Max = 1;
	Val = 0;
	
	Thumb.ZOff(-1, -1);
}

GSlider::~GSlider()
{
}

void GSlider::Value(int64 i)
{
	Val = i;
	Invalidate();
}

int64 GSlider::Value()
{
	return Val;
}

void GSlider::GetLimits(int64 &min, int64 &max)
{
	min = Min;
	max = Max;
}

void GSlider::SetLimits(int64 min, int64 max)
{
	Min = min;
	Max = max;
	if (Val < Min) Val = Min;
	if (Val > Max) Val = Max;
}

int GSlider::OnEvent(BMessage *Msg)
{
	return GView::OnEvent(Msg);
}

#define SLIDER_BAR_SIZE			2
#define SLIDER_THUMB_WIDTH		10
#define SLIDER_THUMB_HEIGHT		6

void GSlider::OnPaint(GSurface *pDC)
{
	GRect r(0, 0, X()-1, Y()-1);

	// fill area
	pDC->Colour(LC_MED, 24);
	pDC->Rectangle(&r);

	// draw tray
	int Mid = (Vertical) ? r.X()/2 : r.Y()/2;
	GRect Bar;
	if (Vertical)
	{
		Bar.ZOff(SLIDER_BAR_SIZE, Y()-1);
		Bar.Offset(Mid-(Bar.X()/2), 0);
	}
	else
	{
		Bar.ZOff(X()-1, SLIDER_BAR_SIZE);
		Bar.Offset(0, Mid-(Bar.Y()/2));
	}
	
	LgiThinBorder(pDC, Bar, SUNKEN);
	pDC->Colour(0);
	pDC->Line(Bar.x1, Bar.y1, Bar.x2, Bar.y2);
	
	// draw thumb
	int Length = ((Vertical) ? Y() : X()) - SLIDER_THUMB_HEIGHT - 1;
	int Pos = (Max != Min) ? Val * Length / (Max - Min) : 0;
	if (Vertical)
	{
		Thumb.ZOff(SLIDER_THUMB_WIDTH, SLIDER_THUMB_HEIGHT);
		Thumb.Offset(Mid - (Thumb.X()/2), Pos);
	}
	else
	{
		Thumb.ZOff(SLIDER_THUMB_HEIGHT, SLIDER_THUMB_WIDTH);
		Thumb.Offset(Pos, Mid - (Thumb.Y()/2));
	}
	
	GRect t = Thumb;
	LgiWideBorder(pDC, t, RAISED);
	pDC->Colour(LC_MED, 24);
	pDC->Rectangle(&t);
}

void GSlider::OnMouseClick(GMouse &m)
{
	if (m.Down())
	{
		if (Thumb.Overlap(m.x, m.y))
		{
			Tx = m.x - Thumb.x1;
			Ty = m.y - Thumb.y1;
			Capture(true);
		}
	}
	else if (IsCapturing())
	{
		Capture(false);
	}
}

void GSlider::OnMouseMove(GMouse &m)
{
	if (IsCapturing())
	{
		int Length = ((Vertical) ? Y() : X()) - SLIDER_THUMB_HEIGHT - 1;
		int NewPos = (Vertical) ? m.y - Ty : m.x - Tx;
		if (NewPos < 0) NewPos = 0;
		if (NewPos > Length) NewPos = Length;
		int NewVal = (Min != Max) ? (NewPos*(Max-Min)) / Length : 0;
		if (NewVal != Val)
		{
			Val = NewVal;
			Invalidate();
			
			if (GetParent())
			{
				GetParent()->OnNotify(this, Val);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////
#include "GBitmap.h"

GBitmap::GBitmap(int id, int x, int y, char *FileName, bool Async) :
	GControl(new BViewRedir(this)),
	ResObject(Res_Bitmap)
{
	SetId(id);
	GRect r(x, y, x + 1, y + 1);
	SetPos(r);
	pDC = 0;
	Sunken(true);

	int Opt = GdcD->SetOption(GDC_PROMOTE_ON_LOAD, GdcD->GetBits());
	pDC = LoadDC(FileName);
	GdcD->SetOption(GDC_PROMOTE_ON_LOAD, Opt);

	if (pDC)
	{
		r.Dimension(pDC->X()+4, pDC->Y()+4);
		SetPos(r);
	}
}

GBitmap::~GBitmap()
{
	DeleteObj(pDC);
}

int GBitmap::OnEvent(BMessage *Msg)
{
	return 0;
}

void GBitmap::OnPaint(GSurface *pScreen)
{
	GRect c = GetClient();
	c.Offset(-c.x1, -c.y1);

	if (pDC)
	{
		pScreen->Blt(c.x1, c.y1, pDC);
	}
	else
	{
		pScreen->Colour(CBit(pScreen->GetBits(), Rgb24(255, 255, 255)));
		pScreen->Rectangle(&c);
	}
}

void GBitmap::OnMouseClick(GMouse &m)
{
	if (NOT m.Down() AND GetParent())
	{
		GDialog *p = dynamic_cast<GDialog*>(GetParent());
		p->OnNotify(this, 0);
	}
}

void GBitmap::SetDC(GSurface *pNewDC)
{
	DeleteObj(pDC);
	pDC = pNewDC;
	if (pDC)
	{
		GRect r = GetPos();
		r.Dimension(pDC->X() + 4, pDC->Y() + 4);
		SetPos(r);
		Invalidate();
	}
}

GSurface *GBitmap::GetSurface()
{
	return pDC;
}

///////////////////////////////////////////////////////////////////////////////////////////
GItemContainer::GItemContainer()
{
	Flags = 0;
	ImageList = 0;
}

GItemContainer::~GItemContainer()
{
	if (OwnList())
	{
		DeleteObj(ImageList);
	}
	else
	{
		ImageList = 0;
	}
}

bool GItemContainer::SetImageList(GImageList *list, bool Own)
{
	ImageList = list;
	OwnList(Own);
	AskImage(ImageList != NULL);
	return ImageList != NULL;
}

/*
bool GItemContainer::Load(char *File)
{
	GSurface *pDC = LoadDC(File);
	if (pDC)
	{
		ImageList = new GImageList(24, 24, pDC);
		if (ImageList)
		{
			#ifdef WIN32
			ImageList->Create(pDC->X(), pDC->Y(), pDC->GetBits());
			#endif
		}
	}
	return pDC != 0;
}
*/

///////////////////////////////////////////////////////////////////////////////////////////
/*
GProgress::GProgress(int id, int x, int y, int cx, int cy, char *name) :
	GControl(new BViewRedir(this))
	, ResObject(Res_Progress)
{
	SetId(id);
	GRect r(x, y, x+cx, y+cy);
	SetPos(r);
	c = Rgb24(50, 150, 255);
}

GProgress::~GProgress()
{
}

void GProgress::Colour(COLOUR Col)
{
	c = Col;
	GView::Invalidate();
}

COLOUR GProgress::Colour()
{
	return c;
}

void GProgress::OnPaint(GSurface *pDC)
{
	GRect r(0, 0, X()-1, Y()-1);
	LgiThinBorder(pDC, r, SUNKEN);
	
	if (High != 0)
	{
		int Pos = ((double) r.X() * (((double) Val - Low) / High));
		if (Pos > 0)
		{
			COLOUR High = Rgb24( (R24(c)+255)/2, (G24(c)+255)/2, (B24(c)+255)/2 );
			COLOUR Low = Rgb24( (R24(c)+0)/2, (G24(c)+0)/2, (B24(c)+0)/2 );
			GRect p = r;

			p.x2 = p.x1 + Pos;
			r.x1 = p.x2 + 1;
			
			pDC->Colour(Low, 24);
			pDC->Line(p.x2, p.y2, p.x2, p.y1);
			pDC->Line(p.x2, p.y2, p.x1, p.y2);

			pDC->Colour(High, 24);
			pDC->Line(p.x1, p.y1, p.x2, p.y1);
			pDC->Line(p.x1, p.y1, p.x1, p.y2);

			p.Size(1, 1);
			pDC->Colour(c, 24);
			pDC->Rectangle(&p);
		}
	}

	if (r.Valid())
	{
		pDC->Colour(Rgb24(255, 255, 255), 24);
		pDC->Rectangle(&r);
	}
}

void GProgress::SetLimits(int l, int h)
{
	Low = l;
	High = h;
	GView::Invalidate();
}

void GProgress::GetLimits(int &l, int &h)
{
	l = Low;
	h = High;
}

void GProgress::Value(int v)
{
	Val = limit(v, Low, High);
	GView::Invalidate();
}

int GProgress::Value()
{
	return Val;
}

int GProgress::OnEvent(BMessage *Msg)
{
	return 0;
}
*/



