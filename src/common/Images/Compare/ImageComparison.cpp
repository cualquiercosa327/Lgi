#include "Lgi.h"
#include "GList.h"
#include "GCombo.h"
#include "ImageComparison.h"
#include "GTabView.h"
#include "resdefs.h"

struct CompareThread : public GThread
{
	GList *lst;
	bool loop;
	
	CompareThread(GList *l) : GThread("CompareThread")
	{
		lst = l;
		loop = true;
		Run();
	}
	
	~CompareThread()
	{
		loop = false;
		while (!IsExited())
			LgiSleep(1);
	}
	
	int Main()
	{
		List<GListItem> items;
		lst->GetAll(items);
		List<GListItem>::I it = items.Start();
		
		while (loop)
		{
			GListItem *i = *it;

			if (i)
			{
				char *left = i->GetText(0);
				char *right = i->GetText(1);
				GAutoPtr<GSurface> left_img(LoadDC(left));
				GAutoPtr<GSurface> right_img(LoadDC(right));
				if (left_img && right_img)
				{
					if (left_img->X() == right_img->X() &&
						left_img->Y() == right_img->Y() &&
						left_img->GetColourSpace() == right_img->GetColourSpace())
					{
						bool diff = false;
						int bytes = (left_img->X() * left_img->GetBits()) / 8;
						for (int y=0; y<left_img->Y(); y++)
						{
							uint8 *left_scan = (*left_img)[y];
							uint8 *right_scan = (*right_img)[y];
							if (memcmp(left_scan, right_scan, bytes))
							{
								diff = true;
								break;
							}
						}
						
						i->SetText(diff ? "Different" : "Same", 2);
					}
					else
					{
						i->SetText("SizeDiff", 2);
					}
				}
				else
				{
					i->SetText("Failed", 2);
				}
				i->Update();
			}
			else break;
						
			it++;
		}

		return 0;
	}
};

struct ImageCompareDlgPriv
{
	GCombo *l, *r;
	GList *lst;
	GTabView *tabs;
	GAutoPtr<class CompareThread> Thread;

	ImageCompareDlgPriv()
	{
		l = r = NULL;
		tabs = NULL;
	}
};

ImageCompareDlg::ImageCompareDlg(GView *p, const char *OutPath)
{
	d = new ImageCompareDlgPriv();
	SetParent(p);
	GRect r(0, 0, 1000, 800);
	SetPos(r);
	MoveToCenter();

	GAutoString ResFile(LgiFindFile("ImageComparison.lr8"));
	LgiAssert(ResFile);
	if (ResFile)
	{
		AddView(d->tabs = new GTabView(10));
		d->tabs->SetPourLargest(true);
		GTabPage *First = d->tabs->Append("Select");
		
		LgiResources *Res = LgiGetResObj(false, ResFile);
		LgiAssert(Res);
		if (Res && Res->LoadDialog(IDD_COMPARE, First))
		{
			MoveToCenter();
			
			if (GetViewById(IDC_LEFT, d->l) &&
				GetViewById(IDC_RIGHT, d->r) &&
				GetViewById(IDC_LIST, d->lst))
			{			
				GDirectory dir;
				for (bool b = dir.First(OutPath); b; b = dir.Next())
				{
					if (dir.IsDir())
					{
						char p[MAX_PATH];
						if (dir.Path(p, sizeof(p)))
						{
							d->l->Insert(p);
							d->r->Insert(p);
						}
					}
				}
				
				d->l->Value(0);
				d->r->Value(1);
			}
		}
	}
}

ImageCompareDlg::~ImageCompareDlg()
{
	DeleteObj(d);
}

int ImageCompareDlg::OnNotify(GViewI *Ctrl, int Flags)
{
	switch (Ctrl->GetId())
	{
		case IDC_LIST:
		{
			if (Flags == GLIST_NOTIFY_DBL_CLICK)
			{
				GListItem *s = d->lst->GetSelected();
				if (s)
				{
					char *left = s->GetText(0);
					char *right = s->GetText(1);
					char p[MAX_PATH];
					LgiGetSystemPath(LSP_APP_INSTALL, p, sizeof(p));
					LgiMakePath(p, sizeof(p), p, "../../../../i.Mage/trunk/Win32Debug/image.exe");
					if (FileExists(p))
					{
						char args[MAX_PATH];
						sprintf_s(args, sizeof(args), "\"%s\" \"%s\"", left, right);
						LgiExecute(p, args);
					}
				}
			}
			break;
		}
		case IDCANCEL:
		{
			EndModal();
			break;
		}
		case IDC_COMPARE:
		{
			GHashTbl<char*, char*> Left;
			GDirectory LDir, RDir;
			char p[MAX_PATH];				
			for (bool b=LDir.First(d->l->Name()); b; b=LDir.Next())
			{
				if (LDir.IsDir())
					continue;
				LDir.Path(p, sizeof(p));
				Left.Add(LDir.GetName(), NewStr(p));
			}
			for (bool b=RDir.First(d->r->Name()); b; b=RDir.Next())
			{
				char *LeftFile = Left.Find(RDir.GetName());
				if (LeftFile)
				{
					RDir.Path(p, sizeof(p));
					GListItem *l = new GListItem;
					l->SetText(LeftFile, 0);
					l->SetText(p, 1);
					l->SetText("Processing...", 2);
					d->lst->Insert(l);
				}
			}
			Left.DeleteArrays();
			d->lst->ResizeColumnsToContent();
			d->Thread.Reset(new CompareThread(d->lst));
			break;
		}
	}
	
	return 0;
}

