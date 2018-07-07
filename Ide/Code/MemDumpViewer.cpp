#include <stdio.h>
#include "Lgi.h"
#include "LgiIde.h"
#include "GToken.h"
#include "GEdit.h"
#include "GProgressDlg.h"
#include "LList.h"

#define IDC_LIST 100

class DumpItem : public LListItem
{
public:
	int Size;
	int Count;
	char *Alloc;
	char *Stack;

	DumpItem()
	{
		Size = Count = 0;
		Alloc = Stack = 0;
	}

	~DumpItem()
	{
		DeleteArray(Alloc);
		DeleteArray(Stack);
	}

	char *GetText(int c)
	{
		static char s[64];
		switch (c)
		{
			case 0:
			{
				LgiFormatSize(s, sizeof(s), Size);
				return s;
				break;
			}
			case 1:
			{
				return Alloc;
				break;
			}
			case 2:
			{
				sprintf(s, "%i", Count);
				return s;
				break;
			}
		}

		return 0;
	}
};

int Cmp(LListItem *A, LListItem *B, NativeInt d)
{
	DumpItem *a = (DumpItem*) A;
	DumpItem *b = (DumpItem*) B;

	switch (d)
	{
		case 0:
		{
			return b->Size - a->Size;
		}
		case 1:
		{
			return stricmp(a->Alloc, b->Alloc);
		}
		case 2:
		{
			return b->Count - a->Count;
		}
	}

	return 0;
}

char *Strnstr(char *s, const char *find, int len)
{
    if (!s || !find || len < 0)
        return 0;

    char *End = s + len;
    int FindLen = strlen(find);
    while (s < End)
    {
        if (*s == *find)
        {
            bool match = true;
            char *to = s + FindLen;
            if (to > End)
                return 0;
            
            for (int n=1; n<FindLen; n++)
            {
                if (s[n] != find[n])
                {
                    match = false;
                    break;
                }
            }
            if (match)
                return s;
        }
        s++;
    }
    
    return 0;
}

class DumpView : public GWindow
{
	AppWnd *App;
	LList *Lst;
	GEdit *Ed;

public:
	DumpView(AppWnd *app, char *file)
	{
		App = app;
		Name("Memory Dump Viewer");
		GRect r(0, 0, 800, 600);
		SetPos(r);
		MoveToCenter();
		if (Attach(0))
		{
			GSplitter *Split;
			Children.Insert(Split = new GSplitter);
			Split->Value(400);
			Split->IsVertical(false);

			Split->SetViewA(Lst = new LList(IDC_LIST, 0, 0, 100, 100), false);
			Lst->AddColumn("Size", 200);
			Lst->AddColumn("Location", 300);
			Lst->AddColumn("Count", 100);

			Split->SetViewB(Ed = new GEdit(101, 0, 0, 100, 100, ""), false);
			Ed->Enabled(false);
			Ed->MultiLine(true);

			AttachChildren();
			Visible(true);

			if (file)
				Load(file);
			else
			{
				GFileSelect s;
				s.Parent(this);
				s.Type("Dump", "*.mem");
				s.Type("All Files", LGI_ALL_FILES);
				if (s.Open())
				{
					Load(s.Name());
				}
			}
		}
	}

	int OnNotify(GViewI *c, int f)
	{
		switch (c->GetId())
		{
			case IDC_LIST:
			{
				switch (f)
				{
					case GNotifyItem_Select:
					{
						LListItem *s = Lst->GetSelected();
						if (s)
						{
							DumpItem *di = dynamic_cast<DumpItem*>(s);
							if (di && Ed)
							{
								Ed->Name(di->Stack);
							}
						}
						break;
					}
					case GNotifyItem_ColumnClicked:
					{
						int Col;
						GMouse m;
						if (Lst->GetColumnClickInfo(Col, m))
						{
							Lst->Sort<NativeInt>(Cmp, Col);
						}
						break;
					}
				}
				break;
			}
		}

		return 0;
	}

	void Load(char *File)
	{
		GHashTbl<const char*, bool> Except(0, false);
		Except.Add("GString.cpp", true);
		Except.Add("GVariant.cpp", true);
		Except.Add("GContainers.cpp", true);
		Except.Add("GContainers.h", true);
		Except.Add("GFile.cpp", true);
		Except.Add("Mail.h", true);
		Except.Add("GArray.h", true);

		GFile f;
		if (!f.Open(File, O_READ))
			LgiMsg(this, "Couldn't read '%s'", AppName, MB_OK, File);
		else
		{
			GProgressDlg Prog(this, true);
			GArray<char> Buf;
			Buf.Length(1 << 20);
			int Pos = 0, Used = 0;
			bool First = true;
			char s[512];

			GHashTbl<char*,DumpItem*> h(0, false);
			h.SetStringPool(true);

			Prog.SetDescription("Reading memory dump...");
			Prog.SetLimits(0, f.GetSize());
			Prog.SetScale(1.0 / 1024.0 / 1024.0);
			Prog.SetType("MB");

			while (true)
			{
				// Consume data
				int Len = Used - Pos;
				char *Cur = &Buf[Pos];
				char *End = Strnstr(Cur, "\r\n\r\n", Len);
				if (End)
				{
					if (First)
					{
						First = false;
						GToken t(Cur, " \t\r\n", true, End-Cur);
						char *Blocks = t[0];
						if (Blocks)
						{
							char *c = strchr(Blocks, ':');
							if (c) *c = 0;
							sprintf(s, "%s (%s)", File, t[0]);
							Name(s);
						}
						else break;
					}
					else
					{
						int Size = 0;
						GToken Lines(Cur, "\r\n", true, End - Cur);
						GArray<char*> Stack;
						for (int i=0; i<Lines.Length(); i++)
						{
							char *n = Lines[i];
							if (*n == '\t')
							{
								Stack.Add(n);
							}
							else
							{
								if (strnicmp(n, "Block ", 6) == 0)
								{
									char *c = strchr(n, ',');
									if (c)
									{
										c++;
										while (*c == ' ') c++;

										Size = atoi(c);
									}
								}
							}
						}

						if (Size && Stack.Length() > 0)
						{
							char *Alloc = 0;

							for (int k=0; k<Stack.Length(); k++)
							{
								char *sp = strchr(Stack[k], ' ');
								if (sp++)
								{
									char *col = strrchr(sp, ':');
									if (col) *col = 0;

									char *f = strrchr(sp, DIR_CHAR);
									if (!f) f = sp;
									else f++;

									if (!Except.Find(f))
									{
										if (col)
											*col = ':';
										Alloc = f;
										break;
									}

									*col = ':';
								}
							}

							if (Alloc)
							{
								DumpItem *di = (DumpItem*) h.Find(Alloc);
								if (!di)
								{
									di = new DumpItem;
									if (di)
									{
										h.Add(Alloc, di);
										di->Alloc = NewStr(Alloc);

										GStringPipe p;
										for (int k=0; k<Stack.Length(); k++)
										{
											p.Print("%s\n", Stack[k] + 1);
										}

										di->Stack = p.NewStr();
									}
								}
								if (di)
								{
									di->Count++;
									di->Size += Size;
								}
							}
							else
							{
								LgiAssert(0);
							}

							Stack.Length(0);
							Size = 0;
						}
					}

					Pos = End - &Buf[0] + 4;
				}
				else
				{
					// Update status
					Prog.Value(f.GetPos());
					LgiYield();

					// Read more data in?
					memmove(&Buf[0], &Buf[Pos], Used - Pos);
					Used -= Pos;
					Pos = 0;
					int r = f.Read(&Buf[Used], Buf.Length() - Used);
					if (r <= 0)
						break;
					Used += r;
				}
			}

			List<LListItem> Items;
			// for (void *p = h.First(); p; p = h.Next())
			for (auto p : h)
			{
				Items.Insert((DumpItem*)p.value);
			}
			Lst->Insert(Items);
			Lst->Sort(Cmp);
		}
	}
};

void NewMemDumpViewer(AppWnd *App, char *File)
{
	new DumpView(App, File);
}
