#include "Lgi.h"
#include "GEdit.h"
#include "GButton.h"
#include "GDisplayString.h"
#include "GTextLabel.h"
#include "GCss.h"
#include "GTableLayout.h"
#include "LDbTable.h"
#include "GXmlTree.h"
#include "GTabView.h"

const char *AppName = "Lgi Test App";

enum Ctrls
{
	IDC_EDIT1 = 100,
	IDC_EDIT2,
	IDC_BLT_TEST,
	IDC_TXT,
};

void GStringTest()
{
	GString a("This<TD>is<TD>a<TD>test");
	a.RFind("<TD>");
	GString f = a(8,10);
	GString end = a(-5, -1);
	
	GString sep(", ");
	GString::Array parts = GString("This is a test").Split(" ");
	GString joined = sep.Join(parts);
	
	GString src("  asdwer   ");
	GString left = src.LStrip();
	GString right = src.RStrip();
	GString both = src.Strip();
	
}

GColourSpace RgbColourSpaces[] =
{
	/*
	CsIndex1,
	CsIndex4,
	CsIndex8,
	CsAlpha8,
	*/
	CsRgb15,
	CsBgr15,
	CsRgb16,
	CsBgr16,
	CsRgb24,
	CsBgr24,
	CsRgbx32,
	CsBgrx32,
	CsXrgb32,
	CsXbgr32,
	CsRgba32,
	CsBgra32,
	CsArgb32,
	CsAbgr32,
	CsBgr48,
	CsRgb48,
	CsBgra64,
	CsRgba64,
	CsAbgr64,
	CsArgb64,
	/*
	CsHls32,
	CsCmyk32,
	*/
	CsNone,
};

class BltTest : public GWindow
{
	struct Test
	{
		GColourSpace Src, Dst;
		GMemDC Result;
		
		void Create()
		{
			if (!Result.Create(16, 16, Dst))
				return;
			for (int y=0; y<Result.Y(); y++)
			{
				Result.Colour(Rgb24(y * 16, 0, 0), 24);
				for (int x=0; x<Result.X(); x++)
				{
					Result.Set(x, y);
				}
			}
			
			GMemDC SrcDC;
			if (!SrcDC.Create(16, 16, Src))
				return;
			
			// Result.Blt(0, 0, &SrcDC);
		}
	};
	
	GArray<Test*> a;

public:
	BltTest()
	{
		Name("BltTest");
		GRect r(0, 0, 1200, 1000);
		SetPos(r);
		MoveToCenter();
		
		if (Attach(0))
		{
			Visible(true);
			
			for (int Si=0; RgbColourSpaces[Si]; Si++)
			{
				for (int Di=0; RgbColourSpaces[Di]; Di++)
				{
					Test *t = new Test;
					t->Src = RgbColourSpaces[Si];
					t->Dst = RgbColourSpaces[Di];
					t->Create();
					a.Add(t);
				}
			}
		}
	}
	
	void OnPaint(GSurface *pDC)
	{
		pDC->Colour(LC_WORKSPACE, 24);
		pDC->Rectangle();
		
		int x = 10;
		int y = 10;
		for (unsigned i=0; i<a.Length(); i++)
		{
			char s[256];
			Test *t = a[i];
			sprintf_s(s, sizeof(s), "%s->%s", GColourSpaceToString(t->Src), GColourSpaceToString(t->Dst));
			GDisplayString ds(SysFont, s);
			ds.Draw(pDC, x, y);
			y += ds.Y();
			if (t->Result.Y())
			{
				pDC->Blt(x, y, &t->Result);
				y += t->Result.Y();
			}
			y += 10;
			if (Y() - y < 100)
			{
				y = 10;
				x += 150;
			}
		}
	}
};

class App : public GWindow
{
	GEdit *e;
	GEdit *e2;
	GTextLabel *Txt;
	GTableLayout *Tbl;

public:
	App()
	{
		e = 0;
		e2 = 0;
		Txt = 0;
		Tbl = 0;

		GRect r(0, 0, 1000, 800);
		SetPos(r);
		Name(AppName);
		MoveToCenter();
		SetQuitOnClose(true);
		
		if (Attach(0))
		{
			#if 1

			GTabView *t = new GTabView(100);
			t->Attach(this);
			t->GetCss(true)->Padding("6px");
			auto *tab = t->Append("First");
			tab->GetCss(true)->FontStyle(GCss::FontStyleItalic);
			tab = t->Append("Second");
			tab->GetCss(true)->FontSize("14pt");
			tab = t->Append("Third");
			tab->GetCss(true)->Color("red");
			t->OnStyleChange();

			#elif 0

			AddView(Tbl = new GTableLayout(100));
			GLayoutCell *c = Tbl->GetCell(0, 0);

			c->Add(Txt = new GTextLabel(IDC_TXT, 0, 0, -1, -1, "This is a test string. &For like\ntesting and stuff. It has multiple\nlines to test wrapping."));
			Txt->SetWrap(true);
			//Txt->GetCss(true)->Color(GCss::ColorDef(GColour::Red));
			// Txt->GetCss(true)->FontWeight(GCss::FontWeightBold);
			// Txt->GetCss(true)->FontStyle(GCss::FontStyleItalic);
			Txt->GetCss(true)->FontSize(GCss::Len("22pt"));
			Txt->OnStyleChange();

			c = Tbl->GetCell(1, 0);
			c->Add(new GEdit(IDC_EDIT1, 0, 0, -1, -1));

			#elif 0

			AddView(e = new GEdit(IDC_EDIT1, 10, 10, 200, 22));
			AddView(e2 = new GEdit(IDC_EDIT1, 10, 50, 200, 22));
			AddView(new GButton(IDC_BLT_TEST, 10, 200, -1, -1, "Blt Test"));
			// e->Focus(true);
			e->Password(true);
			e->SetEmptyText("(this is a test)");

			#endif
			
			AttachChildren();
			Visible(true);
		}
	}

	void OnPosChange()
	{
		if (Tbl)
		{
			GRect c = GetClient();
			c.Size(10, 10);
			Tbl->SetPos(c);
		}
		
		GWindow::OnPosChange();
	}

	void OnPaint(GSurface *pDC)
	{
		pDC->Colour(LC_MED, 24);
		pDC->Rectangle();
	}
	
	int OnNotify(GViewI *Ctrl, int Flags)
	{
		switch (Ctrl->GetId())
		{
			case IDC_BLT_TEST:
				new BltTest();
				break;
		}
		
		return 0;
	}
};

enum EmailFields
{
	M_UID = 100,
	M_FLAGS,
	M_DATE,
	M_LABEL,
	M_COLOUR,
	M_SIZE,
	M_FILENAME,
};

bool DbTesting()
{
	// LDbTable::UnitTests();

	#if 1
	const char *BaseFolder = "C:\\Users\\Matthew\\AppData\\Roaming\\Scribe\\ImapCache\\1434419972\\INBOX";
	#else
	const char *BaseFolder = "C:\\Users\\matthew\\AppData\\Roaming\\Scribe\\ImapCache\\378651814\\INBOX";
	#endif

	GFile::Path FileXml(BaseFolder, "Folder.xml");
	GFile::Path FileDb(BaseFolder, "Folder.db");
	GFile::Path FileDebug(BaseFolder, "Debug.txt");
	GFile In;
	if (!In.Open(FileXml, O_READ))
		return false;	
	
	// Read XML
	GXmlTag r;
	GXmlTree t;
	uint64 Start = LgiMicroTime();
	if (!t.Read(&r, &In))
		return false;
	uint64 XmlReadTime = LgiMicroTime() - Start;


	// Convert XML to DB
	LDbTable Tbl;
	Tbl.AddField(M_UID, GV_INT32);
	Tbl.AddField(M_FLAGS, GV_INT32);
	Tbl.AddField(M_DATE, GV_DATETIME);
	Tbl.AddField(M_LABEL, GV_STRING);
	Tbl.AddField(M_COLOUR, GV_INT32);
	Tbl.AddField(M_SIZE, GV_INT64);
	Tbl.AddField(M_FILENAME, GV_STRING);

	Start = LgiMicroTime();
	GXmlTag *Emails = r.GetChildTag("Emails");
	if (!Emails)
		return false;
	for (GXmlTag *c = Emails->Children.First(); c; c = Emails->Children.Next())
	{
		LDbRow *m = Tbl.NewRow();
		m->SetInt(M_UID, c->GetAsInt("Uid"));
		
		ImapMailFlags Flgs;
		Flgs.Set(c->GetAttr("Flags"));
		m->SetInt(M_FLAGS, Flgs.All);
		
		char *Date = c->GetAttr("Date");
		if (Date)
		{
			LDateTime Dt;
			if (Dt.Decode(Date))
			{
				if (!m->SetDate(M_DATE, &Dt))
				{
					LgiAssert(0);
				}
			}
			else
			{
				LgiAssert(0);
			}
		}
		else LgiAssert(0);
		
		m->SetStr(M_LABEL, c->GetAttr("Label"));
		m->SetInt(M_COLOUR, c->GetAsInt("Colour"));
		m->SetInt(M_SIZE, Atoi(c->GetAttr("Size")));
		m->SetStr(M_FILENAME, GString(c->GetContent()).Strip());
	}
	uint64 ConvertTime = LgiMicroTime() - Start;

	Start = LgiMicroTime();
	if (!Tbl.Serialize(FileDb, true))
		return false;
	uint64 WriteTime = LgiMicroTime() - Start;

	LDbTable Test;
	Start = LgiMicroTime();
	if (!Test.Serialize(FileDb, false))
		return false;
	uint64 ReadTime = LgiMicroTime() - Start;

	Start = LgiMicroTime();
	GAutoPtr<DbArrayIndex> Idx(Test.Sort(M_FILENAME));
	uint64 SortTime = LgiMicroTime() - Start;
	if (Idx)
	{
		GFile Out;
		if (Out.Open(FileDebug, O_WRITE))
		{
			Out.SetSize(0);
			for (unsigned i=0; i<Idx->Length(); i++)
			{
				GString s = Idx->ItemAt(i)->ToString();
				s += "\n";
				Out.Write(s);
			}
			Out.Close();
		}
	}

	LgiTrace("DbTest: %i -> %i\n"
		"\tXmlReadTime=%.3f\n"
		"\tConvertTime=%.3f\n"
		"\tWriteTime=%.3f\n"
		"\tReadTime=%.3f\n"
		"\tSortTime=%.3f\n",
		Tbl.GetRows(), Test.GetRows(),
		(double)XmlReadTime/1000.0,
		(double)ConvertTime/1000.0,
		(double)WriteTime/1000.0,
		(double)ReadTime/1000.0,
		(double)SortTime/1000.0);

	return true;
}

int LgiMain(OsAppArguments &AppArgs)
{
	GApp a(AppArgs, "Lgi Test");
	if (a.IsOk())
	{
		// GStringTest();
		// DbTesting();

		a.AppWnd = new App;
		a.Run();
	}
	return 0;
}
