#include <stdio.h>
#include <ctype.h>

#include "Lgi.h"
#include "LgiIde.h"
#include "GProcess.h"
#include "GDocView.h"
#include "GToken.h"

/////////////////////////////////////////////////////////////////////////////////////
FindInFiles::FindInFiles(AppWnd *app)
{
	TypeHistory = 0;
	FolderHistory = 0;

	SetParent(App = app);
	Params = new FindParams;
	if (LoadFromResource(IDD_FIND_IN_FILES))
	{
		MoveToCenter();

		if (GetViewById(IDC_TYPE_HISTORY, TypeHistory))
			TypeHistory->SetTargetId(IDC_FILE_TYPES);

		if (GetViewById(IDC_FOLDER_HISTORY, FolderHistory))
			FolderHistory->SetTargetId(IDC_DIR);
	}
}

FindInFiles::~FindInFiles()
{
	DeleteObj(Params);
}

void SerializeHistory(GHistory *h, const char *opt, GOptionsFile *p, bool Write)
{
	if (h && p)
	{
		GString last;
		last.Printf("%sSelect", opt);
		
		GVariant v;
		if (Write)
		{
			GStringPipe b;
			for (char *s=h->First(); s; s=h->Next())
			{
				if (b.GetSize()) b.Push("|");
				b.Push(s);
			}
			GAutoString strs;
			if (strs.Reset(b.NewStr()))
			{
				p->SetValue(opt, v = strs.Get());
				p->SetValue(last, v = h->Value());
			}
		}
		else
		{
			if (p->GetValue(opt, v))
			{
				GToken t(v.Str(), "|");
				h->DeleteArrays();
				for (int i=0; i<t.Length(); i++)
				{
					h->Insert(NewStr(t[i]));
				}
				h->Update();
				
				if (p->GetValue(last, v))
				{
					h->Value(v.CastInt64());
					// printf("Selecting '%s' -> %i\n", last.Get(), v.CastInt32());
				}
				else LgiTrace("%s:%i - No option '%s'\n", _FL, last.Get());
			}
		}
	}
}

void FindInFiles::OnCreate()
{
	if (Params)
	{
		SetCtrlValue(IDC_ENTIRE_SOLUTION, Params->Type == FifSearchSolution);
		SetCtrlValue(IDC_SEARCH_DIR, Params->Type == FifSearchDirectory);

		SetCtrlName(IDC_LOOK_FOR, Params->Text);
		SetCtrlName(IDC_FILE_TYPES, Params->Ext);
		SetCtrlName(IDC_DIR, Params->Dir);
		
		SetCtrlValue(IDC_WHOLE_WORD, Params->MatchWord);
		SetCtrlValue(IDC_CASE, Params->MatchCase);
		SetCtrlValue(IDC_SUB_DIRS, Params->SubDirs);
		
		SerializeHistory(TypeHistory, "TypeHist", App->GetOptions(), false);
		SerializeHistory(FolderHistory, "FolderHist", App->GetOptions(), false);
		
		GViewI *v;
		if (GetViewById(IDC_LOOK_FOR, v))
			v->Focus(true);
	}
}

int FindInFiles::OnNotify(GViewI *v, int f)
{
	switch (v->GetId())
	{
		case IDC_SET_DIR:
		{
			GFileSelect s;
			s.Parent(this);
			s.InitialDir(GetCtrlName(IDC_DIR));
			if (s.OpenFolder())
			{
				int Idx = FolderHistory->Add(s.Name());
				if (Idx >= 0)
					FolderHistory->Value(Idx);
				else
					SetCtrlName(IDC_DIR, s.Name());
			}
			break;
		}
		case IDC_ENTIRE_SOLUTION:
		case IDC_SEARCH_DIR:
		{
			bool SearchSol = GetCtrlValue(IDC_ENTIRE_SOLUTION) != 0;
			SetCtrlEnabled(IDC_DIR, !SearchSol);
			SetCtrlEnabled(IDC_SET_DIR, !SearchSol);
			SetCtrlEnabled(IDC_FOLDER_HISTORY, !SearchSol);
			SetCtrlEnabled(IDC_SUB_DIRS, !SearchSol);
			break;
		}
		case IDOK:
		{
			Params->Type = GetCtrlValue(IDC_ENTIRE_SOLUTION) != 0 ? FifSearchSolution :  FifSearchDirectory;
			Params->Text = GetCtrlName(IDC_LOOK_FOR);
			Params->Ext = GetCtrlName(IDC_FILE_TYPES);
			Params->Dir = GetCtrlName(IDC_DIR);
			
			Params->MatchWord = GetCtrlValue(IDC_WHOLE_WORD);
			Params->MatchCase = GetCtrlValue(IDC_CASE);
			Params->SubDirs = GetCtrlValue(IDC_SUB_DIRS);
		
			if (TypeHistory) TypeHistory->Add(Params->Ext);
			SerializeHistory(TypeHistory, "TypeHist", App->GetOptions(), true);
			if (FolderHistory) FolderHistory->Add(Params->Dir);
			SerializeHistory(FolderHistory, "FolderHist", App->GetOptions(), true);
			
			EndModal(1);
			break;
		}
		case IDCANCEL:
		{
			EndModal(0);
			break;
		}
	}
	
	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////
class FindInFilesThreadPrivate
{
public:
	AppWnd *App;
	FindParams *Params;
	bool Loop;
	GStringPipe Pipe;
	int64 Last;
};

FindInFilesThread::FindInFilesThread(AppWnd *App, FindParams *Params) : GThread("FindInFilesThread")
{
	d = new FindInFilesThreadPrivate;
	d->App = App;
	d->Params = Params;
	d->Loop = true;
	d->Last = 0;
	
	DeleteOnExit = true;	
	Run();
}

FindInFilesThread::~FindInFilesThread()
{
	DeleteObj(d->Params);
	DeleteObj(d);
}

void FindInFilesThread::Stop()
{
	d->Loop = false;
}

void FindInFilesThread::SearchFile(char *File)
{
	if (File && ValidStr(d->Params->Text))
	{
		char *Doc = ReadTextFile(File);
		if (Doc)
		{
			int Len = d->Params->Text.Length();
			const char *Text = d->Params->Text;
			
			char *LineStart = 0;
			int Line = 0;
			for (char *s = Doc; *s && d->Loop; s++)
			{
				if (*s == '\n')
				{
					Line++;
					LineStart = 0;
				}
				else
				{
					if (!LineStart)
						LineStart = s;
						
					bool Match = false;
					if (d->Params->MatchCase)
					{
						if (Text[0] == *s)
						{
							Match = strncmp(s, Text, Len) == 0;
						}
					}
					else
					{
						if (toupper(Text[0]) == toupper(*s))
						{
							Match = strnicmp(s, Text, Len) == 0;
						}
					}
					
					if (Match)
					{
						char *Eol = s + Len;
						while (*Eol && *Eol != '\n') Eol++;
						int LineLen = Eol - LineStart;						
						
						bool StartOk = true;
						bool EndOk = true;
						if (d->Params->MatchWord)
						{
							if (s > Doc)
							{
								StartOk = IsWordBoundry(s[-1]);
							}
							
							EndOk = IsWordBoundry(s[Len]);
						}
						
						if (StartOk && EndOk)
						{
							static char Buf[1024];
							int Chars = snprintf(Buf, sizeof(Buf), "%s:%i:%.*s\n", File, Line + 1, LineLen, LineStart);
							d->Pipe.Push(Buf, Chars);
							
							int64 Now = LgiCurrentTime();
							if (Now > d->Last  + 500)
							{
								d->App->PostEvent(M_APPEND_TEXT, (GMessage::Param)d->Pipe.NewStr(), 2);
							}
						}
						s = Eol - 1;
					}
				}
			}

			DeleteArray(Doc);
		}
		else if (LgiFileSize(File) > 0)
		{
			LgiTrace("%s:%i - Couldn't Read file '%s'\n", _FL, File);
		}
	}
}

bool FindInFilesCallback(void *UserData, char *Path, GDirectory *Dir)
{
	if (Dir->IsDir())
	{
		char *p = Dir->GetName();
		if
		(
			p
			&&
			(
				stricmp(p, ".svn") == 0
				||
				stricmp(p, ".git") == 0
			)
		)
			return false;
	}
	
	return true;
}

int FindInFilesThread::Main()
{
	if (d->App &&
		d->Params &&
		ValidStr(d->Params->Text))
	{
		char Msg[256];

		snprintf(Msg, sizeof(Msg), "Searching for '%s'...\n", d->Params->Text.Get());
		d->App->PostEvent(M_APPEND_TEXT, 0, 2);
		d->App->PostEvent(M_APPEND_TEXT, (GMessage::Param)NewStr(Msg), 2);

		GArray<const char*> Ext;
		GToken e(d->Params->Ext, ";, ");
		for (int i=0; i<e.Length(); i++)
		{
			Ext.Add(e[i]);
		}
		
		GArray<char*> Files;
		if (d->Params->Type == FifSearchSolution)
		{
			// Do the extension filtering...
			for (unsigned i=0; i<d->Params->ProjectFiles.Length(); i++)
			{
				GString p = d->Params->ProjectFiles[i];
				const char *Leaf = LgiGetLeaf(p);
				for (unsigned n=0; n<Ext.Length(); n++)
				{
					if (MatchStr(Ext[n], Leaf))
						Files.Add(NewStr(p));
				}
			}
		}
		else
		{
			// Find the files recursively...
			LgiRecursiveFileSearch(d->Params->Dir, &Ext, &Files, 0, 0, FindInFilesCallback);
		}

		if (Files.Length() > 0)
		{			
			sprintf(Msg, "in %i files...\n", Files.Length());
			d->App->PostEvent(M_APPEND_TEXT, (GMessage::Param)NewStr(Msg), 2);

			for (int i=0; i<Files.Length() && d->Loop; i++)
			{
				char *f = Files[i];
				char *Dir = strrchr(f, DIR_CHAR);
				if (!Dir || Dir[1] != '.')
				{
					SearchFile(f);
				}
			}
			
			char *Str = d->Pipe.NewStr();
			if (Str)
			{
				d->App->PostEvent(M_APPEND_TEXT, (GMessage::Param)Str, 2);
			}
			
			Files.DeleteArrays();

			d->App->PostEvent(M_APPEND_TEXT, (GMessage::Param)NewStr("Done.\n"), 2);
		}
		else
		{
			d->App->PostEvent(M_APPEND_TEXT, (GMessage::Param)NewStr("No files matched.\n"), 2);
		}
	}

	d->App->OnFindFinished();
	return 0;
}
