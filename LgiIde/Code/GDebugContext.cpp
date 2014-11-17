#include "Lgi.h"
#include "LgiIde.h"
#include "IdeProject.h"
#include "GTextLog.h"

enum DebugMessages
{
	M_RUN_STATE = M_USER + 100,
};

class GDebugContextPriv
{
public:
	GDebugContext *Ctx;
	AppWnd *App;
	IdeProject *Proj;
	GAutoPtr<GDebugger> Db;
	GAutoString Exe, Args;

	GDebugContextPriv(GDebugContext *ctx)
	{
		Ctx = ctx;
		App = NULL;
		Proj = NULL;
	}
	
	~GDebugContextPriv()
	{
	}
	
	void Log(const char *Fmt, ...)
	{
		if (Ctx->DebuggerLog)
		{
			va_list Arg;
			va_start(Arg, Fmt);
			GStreamPrintf(Ctx->DebuggerLog, 0, Fmt, Arg);
			va_end(Arg);
		}
	}

	int Main()
	{
		return 0;
	}
};

GDebugContext::GDebugContext(AppWnd *App, IdeProject *Proj, const char *Exe, const char *Args)
{
	Watch = NULL;
	Locals = NULL;
	DebuggerLog = NULL;

	d = new GDebugContextPriv(this);
	d->App = App;
	d->Proj = Proj;
	d->Exe.Reset(NewStr(Exe));
	
	if (d->Db.Reset(CreateGdbDebugger()))
	{
		if (!d->Db->Load(this, Exe, Args, NULL))
		{
			d->Log("Failed to load '%s' into debugger.\n", d->Exe.Get());
			d->Db.Reset();
		}
	}
}

GDebugContext::~GDebugContext()
{
	DeleteObj(d);
}

GMessage::Param GDebugContext::OnEvent(GMessage *m)
{
	switch (m->Msg)
	{
	}
	
	return 0;
}

bool GDebugContext::OnCommand(int Cmd)
{
	switch (Cmd)
	{
		case IDM_START_DEBUG:
		{
			/*
			if (d->Db)
				d->Db->SetRuning(true);
			*/
			break;
		}
		case IDM_ATTACH_TO_PROCESS:
		{
			break;
		}
		case IDM_STOP_DEBUG:
		{
			if (d->Db)
				return d->Db->Unload();
			else
				return false;				
			break;
		}
		case IDM_PAUSE_DEBUG:
		{
			if (d->Db)
				d->Db->SetRuning(false);
			break;
		}
		case IDM_RESTART_DEBUGGING:
		{
			if (d->Db)
				d->Db->Restart();
			break;
		}
		case IDM_RUN_TO:
		{
			break;
		}
		case IDM_STEP_INTO:
		{
			if (d->Db)
				d->Db->StepInto();
			break;
		}
		case IDM_STEP_OVER:
		{
			if (d->Db)
				d->Db->StepOver();
			break;
		}
		case IDM_STEP_OUT:
		{
			if (d->Db)
				d->Db->StepOut();
			break;
		}
		case IDM_TOGGLE_BREAKPOINT:
		{
			break;
		}
	}
	
	return true;
}

void GDebugContext::OnUserCommand(const char *Cmd)
{
	if (d->Db)
		d->Db->UserCommand(Cmd);
}

void GDebugContext::OnChildLoaded(bool Loaded)
{
}

void GDebugContext::OnRunState(bool Running)
{
	if (d->App)
		d->App->OnRunState(Running);
}

void GDebugContext::OnFileLine(const char *File, int Line)
{
	if (File && d->App)
		d->App->GotoReference(File, Line);
}

int GDebugContext::Write(const void *Ptr, int Size, int Flags)
{
	if (DebuggerLog && Ptr)
	{
		// LgiTrace("Write '%.*s'\n", Size, Ptr);
		return DebuggerLog->Write(Ptr, Size, 0);
	}
		
	return -1;
}

void GDebugContext::OnError(int Code, const char *Str)
{
	if (DebuggerLog)
		DebuggerLog->Print("Error(%i): %s\n", Code, Str);
}
