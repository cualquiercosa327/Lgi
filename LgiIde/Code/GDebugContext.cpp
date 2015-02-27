#include "Lgi.h"
#include "LgiIde.h"
#include "IdeProject.h"
#include "GTextLog.h"
#include "GList.h"

enum DebugMessages
{
	M_RUN_STATE = M_USER + 100,
	M_ON_CRASH,
};

class GDebugContextPriv
{
public:
	GDebugContext *Ctx;
	AppWnd *App;
	IdeProject *Proj;
	GAutoPtr<GDebugger> Db;
	GAutoString Exe, Args;
	
	NativeInt MemDumpStart;
	GArray<uint8> MemDump;

	GDebugContextPriv(GDebugContext *ctx)
	{
		Ctx = ctx;
		App = NULL;
		Proj = NULL;
		MemDumpStart = 0;
	}
	
	~GDebugContextPriv()
	{
	}

	void UpdateCallStack()
	{
		if (Db && Ctx->CallStack)
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
				
				Ctx->CallStack->SendNotify(M_CHANGE);
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
			d->UpdateCallStack();
			break;
		}
	}
	
	return 0;
}

bool GDebugContext::SetFrame(int Frame)
{
	return d->Db ? d->Db->SetFrame(Frame) : false;
}

bool GDebugContext::DumpObject(const char *Var)
{
	if (!d->Db || !Var || !ObjectDump)
		return false;
	
	ObjectDump->Name(NULL);
	return d->Db->PrintObject(Var, ObjectDump);
}

bool GDebugContext::UpdateRegisters()
{
	if (!d->Db || !Registers)
		return false;
	
	return d->Db->GetRegisters(Registers);
}

bool GDebugContext::UpdateLocals()
{
	if (!Locals || !d->Db)
		return false;

	GArray<GDebugger::Variable> Vars;
	if (!d->Db->GetVariables(true, Vars, true))
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
	switch (Cmd)
	{
		case IDM_START_DEBUG:
		{
			if (d->Db)
				d->Db->SetRuning(true);
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

void GDebugContext::FormatMemoryDump(int WordSize, int Width, bool InHex)
{
	if (MemoryDump)
	{
		if (d->MemDump.Length() == 0)
			MemoryDump->Name("No data.");
		else
		{
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
	}
}

void GDebugContext::OnMemoryDump(const char *Addr, int WordSize, int Width, bool IsHex)
{
	if (MemoryDump && d->Db)
	{
		MemoryDump->Name(NULL);

		GAutoString sAddr(TrimStr(Addr));
		d->MemDumpStart = htoi64(sAddr);
		
		if (d->Db->ReadMemory(d->MemDumpStart, 1024, d->MemDump))
		{
			FormatMemoryDump(WordSize, Width, IsHex);
		}
	}
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

void GDebugContext::OnCrash(int Code)
{
	d->App->PostEvent(M_ON_CRASH);
}
