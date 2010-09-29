// BeOS Implementation of General LGI functions
#include <NodeInfo.h>
#include <Path.h>
#include "Lgi.h"

////////////////////////////////////////////////////////////////
// Local helper functions
bool _lgi_check_file(char *Path)
{
	if (Path)
	{
		if (FileExists(Path))
		{
			// file is there
			return true;
		}
		else
		{
			// shortcut?
			char *e = Path + strlen(Path);
			strcpy(e, ".lnk");
			if (FileExists(Path))
			{
				// resolve shortcut
				char Link[256];
				if (ResolveShortcut(Path, Link, sizeof(Link)))
				{
					// check destination of link
					if (FileExists(Link))
					{
						strcpy(Path, Link);
						return true;
					}
				}
			}
			*e = 0;
		}
	}

	return false;
}

long _lgi_play_sound(void *Ptr)
{
	BEntry Entry((char*)Ptr);
	entry_ref Ref;
	if (Entry.GetRef(&Ref) == B_OK)
	{
		BSimpleGameSound Sound(&Ref);
		if (Sound.InitCheck() == B_OK)
		{
			Sound.StartPlaying();
			while (Sound.IsPlaying())
			{
			}
			return true;
		}
	}
	
	return false;
}

////////////////////////////////////////////////////////////////////////
// Implementations
bool LgiGetFileMimeType(char *File, char *Mime)
{
	bool Status = false;
	if (File AND Mime)
	{
		BFile f(File, B_READ_ONLY);
		BNodeInfo i(&f);
		Status = i.GetType(Mime) == B_OK;		
	}
	return Status;
}

bool LgiGetsAppForMimeType(char *Mime, GArray<GAppInfo*> &Apps, int Limit)
{
	bool Status = false;
	if (Mime)
	{
		BMimeType m(Mime);
		char DefApp[B_MIME_TYPE_LENGTH];
		if (m.GetPreferredApp(DefApp) == B_OK)
		{
			entry_ref ap;
			BMimeType a(DefApp);
			if (a.GetAppHint(&ap) == B_OK)
			{
				BPath p;
				BEntry e(&ap);
				if (e.GetPath(&p) == B_OK)
				{
					GAppInfo *i = new GAppInfo;
					if (i)
					{
						i->Path = NewStr((char*) p.Path());
						Apps[0] = i;
						Status = true;
					}
				}
			}
		}
	}
	return Status;
}

bool LgiGetAppForMimeType(char *Mime, char *AppPath, int BufSize)
{
	GArray<GAppInfo*> Apps;
	if (AppPath AND LgiGetsAppForMimeType(Mime, Apps, 1))
	{
		strsafecpy(AppPath, Apps[0]->Path, BufSize);
		Apps.DeleteObjects();
		return true;
	}
	
	return false;
}

bool LgiGetFileMimeType(char *File, char *Mime, int MimeBufSize)
{
	bool Status = false;
	
	if (File AND Mime)
	{
		BNode e(File);
		BNodeInfo i(&e);
		char Buf[256];
		if (i.GetType(Buf) == B_OK)
		{
			strsafecpy(Mime, Buf, MimeBufSize);
			Status = true;
		}
	}
	
	return Status;
}

int LgiRand(int Limit)
{
	return rand() % Limit;
}

bool LgiPlaySound(char *FileName, int ASync)
{
	bool Status = false;

	if (ASync)
	{
		thread_id Thread = spawn_thread(_lgi_play_sound, "_lgi_play_sound", B_LOW_PRIORITY, FileName);
		if (Thread > 0)
		{
			resume_thread(Thread);
			Status = true;
		}
	}
	else
	{
		Status = _lgi_play_sound(FileName);
	}

	return Status;
}

bool LgiExecute(char *File, char *Args, char *Dir)
{
	if (File)
	{
		char f[256];
		
		if (ValidStr(Dir))
		{
			sprintf(f, "%s/%s", Dir, File);
		}
		else
		{
			strcpy(f, File);
		}
	
		BRoster Roster;
		BEntry Entry(f);
		entry_ref Ref;
		if (Entry.GetRef(&Ref) == B_OK)
		{
			status_t s = B_ERROR;
			
			if (stricmp(f, "/BeOS") == 0 OR
				stricmp(f, "/") == 0 OR
				Entry.IsDirectory())
			{
				char *DirMimeType = "application/x-vnd.Be-directory";
				if (Roster.FindApp(DirMimeType, &Ref) == B_OK)
				{
					char *Arg[1] = {File};
					s = Roster.Launch(&Ref, 1, Arg);
				}
			}
			else
			{
				s = Roster.Launch(&Ref);
			}
			
			return s == B_OK OR s == B_ALREADY_RUNNING;
		}
		else
		{
			if (strnicmp(File, "http://", 7) == 0)
			{
				if (Roster.FindApp("text/html", &Ref) == B_OK)
				{
					/*
					char *Arg[2] = {Ref.name, File};
					return Roster.Launch(&Ref, 2, Arg) == B_OK;
					*/
					char *Arg[1] = {File};
					status_t s = Roster.Launch(&Ref, 1, Arg);
					return s == B_OK OR s == B_ALREADY_RUNNING;
				}
			}
		}
	}
	return false;
}

