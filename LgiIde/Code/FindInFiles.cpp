#include <stdio.h>
#include <ctype.h>

#include "Lgi.h"
#include "LgiIde.h"
#include "GProcess.h"
#include "GDocView.h"
#include "GToken.h"
#include "GEdit.h"
#include "GCheckBox.h"
#include "GTextLabel.h"

#define DEBUG_HIST		1

/////////////////////////////////////////////////////////////////////////////////////
FindInFiles::FindInFiles(AppWnd *app, FindParams *params)
{
	TypeHistory = 0;
	FolderHistory = 0;

	SetParent(App = app);
	
	if (params)
	{
		Params = params;
		OwnParams = false;
	}
	else
	{
		Params = new FindParams;
		OwnParams = true;
	}

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
	if (OwnParams)
		DeleteObj(Params);
}

void SerializeHistory(GHistory *h, const char *opt, GOptionsFile *p, bool Write)
{
	if (h && p)
	{
		GString last;
		last.Printf("%sSelect", opt);
		
		#if DEBUG_HIST
		LgiTrace("%s:%i - SerializeHistory '%s'\n", _FL, last.Get());
		#endif

		GVariant v;
		if (Write)
		{
			GString::Array a;
			a.SetFixedLength(false);
			int i=0;
			for (char *s=h->First(); s; s=h->Next(), i++)
			{
				a.Add(s);
				#if DEBUG_HIST
				LgiTrace("\t[%i]='%s'\n", i, s);
				#endif
			}

			GString strs = GString("|").Join(a);
			p->SetValue(opt, v = strs.Get());
			#if DEBUG_HIST
			LgiTrace("\tstrs='%s'\n", strs.Get());
			#endif
			
			p->SetValue(last, v = h->Value());
			#if DEBUG_HIST
			LgiTrace("\tv=%i\n", v.CastInt32());
			#endif
		}
		else
		{
			if (p->GetValue(opt, v))
			{
				GString raw(v.Str());
				GString::Array t = raw.Split("|");
				h->DeleteArrays();
				for (unsigned i=0; i<t.Length(); i++)
				{
					h->Insert(NewStr(t[i]));
					#if DEBUG_HIST
					LgiTrace("\t[%i]='%s'\n", i, t[i].Get());
					#endif
				}
				h->Update();
				
				if (p->GetValue(last, v))
				{
					h->Value(v.CastInt64());
					#if DEBUG_HIST
					LgiTrace("\tValue=%i\n", v.CastInt32());
					#endif
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

		GEdit *v;
		if (GetViewById(IDC_LOOK_FOR, v))
		{
			v->Focus(true);
			v->SelectAll();
		}
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
		case IDCANCEL:
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
			
			EndModal(v->GetId() == IDOK);
			break;
		}
	}
	
	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////
class FindInFilesThreadPrivate
{
public:
	int AppHnd;
	GAutoPtr<FindParams> Params;
	bool Loop, Busy;
	GStringPipe Pipe;
	int64 Last;
};

FindInFilesThread::FindInFilesThread(int AppHnd) : GEventTargetThread("FindInFiles")
{
	d = new FindInFilesThreadPrivate;
	d->AppHnd = AppHnd;
	d->Loop = true;
	d->Busy = false;
	d->Last = 0;
}

FindInFilesThread::~FindInFilesThread()
{
	DeleteObj(d);
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
								GEventSinkMap::Dispatch.PostEvent(d->AppHnd, M_APPEND_TEXT, (GMessage::Param)d->Pipe.NewStr(), 2);
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

void FindInFilesThread::Stop()
{
	d->Loop = false;
	while (d->Busy)
		LgiSleep(1);
}

GMessage::Result FindInFilesThread::OnEvent(GMessage *Msg)
{
	switch (Msg->Msg())
	{
		case M_START_SEARCH:
		{
			d->Params.Reset((FindParams*)Msg->A());
			if (d->AppHnd &&
				d->Params &&
				ValidStr(d->Params->Text))
			{
				char Msg[256];

				d->Loop = true;
				d->Busy = true;
				snprintf(Msg, sizeof(Msg), "Searching for '%s'...\n", d->Params->Text.Get());
				GEventSinkMap::Dispatch.PostEvent(d->AppHnd, M_APPEND_TEXT, 0, 2);
				GEventSinkMap::Dispatch.PostEvent(d->AppHnd, M_APPEND_TEXT, (GMessage::Param)NewStr(Msg), 2);

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
						if (p)
						{
							const char *Leaf = LgiGetLeaf(p);
							for (unsigned n=0; n<Ext.Length(); n++)
							{
								if (MatchStr(Ext[n], Leaf))
								{
									char *np = NewStr(p);
									if (np)
										Files.Add(np);
									else
										LgiTrace("%s:%i - Can't dup '%s'\n", _FL, p.Get());
								}
							}
						}
						else LgiTrace("%s:%i - Null string in project files array.\n", _FL);
					}
				}
				else
				{
					// Find the files recursively...
					LgiRecursiveFileSearch(d->Params->Dir, &Ext, &Files, 0, 0, FindInFilesCallback);
				}

				if (Files.Length() > 0)
				{			
					sprintf(Msg, "in %i files...\n", (int)Files.Length());
					GEventSinkMap::Dispatch.PostEvent(d->AppHnd, M_APPEND_TEXT, (GMessage::Param)NewStr(Msg), 2);

					for (int i=0; i<Files.Length() && d->Loop; i++)
					{
						char *f = Files[i];
						if (f)
						{
							char *Dir = strrchr(f, DIR_CHAR);
							if (!Dir || Dir[1] != '.')
							{
								/*
								sprintf(Msg, "%s\n", f);
								GEventSinkMap::Dispatch.PostEvent(d->AppHnd, M_APPEND_TEXT, (GMessage::Param)NewStr(Msg), 2);
								*/

								SearchFile(f);
							}
						}
						else LgiTrace("%s:%i - NULL Ptr in list????", _FL);
					}
			
					char *Str = d->Pipe.NewStr();
					if (Str)
					{
						GEventSinkMap::Dispatch.PostEvent(d->AppHnd, M_APPEND_TEXT, (GMessage::Param)Str, 2);
					}
			
					Files.DeleteArrays();

					GEventSinkMap::Dispatch.PostEvent(d->AppHnd, M_APPEND_TEXT, (GMessage::Param)NewStr("Done.\n"), 2);
				}
				else
				{
					GEventSinkMap::Dispatch.PostEvent(d->AppHnd, M_APPEND_TEXT, (GMessage::Param)NewStr("No files matched.\n"), 2);
				}
				
				d->Busy = false;
			}
			break;
		}
	}

	return 0;
}
