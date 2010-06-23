#include <stdio.h>
#include "Lgi.h"
#include "LgiIde.h"
#include "GToken.h"
#include "GEdit.h"

#define IDC_LIST 100

class DumpItem : public GListItem
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
				LgiFormatSize(s, Size);
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

int Cmp(GListItem *A, GListItem *B, int d)
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

class DumpView : public GWindow
{
	AppWnd *App;
	GList *Lst;
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

			Split->SetViewA(Lst = new GList(IDC_LIST, 0, 0, 100, 100), false);
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
					case GLIST_NOTIFY_SELECT:
					{
						GListItem *s = Lst->GetSelected();
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
					case GLIST_NOTIFY_COLS_CLICK:
					{
						int Col;
						GMouse m;
						if (Lst->GetColumnClickInfo(Col, m))
						{
							Lst->Sort(Cmp, Col);
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
		GHashTable Except;
		Except.Add("GString.cpp");
		Except.Add("GContainers.cpp");
		Except.Add("GContainers.h");
		Except.Add("GFile.cpp");
		Except.Add("Mail.h");

		char *Txt = ReadTextFile(File);
		if (Txt)
		{
			GToken t(Txt, "\r\n");
			DeleteArray(Txt);

			char s[512];
			char *Blocks = t[0];
			if (Blocks)
			{
				char *c = strchr(Blocks, ':');
				if (c) *c = 0;
				sprintf(s, "%s (%s)", File, t[0]);
				Name(s);

				int Size = 0;
				GArray<char*> Stack;
				GHashTable h;
				h.SetStringPool(true);

				for (int i=1; i<t.Length(); i++)
				{
					char *n = t[i];
					if (*n == '\t')
					{
						Stack.Add(n);
					}
					else
					{
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

				List<GListItem> Items;
				for (void *p = h.First(); p; p = h.Next())
				{
					Items.Insert((DumpItem*)p);
				}
				Lst->Insert(Items);
				Lst->Sort(Cmp, 0);
			}
		}
	}
};

void NewMemDumpViewer(AppWnd *App, char *File)
{
	new DumpView(App, File);
}
