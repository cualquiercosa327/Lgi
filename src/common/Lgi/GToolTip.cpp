#define _WIN32_WINNT 0x500
#include "Lgi.h"
#if defined WIN32
#include <commctrl.h>
#endif

#if !WIN32NATIVE
#include "GDisplayString.h"

class NativeTip : public GView
{
	GDisplayString *s;
	
public:
	int Id;
	GRect Watch;
	GViewI *Owner;

	NativeTip(int id, GViewI *p)
	{
		Id = id;
		Owner = p;
		s = 0;
		ClearFlag(WndFlags, GWF_VISIBLE);
		Watch.ZOff(-1, -1);
	}
	
	~NativeTip()
	{
		DeleteObj(s);
	}
	
	bool Name(char *n)
	{
		bool Status = GView::Name(n);
		DeleteObj(s);
		s = new GDisplayString(SysFont, GView::Name());
		if (s)
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
		COLOUR b = Rgb24(255, 255, 231);
		
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

class NativeTipThread : public GThread
{
	bool Loop;
	List<NativeTip> *Tips;

public:
	NativeTipThread(List<NativeTip> *tips) : GThread("NativeTipThread")
	{
		Loop = true;
		Tips = tips;
		
		#if defined(BEOS) || defined(__GTK_H__)
		printf("Warning: No NativeTipThread support.\n");
		#else
		Run();
		#endif
	}
	
	~NativeTipThread()
	{
		Loop = false;
		#if defined(BEOS) || defined(__GTK_H__)
		#else
		while (!IsExited())
		{
			LgiSleep(25);
		}
		#endif
	}
	
	int Main()
	{
		GMouse Old;

		// This thread sit and watches the mouse position waiting for changes.
		// Once the mouse moves, it checks all the watch areas on the tool tips
		// and if the mouse has moved in or out of any of them it updates the
		// tool tips visibility state.
		//
		// Most OS's don't provide an easier way to get all mouse move events,
		// but if your target OS's does, then you might want to use that instead
		// of chewing cycles by polling.		
		while (Loop)
		{
			NativeTip *t = Tips->First();
			if (t)
			{
				GMouse m;
				t->GetMouse(m, true);
				
				// Check if mouse moved
				if (m.x != Old.x ||
					m.y != Old.y)
				{
					// Go through all the tips and see if anything should change
					for (t = Tips->First(); t; t = Tips->Next())
					{
						// Is the mouse over the watch region?
						GRect w = t->Watch;
						GdcPt2 p(0, 0);
						t->Owner->PointToScreen(p);
						w.Offset(p.x, p.y);
						
						bool Over = w.Overlap(m.x, m.y) AND t->IsOverParent(m.x, m.y);
						if (Over ^ t->Visible())
						{
							if (Over)
							{
								GRect r(0, 0, t->X()-1, t->Y()-1);
								r.Offset(w.x1 + w.X()/2, w.y2 + 5);
								t->SetPos(r);
							}
							
							// Change it's state
							t->Visible(Over);
						}
					}
					
					Old = m;
				}
			}
		
			LgiSleep(50);
		}
		
		return 0;
	}
};

#endif

class GToolTipPrivate
{
public:
	int NextUid;
	
	#if !WIN32NATIVE
	List<NativeTip> Tips;
	GViewI *Parent;
	GThread *Thread;
	#endif
	
	GToolTipPrivate()
	{
		NextUid = 1;
		#if !WIN32NATIVE
		Parent = 0;
		Thread = 0;
		#endif
	}

	~GToolTipPrivate()
	{
		#if !WIN32NATIVE
		DeleteObj(Thread);
		Tips.DeleteObjects();
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

	#if WIN32NATIVE

	if (_View && Name && GetParent())
	{
		TOOLINFOW ti;

		ZeroObj(ti);
		ti.cbSize = sizeof(ti);
		ti.uFlags = TTF_SUBCLASS;
		ti.hwnd = GetParent()->Handle();
		ti.rect = Pos;
		ti.lpszText = LgiNewUtf8To16(Name);
		ti.uId = Status = d->NextUid++;

		int Result = SendMessage(_View, TTM_ADDTOOLW, 0, (LPARAM) &ti);

		DeleteArray(ti.lpszText);
	}
	
	#elif !defined(MAC)
	
	if (ValidStr(Name) AND d->Parent)
	{
		// printf("NewTip('%s',%s)\n", Name, Pos.Describe());
		
		NativeTip *t;
		d->Tips.Insert(t = new NativeTip(d->NextUid++, d->Parent));
		if (t)
		{
			t->Watch = Pos;
			t->Name(Name);
			if (t->Attach(0))
			{
				if (!d->Thread)
				{
					d->Thread = new NativeTipThread(&d->Tips);
				}
			}
			
			Status = true;
		}
	}
	
	#endif

	return Status;
}

void GToolTip::DeleteTip(int Id)
{
	#if WIN32NATIVE

	if (GetParent())
	{
		TOOLINFOW ti;

		ZeroObj(ti);
		ti.cbSize = sizeof(ti);
		ti.hwnd = GetParent()->Handle();
		ti.uId = Id;

		SendMessage(_View, TTM_DELTOOL, 0, (LPARAM) &ti);
	}
	
	#else
	
	for (NativeTip *t = d->Tips.First(); t; t = d->Tips.Next())
	{
		if (t->Id == Id)
		{
			d->Tips.Delete(t);
			DeleteObj(t);
			break;
		}
	}

	#endif
}

bool GToolTip::Attach(GViewI *p)
{
	#if WIN32NATIVE

	if (!_View && p)
	{
		SetParent(p);

		_View = CreateWindowEx(	NULL, TOOLTIPS_CLASS, NULL,
								WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
								CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
								NULL, // p->Handle(),
								NULL,
								NULL,
								NULL);

		if (_View)
		{
			SetWindowPos(		_View,
								HWND_TOPMOST,
								0, 0, 0, 0,
								SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
			return true;
		}
	}
	
	#else
	
	d->Parent = p;

	#endif

	return false;
}
