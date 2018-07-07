
#define _WIN32_WINNT 0x500
#include "Lgi.h"

#if defined(WIN32)
	#include <commctrl.h>
#elif defined(MAC)
	#include <Carbon/Carbon.h>
#elif !defined(BEOS)
	#define LGI_NATIVE_TIPS 1
#endif

#if LGI_NATIVE_TIPS
#include "GDisplayString.h"
#include "GPopup.h"

class NativeTip : public GPopup
{
	GAutoPtr<GDisplayString> s;
	
public:
	static GArray<NativeTip*> All;
	static NativeTip *PulseRunning;

	int Id;
	GRect Watch;
	
	const char *GetClass() { return "NativeTip"; }

	NativeTip(int id, GView *p) : GPopup(p)
	{
		All.Add(this);
		Id = id;
		Owner = p;
		ClearFlag(WndFlags, GWF_VISIBLE);
		Watch.ZOff(-1, -1);
	}
	
	~NativeTip()
	{
		All.Delete(this);
		if (PulseRunning == this)
		{
			PulseRunning = NULL;
			if (All.Length() > 0)
			{
				PulseRunning = All[0];
				PulseRunning->SetPulse(300);
			}
		}
	}
	
	void OnCreate()
	{
		if (!PulseRunning)
		{
			PulseRunning = this;
			SetPulse(300);
		}
	}
	
	void OnPulse()
	{
		// Check mouse position...
		for (unsigned i=0; i<All.Length(); i++)
		{
			NativeTip *t = All[i];
			GMouse m;
			if (t->Owner)
			{
				GWindow *Wnd = t->Owner->GetWindow();
				bool Active = Wnd ? Wnd->IsActive() : false;
				
				if (t->Owner->GetMouse(m))
				{
					m.Target = t->Owner;
			
					GRect w = t->Watch;
					bool Vis = w.Overlap(m.x, m.y);					
					// printf("Tip %s, in=%i, act=%i\n", t->GView::Name(), Vis, Active);					
					Vis = Vis && Active;
					
					if (Vis ^ t->Visible())
					{
						GRect r = t->GetPos();
						GdcPt2 pt(w.x1, w.y2);
						#ifdef __GTK_H__
						pt.y += 8;
						#endif
						t->Owner->PointToScreen(pt);

						// printf("Vis(%i): r=%s pt=%i,%i->%i,%i\n", in, r.GetStr(), w.x1, w.y2, pt.x, pt.y);
						
						r.Offset(pt.x - r.x1, pt.y - r.y1);
						t->SetPos(r);
						t->Visible(Vis);
					}
				}
			}
			else
			{
				All.DeleteAt(i--);
			}
		}
	}
	
	bool Name(char *n)
	{
		bool Status = GView::Name(n);
		if (s.Reset(new GDisplayString(SysFont, GView::Name())))
		{
			GRect r = GetPos();
			r.Dimension(s->X() + 4, s->Y() + 2);
			SetPos(r);
		}
		return Status;
	}
	
	void OnPaint(GSurface *pDC)
	{
		GRect c = GetClient();
		COLOUR b = LC_TOOL_TIP;
		
		// Draw border
		pDC->Colour(LC_BLACK, 24);
		pDC->Box(&c);
		c.Size(1, 1);
		
		// Draw text interior
		SysFont->Colour(LC_TEXT, b);
		SysFont->Transparent(false);

		if (s)
		{
			s->Draw(pDC, c.x1+1, c.y1, &c);
		}
		else
		{
			pDC->Colour(b, 24);
			pDC->Rectangle(&c);
		}
	}
	
	bool IsOverParent(int x, int y)
	{
		// Implement this code to return true if the x, y coordinate passed in is over the
		// window 'Parent'. It must return false if another window obsures the location given
		// be 'x' and 'y'.
		return false;
	}
};

NativeTip *NativeTip::PulseRunning = NULL;
GArray<NativeTip*> NativeTip::All;

#endif

class GToolTipPrivate
{
public:
	int NextUid;

	#if defined(MAC)
	HMHelpContentRec Tag;
	#elif LGI_NATIVE_TIPS
	GView *Parent;
	GHashTbl<int, NativeTip*> Tips;
	#endif
	
	GToolTipPrivate()
	{
		NextUid = 1;
		#if defined(MAC)
		#elif LGI_NATIVE_TIPS
		Parent = 0;
		#endif
	}

	~GToolTipPrivate()
	{
		#if defined(MAC)
		#elif LGI_NATIVE_TIPS
		// for (NativeTip *t = Tips.First(); t; t = Tips.Next())
		for (auto t : Tips)
		{
			delete t.value;
		}
		#endif
	}
};

GToolTip::GToolTip() : GView(0)
{
	d = new GToolTipPrivate;
}

GToolTip::~GToolTip()
{
	DeleteObj(d);
}

int GToolTip::NewTip(char *Name, GRect &Pos)
{
	int Status = 0;

	#if defined(MAC)

		#if COCOA
		#warning FIXME
		#else
	
		#ifdef __MACHELP__
		HMSetHelpTagsDisplayed(true);
		#else
		#error "__MACHELP__ not defined"
		#endif

		if (Name)
		{
			d->Tag.version = kMacHelpVersion;
			d->Tag.tagSide = kHMDefaultSide;
			d->Tag.content[kHMMinimumContentIndex].contentType = kHMCFStringLocalizedContent;
			d->Tag.content[kHMMinimumContentIndex].u.tagCFString = CFStringCreateWithCString(NULL, Name, kCFStringEncodingUTF8);
			d->Tag.absHotRect = Pos;
		}
		#endif
	
	#elif WINNATIVE

	if (_View && Name && GetParent())
	{
		TOOLINFOW ti;

		ZeroObj(ti);
		ti.cbSize = sizeof(ti);
		ti.uFlags = TTF_SUBCLASS;
		ti.hwnd = GetParent()->Handle();
		ti.rect = Pos;
		ti.lpszText = Utf8ToWide(Name);
		ti.uId = Status = d->NextUid++;

		int Result = SendMessage(_View, TTM_ADDTOOLW, 0, (LPARAM) &ti);

		DeleteArray(ti.lpszText);
	}
	
	#elif LGI_NATIVE_TIPS
	
	if (ValidStr(Name) && d->Parent)
	{
		// printf("NewTip('%s',%s)\n", Name, Pos.Describe());
		
		NativeTip *t = new NativeTip(d->NextUid++, d->Parent);
		if (t)
		{
			t->Watch = Pos;
			t->Name(Name);
			
			// This is a hack to get it to create a window...
			t->Visible(true);
			// But not show it yet... duh...
			t->Visible(false);
			
			d->Tips.Add(t->Id, t);
			
			Status = true;
		}
	}
	
	#endif

	return Status;
}

void GToolTip::DeleteTip(int Id)
{
	#if defined(MAC)
	
	
	
	#elif WINNATIVE

	if (GetParent())
	{
		TOOLINFOW ti;

		ZeroObj(ti);
		ti.cbSize = sizeof(ti);
		ti.hwnd = GetParent()->Handle();
		ti.uId = Id;

		SendMessage(_View, TTM_DELTOOL, 0, (LPARAM) &ti);
	}
	
	#elif LGI_NATIVE_TIPS
	
	for (unsigned i = 0; i < NativeTip::All.Length(); i++)
	{
		NativeTip *&t = NativeTip::All[i];
		if (t->Id == Id)
		{
			d->Tips.Delete(Id);
			DeleteObj(t);
			NativeTip::All.DeleteAt(i);
			break;
		}
	}

	#endif
}

bool GToolTip::Attach(GViewI *p)
{
	#if defined(MAC)
	
	/*
	if (!p)
	{
		LgiTrace("%s:%i - Error: no parent for tip.\n", _FL);
		return false;
	}

	GWindow *w = p->GetWindow();
	if (!w)
	{
		LgiTrace("%s:%i - Error: no window to attach tip to.\n", _FL);
		return false;
	}
	
	GdcPt2 pt(0, 0);
	for (GViewI *v = p; v && v != (GViewI*)w; v = v->GetParent())
	{
		GRect r = v->GetPos();
		pt.x += r.x1;
		pt.y += r.y1;
	}
	
	d->Tag.absHotRect.left += pt.x;
	d->Tag.absHotRect.top += pt.y;
	d->Tag.absHotRect.right += pt.x;
	d->Tag.absHotRect.bottom += pt.y;
	
	HMSetWindowHelpContent(w->WindowHandle(), &d->Tag);
	HMDisplayTag(&d->Tag);
	*/
	
	#elif WINNATIVE

	if (!p)
		return false;

	if (!_View)
	{
		SetParent(p);

		_View = CreateWindowEx(	NULL, TOOLTIPS_CLASS, NULL,
								WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
								CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
								NULL, // p->Handle(),
								NULL,
								NULL,
								NULL);
	}

	if (!_View)
		return false;
	SetWindowLongPtr(	_View, GWLP_USERDATA, (LONG_PTR)(GViewI*)this);
	SetWindowLong(		_View, GWL_LGI_MAGIC, LGI_GViewMagic);			
	SetWindowPos(		_View,
						HWND_TOPMOST,
						0, 0, 0, 0,
						SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
	
	#elif LGI_NATIVE_TIPS
	
	d->Parent = p->GetGView();
	return false;

	#endif

	return true;
}
