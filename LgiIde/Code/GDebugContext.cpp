#include "Lgi.h"
#include "LgiIde.h"
#include "IdeProject.h"
#include "GTextLog.h"
#include "GList.h"

enum DebugMessages
{
	M_RUN_STATE = M_USER + 100,
	M_ON_CRASH,
	M_FILE_LINE,
};

class GDebugContextPriv : public GMutex
{
public:
	GDebugContext *Ctx;
	AppWnd *App;
	IdeProject *Proj;
	bool InDebugging;
	GAutoPtr<GDebugger> Db;
	GAutoString Exe, Args;
	
	GString SeekFile;
	int SeekLine;
	bool SeekCurrentIp;
	
	GString MemDumpAddr;
	NativeInt MemDumpStart;
	GArray<uint8> MemDump;

	GDebugContextPriv(GDebugContext *ctx) : GMutex("GDebugContextPriv")
	{
		Ctx = ctx;
		MemDumpStart = 0;
		App = NULL;
		Proj = NULL;
		InDebugging = false;
		SeekLine = 0;
		SeekCurrentIp = false;
	}
	
	~GDebugContextPriv()
	{
		#if DEBUG_SESSION_LOGGING
		LgiTrace("~GDebugContextPriv freeing debugger...\n");
		#endif
		Db.Reset();
		#if DEBUG_SESSION_LOGGING
		LgiTrace("...done.\n");
		#endif
	}

	void UpdateThreads()
	{
		if (!Db || !Ctx->Threads || !InDebugging)
		{
			LgiTrace("%s:%i - No debugger.\n", _FL);
			return;
		}
		
		GArray<GString> Threads;
		int CurrentThread = -1;
		if (!Db->GetThreads(Threads, &CurrentThread))
		{
			LgiTrace("%s:%i - Failed to get threads from debugger.\n", _FL);
			return;
		}
		
		Ctx->Threads->Empty();
		for (unsigned i=0; i<Threads.Length(); i++)
		{
			char *f = Threads[i];
			if (IsDigit(*f))
			{
				char *Sp = f;
				while (*Sp && IsDigit(*Sp))
					Sp++;
				if (*Sp)
				{
					*Sp++ = 0;
					GListItem *it = new GListItem;
					
					int ThreadId = atoi(f);
					it->SetText(f, 0);
					it->SetText(Sp, 1);
					
					Ctx->Threads->Insert(it);
					it->Select(ThreadId == CurrentThread);
				}
			}			
		}
		
		Ctx->Threads->SendNotify();
	}

	void UpdateCallStack()
	{
		if (Db && Ctx->CallStack && InDebugging)
		{
			GArray<GAutoString> Stack;
			if (Db->GetCallStack(Stack))
			{
				Ctx->CallStack->Empty();
				for (int i=0; i<Stack.Length(); i++)
				{
					GListItem *it = new GListItem;
					char *f = Stack[i];
					if (*f == '#')
					{
						char *Sp = strchr(++f, ' ');
						if (Sp)
						{
							*Sp++ = 0;
							it->SetText(f, 0);
							it->SetText(Sp, 1);
						}
						else
						{
							it->SetText(Stack[i], 1);
						}
					}
					else
					{					
						it->SetText(Stack[i], 1);
					}
					
					Ctx->CallStack->Insert(it);
				}
				
				Ctx->CallStack->SendNotify();
			}
		}
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
	ObjectDump = NULL;
	Registers = NULL;
	Threads = NULL;

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
	switch (MsgCode(m))
	{
		case M_ON_CRASH:
		{
			#if DEBUG_SESSION_LOGGING
			LgiTrace("GDebugContext::OnEvent(M_ON_CRASH)\n");
			#endif
			d->UpdateCallStack();
			break;
		}
		case M_FILE_LINE:
		{
			#if DEBUG_SESSION_LOGGING
			LgiTrace("GDebugContext::OnEvent(M_FILE_LINE)\n");
			#endif
			GString File;
			{
				GMutex::Auto a(d, _FL);
				File = d->SeekFile;
			}
			d->App->GotoReference(File, d->SeekLine, d->SeekCurrentIp);
			break;
		}
	}
	
	return 0;
}

bool GDebugContext::SetFrame(int Frame)
{
	return d->Db ? d->Db->SetFrame(Frame) : false;
}

bool GDebugContext::DumpObject(const char *Var, const char *Val)
{
	if (!d->Db || !Var || !ObjectDump || !d->InDebugging)
		return false;
	
	ObjectDump->Name(NULL);
	
	if (Val && *Val == '{')
	{
		// Local parse the value into the dump
		int Depth = 0;
		const char *Start = NULL;
		char Spaces[256];
		memset(Spaces, ' ', sizeof(Spaces));
		int IndentShift = 2;

		#define Emit() \
			if (Start) \
			{ \
				int bytes = s - Start; \
				const char *last = s-1; while (last > Start && strchr(WhiteSpace, *last)) last--; \
				ObjectDump->Print("%.*s%.*s%s\n", Depth<<IndentShift, Spaces, bytes, Start, *last == '=' ? "" : ";"); \
				Start = NULL; \
			}
			
		for (const char *s = Val; *s; s++)
		{
			if (*s == '{')
			{
				Emit();
				ObjectDump->Print("%.*s%c\n", Depth<<IndentShift, Spaces, *s);
				Depth++;
			}
			else if (*s == '}')
			{
				Emit();
				Depth--;
				ObjectDump->Print("%.*s%c\n", Depth<<IndentShift, Spaces, *s);
			}
			else if (*s == ',')
			{
				Emit();
			}
			else if (!strchr(WhiteSpace, *s))
			{
				if (Start == NULL)
					Start = s;
			}
		}
		
		return true;
	}
	
	return d->Db->PrintObject(Var, ObjectDump);
}

bool GDebugContext::UpdateRegisters()
{
	if (!d->Db || !Registers || !d->InDebugging)
		return false;
	
	return d->Db->GetRegisters(Registers);
}

bool GDebugContext::UpdateLocals()
{
	if (!Locals || !d->Db || !d->InDebugging)
		return false;

	GArray<GDebugger::Variable> Vars;
	if (!d->Db->GetVariables(true, Vars, false))
		return false;
	
	Locals->Empty();
	for (int i=0; i<Vars.Length(); i++)
	{
		GDebugger::Variable &v = Vars[i];
		GListItem *it = new GListItem;
		if (it)
		{
			switch (v.Scope)
			{
				default:
				case GDebugger::Variable::Local:
					it->SetText("local", 0);
					break;
				case GDebugger::Variable::Global:
					it->SetText("global", 0);
					break;
				case GDebugger::Variable::Arg:
					it->SetText("arg", 0);
					break;
			}

			char s[256];
			switch (v.Value.Type)
			{
				case GV_BOOL:
				{
					it->SetText(v.Type ? v.Type.Get() : "bool", 1);
					sprintf_s(s, sizeof(s), "%s", v.Value.Value.Bool ? "true" : "false");
					break;
				}
				case GV_INT32:
				{
					it->SetText(v.Type ? v.Type.Get() : "int32", 1);
					sprintf_s(s, sizeof(s), "%i (0x%x)", v.Value.Value.Int, v.Value.Value.Int);
					break;
				}
				case GV_INT64:
				{
					it->SetText(v.Type ? v.Type.Get() : "int64", 1);
					sprintf_s(s, sizeof(s), LGI_PrintfInt64, v.Value.Value.Int64);
					break;
				}
				case GV_DOUBLE:
				{
					it->SetText(v.Type ? v.Type.Get() : "double", 1);
					sprintf_s(s, sizeof(s), "%f", v.Value.Value.Dbl);
					break;
				}
				case GV_STRING:
				{
					it->SetText(v.Type ? v.Type.Get() : "string", 1);
					sprintf_s(s, sizeof(s), "%s", v.Value.Value.String);
					break;
				}
				case GV_WSTRING:
				{
					it->SetText(v.Type ? v.Type.Get() : "wstring", 1);
					#ifdef MAC
					GAutoString tmp(LgiNewUtf16To8(v.Value.Value.WString));
					sprintf_s(s, sizeof(s), "%s", tmp.Get());
					#else
					sprintf_s(s, sizeof(s), "%S", v.Value.Value.WString);
					#endif
					break;
				}
				case GV_VOID_PTR:
				{
					it->SetText(v.Type ? v.Type.Get() : "void*", 1);
					sprintf_s(s, sizeof(s), "%p", v.Value.Value.Ptr);
					break;
				}
				default:
				{
					sprintf_s(s, sizeof(s), "notimp(%s)", GVariant::TypeToString(v.Value.Type));
					it->SetText(v.Type ? v.Type : s, 1);
					s[0] = 0;
					break;
				}
			}

			it->SetText(v.Name, 2);
			it->SetText(s, 3);
			Locals->Insert(it);
		}
	}
	
	Locals->ResizeColumnsToContent();
	
	return true;
}

void GDebugContext::UpdateCallStack()
{
	d->UpdateCallStack();
}

void GDebugContext::UpdateThreads()
{
	d->UpdateThreads();
}

bool GDebugContext::SelectThread(int ThreadId)
{
	if (!d->Db)
	{
		LgiTrace("%s:%i - No debugger.\n", _FL);
		return false;
	}
	
	return d->Db->SetCurrentThread(ThreadId);
}

bool GDebugContext::ParseFrameReference(const char *Frame, GAutoString &File, int &Line)
{
	if (!Frame)
		return false;
	
	const char *At = NULL, *s = Frame;
	while ((s = stristr(s, "at")))
	{
		At = s;
		s += 2;
	}

	if (!At)
		return false;

	At += 3;
	if (!File.Reset(LgiTokStr(At)))
		return false;
	
	char *Colon = strchr(File, ':');
	if (!Colon)
		return false;

	*Colon++ = 0;
	Line = atoi(Colon);
	return Line > 0;
}

bool GDebugContext::OnCommand(int Cmd)
{
	#if DEBUG_SESSION_LOGGING
	LgiTrace("GDebugContext::OnCommand(%i)\n", Cmd);
	#endif
	
	switch (Cmd)
	{
		case IDM_START_DEBUG:
		{
			if (d->Db)
			{
				d->App->LoadBreakPoints(d->Db);
				d->Db->SetRunning(true);
			}
			break;
		}
		case IDM_CONTINUE:
		{
			if (d->Db)
				d->Db->SetRunning(true);
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
				d->Db->SetRunning(false);
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
		default:
			return false;
	}

	return true;
}

void GDebugContext::OnUserCommand(const char *Cmd)
{
	if (d->Db)
		d->Db->UserCommand(Cmd);
}

void GDebugContext::FormatMemoryDump(int WordSize, int Width, bool InHex)
{
	if (!MemoryDump)
	{
		LgiTrace("%s:%i - No MemoryDump.\n", _FL);
		return;
	}
	
	if (d->MemDump.Length() == 0)
	{
		MemoryDump->Name("No data.");
		return;
	}

	if (!Width)
		Width = 16 / WordSize;			
	if (!WordSize)
		WordSize = 1;

	// Format output to the mem dump window
	GStringPipe p;
	int LineBytes = WordSize * Width;
	
	GPointer ptr;
	ptr.u8 = &d->MemDump[0];
	
	for (NativeInt i = 0; i < d->MemDump.Length(); i += LineBytes)
	{
		GPointer Start = ptr;
		int DisplayBytes = min(d->MemDump.Length() - i, LineBytes);
		int DisplayWords = DisplayBytes / WordSize;			
		NativeInt iAddr = d->MemDumpStart + i;
		p.Print("%p  ", iAddr);
		for (int n=0; n<DisplayWords; n++)
		{
			switch (WordSize)
			{
				default:
					if (InHex)
						p.Print("%02X ", *ptr.u8++);
					else
						p.Print("%4u ", *ptr.u8++);
					break;
				case 2:
					if (InHex)
						p.Print("%04X ", *ptr.u16++);
					else
						p.Print("%7u ", *ptr.u16++);
					break;
				case 4:
					if (InHex)
						p.Print("%08X ", *ptr.u32++);
					else
						p.Print("%10u ", *ptr.u32++);
					break;
				case 8:
					if (InHex)
						#ifdef WIN32
						p.Print("%016I64X ", *ptr.u64++);
						#else
						p.Print("%016LX ", *ptr.u64++);
						#endif
					else
						#ifdef WIN32
						p.Print("%17I64u ", *ptr.u64++);
						#else
						p.Print("%17Lu ", *ptr.u64++);
						#endif
					break;
			}
		}
		
		p.Print("  ", iAddr);
		// p.Print("%.*s", DisplayBytes, Start.s8);
		p.Print("\n");
	}
	
	GAutoString a(p.NewStr());
	MemoryDump->Name(a);
}

void GDebugContext::OnMemoryDump(const char *Addr, int WordSize, int Width, bool IsHex)
{
	if (MemoryDump && d->Db)
	{
		MemoryDump->Name(NULL);
		GString ErrMsg;		
		if (d->Db->ReadMemory(d->MemDumpAddr = Addr, 1024, d->MemDump, &ErrMsg))
		{
			d->MemDumpStart = d->MemDumpAddr.Int(16);
			FormatMemoryDump(WordSize, Width, IsHex);
		}
		else
		{
			MemoryDump->Name(ErrMsg ? ErrMsg : "ReadMemory failed.");
		}
	}
}

void GDebugContext::OnState(bool Debugging, bool Running)
{
	#if DEBUG_SESSION_LOGGING
	LgiTrace("GDebugContext::OnState(%i, %i)\n", Debugging, Running);
	#endif
	
	if (d->InDebugging != Debugging && d->Db)
	{
		d->InDebugging = Debugging;
	}

	if (d->App)
	{
		d->App->OnDebugState(Debugging, Running);

	}
	
	#if DEBUG_SESSION_LOGGING
	LgiTrace("GDebugContext::OnState(%i, %i) ###ENDED###\n", Debugging, Running);
	#endif
	
	// This object may be deleted at this point... don't access anything.
}

void GDebugContext::OnFileLine(const char *File, int Line, bool CurrentIp)
{
	if (!d->App)
		return;
	
	if (!File && !Line)
	{
		LgiTrace("%s:%i - Error: No File or Line... one or both must be valid.\n", _FL);
		return;
	}

	if (d->App->InThread())
	{
		d->App->GotoReference(File, Line, CurrentIp);
	}
	else
	{
		{
			GMutex::Auto a(d, _FL);
			if (File)
				d->SeekFile = File;
			d->SeekLine = Line;
			d->SeekCurrentIp = CurrentIp;
		}

		d->App->PostEvent(M_FILE_LINE);
	}
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

void GDebugContext::OnCrash(int Code)
{
	d->App->PostEvent(M_ON_CRASH);
}

bool GDebugContext::OnBreakPoint(GDebugger::BreakPoint &b, bool Add)
{
	if (!d->Db)
	{
		LgiTrace("%s:%i - No debugger loaded.\n", _FL);
		return false;
	}
	
	if (Add)
		return d->Db->SetBreakPoint(&b);
	else
		return d->Db->RemoveBreakPoint(&b);
}
