//
//  GWindowCommon.cpp
//  LgiCarbon
//
//  Created by Matthew Allen on 11/09/14.
//
//
#include "Lgi.h"
#include "INet.h"

void GWindow::MoveOnScreen()
{
	GRect p = GetPos();
	GArray<GDisplayInfo*> Displays;
	GRect Screen(0, 0, -1, -1);

	if (
		#if WINNATIVE
		!IsZoomed(Handle()) &&
		!IsIconic(Handle()) &&
		#endif
		LgiGetDisplays(Displays))
	{
		int Best = -1;
		int Pixels = 0;
		int Close = 0x7fffffff;
		for (int i=0; i<Displays.Length(); i++)
		{
			GRect o = p;
			o.Bound(&Displays[i]->r);
			if (o.Valid())
			{
				int Pix = o.X()*o.Y();
				if (Best < 0 || Pix > Pixels)
				{
					Best = i;
					Pixels = Pix;
				}
			}
			else if (Pixels == 0)
			{
				int n = Displays[i]->r.Near(p);
				if (n < Close)
				{
					Best = i;
					Close = n;
				}
			}
		}

		if (Best >= 0)
			Screen = Displays[Best]->r;
	}

	if (!Screen.Valid())
		Screen.Set(0, 0, GdcD->X()-1, GdcD->Y()-1);

	if (p.x2 >= Screen.x2)
		p.Offset(Screen.x2 - p.x2, 0);
	if (p.y2 >= Screen.y2)
		p.Offset(0, Screen.y2 - p.y2);
	if (p.x1 < Screen.x1)
		p.Offset(Screen.x1 - p.x1, 0);
	if (p.y1 < Screen.y1)
		p.Offset(0, Screen.y1 - p.y1);

	SetPos(p, true);

	Displays.DeleteObjects();
}

void GWindow::MoveToCenter()
{
	GRect Screen(0, 0, GdcD->X()-1, GdcD->Y()-1);
	GRect p = GetPos();

	p.Offset(-p.x1, -p.y1);
	p.Offset((Screen.X() - p.X()) / 2, (Screen.Y() - p.Y()) / 2);

	SetPos(p, true);
}

void GWindow::MoveToMouse()
{
	GMouse m;
	if (GetMouse(m, true))
	{
		GRect p = GetPos();

		p.Offset(-p.x1, -p.y1);
		p.Offset(m.x-(p.X()/2), m.y-(p.Y()/2));

		SetPos(p, true);
		MoveOnScreen();
	}
}

bool GWindow::MoveSameScreen(GViewI *wnd)
{
	if (!wnd)
	{
		LgiAssert(0);
		return false;
	}
	
	GRect p = wnd->GetPos();
	int cx = p.x1 + (p.X() >> 4);
	int cy = p.y1 + (p.Y() >> 4);
	
	GRect np = GetPos();
	np.Offset(cx - np.x1, cy - np.y1);
	SetPos(np);
	
	MoveOnScreen();
	return true;
}

int GWindow::WillAccept(List<char> &Formats, GdcPt2 Pt, int KeyState)
{
	int Status = DROPEFFECT_NONE;

	#if DEBUG_DND	
	LgiTrace("%s:%i - WillAccept Formats=%i Pt=%i,%i Key=0x%x\n", _FL, Formats.Length(), Pt.x, Pt.y, KeyState);
	#endif
	for (char *f=Formats.First(); f; )
	{
		#if DEBUG_DND
		LgiTrace("\tFmt=%s\n", f);
		#endif
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

int GWindow::OnDrop(GArray<GDragData> &Data, GdcPt2 Pt, int KeyState)
{
	int Status = DROPEFFECT_NONE;
	
	#ifdef DEBUG_DND	
	LgiTrace("%s:%i - OnDrop Data=%i Pt=%i,%i Key=0x%x\n", _FL, Data.Length(), Pt.x, Pt.y, KeyState);
	#endif
	for (unsigned i=0; i<Data.Length(); i++)
	{
		GDragData &dd = Data[i];

		#ifdef DEBUG_DND	
		LgiTrace("\tFmt=%s\n", dd.Format.Get());
		#endif
		
		if (dd.IsFileDrop())
		{
			GArray<char*> Files;
			GString::Array Uri;
			
			for (unsigned n=0; n<dd.Data.Length(); n++)
			{
				GVariant *Data = &dd.Data[n];
				if (Data->IsBinary())
				{
					Uri.New().Set((char*)Data->Value.Binary.Data, Data->Value.Binary.Length);
				}
				else if (Data->Str())
				{
					Uri.New() = Data->Str();
				}
				else if (Data->Type == GV_LIST)
				{
					for (GVariant *v=Data->Value.Lst->First(); v; v=Data->Value.Lst->Next())
					{
						char *f = v->Str();
						Uri.New() = f;
					}
				}
			}
			
			for (int i=0; i<Uri.Length(); i++)
			{
				char *File = Uri[i].Strip().Get();
				GUri u(File);
				/*
				if (strnicmp(File, "file:", 5) == 0)
					File += 5;
				if (strnicmp(File, "//localhost", 11) == 0)
					File += 11;
				*/
				
				char *in = u.Path, *out = u.Path;
				while (*in)
				{
					if (in[0] == '%' &&
						in[1] &&
						in[2])
					{
						char h[3] = { in[1], in[2], 0 };
						*out++ = htoi(h);
						in += 3;
					}
					else
					{
						*out++ = *in++;
					}
				}
				*out++ = 0;
				
				if (FileExists(u.Path))
				{
					Files.Add(NewStr(u.Path));
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
