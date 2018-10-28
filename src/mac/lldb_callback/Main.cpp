#include "Lgi.h"
#include "GTextLog.h"
#include "INet.h"
#include "lldb_callback.h"
#include "LWebSocket.h"
#include "GSubProcess.h"

//////////////////////////////////////////////////////////////////
const char *AppName = "lldb_callback";
const char *Exe = "/Users/matthew/Code/Scribe/trunk/build/Debug/Scribe.app/Contents/MacOS/Scribe";

enum LLDBState
{
	LInit,
	LRunning,
};

class App : public GWindow, public LThread
{
	GTextLog *Log;
	bool Loop;
	GSubProcess LLDB;
	LLDBState State;

public:
    App() : LThread("App"), LLDB("lldb", Exe), State(LInit)

    {
		Log = NULL;
		Loop = true;
        GWindow::Name(AppName);
        GRect r(0, 0, 1000, 800);
        SetPos(r);
        MoveToCenter();
        SetQuitOnClose(true);

        if (Attach(0))
        {
			AddView(Log = new GTextLog(100));
			AttachChildren();
            Visible(true);
        }
		
		Run();
    }
	
	~App()
	{
		Loop = false;
		while (!IsExited())
			LgiSleep(10);
	}
	
	LHashTbl<PtrKey<void*>,int> Watchpoints;

	GString Wait(const char *Key, const char *AltKey = NULL)
	{
		GString r;
		int p;
		char buf[256];
		uint64 Start = LgiCurrentTime();
		do
		{
			if ((p = LLDB.Peek()) > 0)
			{
				int rd = LLDB.Read(buf, MIN(sizeof(buf),p));
				if (rd > 0)
				{
					r += GString(buf, rd);
					Log->Write(buf, rd);
					
					if (r.Find(Key) >= 0)
						break;
					if (AltKey != NULL && r.Find(AltKey) >= 0)
						break;
				
				}
			}
		}
		while (LgiCurrentTime() - Start < LLDB_CMD_TIMEOUT);

		return r;
	}
	
	bool OnMsg(LcbMsg &out, LcbMsg &in)
	{
		switch (in.Type)
		{
			case MsgSetWatch:
			{
				GString s;
				LLDB.Interrupt();
				Wait("stopped");
				s.Printf("w s e -- %p\n", in.vp);
				
				out.u64 = LLDB.Write(s);
				if (!out.u64)
					Log->Print("%s:%i - Error\n", _FL);
				else
				{
					int Index = 0;
					GString r = Wait("addr =", "creation failed");
					auto Lines = r.SplitDelimit("\n");
					for (auto Ln : Lines)
					{
						auto Parts = Ln.Split(":");
						if (Parts.Length() >= 2 &&
							Parts[0].Strip().Equals("Watchpoint created"))
						{
							auto p = Parts[1].Strip().SplitDelimit();
							if (p.Length() == 2 &&
								p[0].Equals("Watchpoint") &&
								p[1].IsNumeric())
							{
								Index = (int)p[1].Int();
							}
						}
					}
					if (Index > 0)
					{
						Watchpoints.Add(in.vp, Index);
						Log->Print("New watchpoint created: %i (%i total).\n", Index, (int)Watchpoints.Length());
					}
					else
					{
						printf("r=%s\n", r.Get());
						Log->Print("Error: can't find watchpoint index in response.\n");
						out.u64 = false;
					}
					
					LLDB.Write("continue\n");
				}

				out.Length = sizeof(out);
				out.Type = MsgStatus;

				return true;
			}
			case MsgClearWatch:
			{
				out.Length = sizeof(out);
				out.Type = MsgStatus;
				out.u64 = false;

				int Index = Watchpoints.Find(in.vp);
				if (Index > 0)
				{
					GString s;
					LLDB.Interrupt();
					Wait("stopped");
					s.Printf("watchpoint delete %i\n", Index);
					
					Log->Print("Deleting watchpoint %i...\n", Index);
					if (LLDB.Write(s))
					{
						GString r = Wait("watchpoints deleted", "No watchpoints exist");
						if (r.Find("watchpoints deleted") >= 0)
						{
							out.u64 = true;
							Watchpoints.Delete(in.vp);
							Log->Print("Deleted watchpoint %i... (%i remain)\n", Index, (int)Watchpoints.Length());
						}
						else
						{
							Log->Print("Error: Failed to delete watchpoint. %s:%i\n", _FL);
						}
					}
					else Log->Print("Error: Failed to write lldb cmd. %s:%i\n", _FL);

					LLDB.Write("continue\n");
				}
				else Log->Print("Error: Failed find watchpoint index. %s:%i\n", _FL);

				return true;
			}
		}
		
		return false;
	}
	
	int Main()
	{
		GSocket Listen;
		GSocket Client;

		bool ListenWarn = false;
		while (!Listen.Listen(LLDB_CB_PORT))
		{
			if (!ListenWarn)
			{
				ListenWarn = true;
				Log->Print("Can't listen on socket..\n");
			}
			LgiSleep(1000);
		}
		if (ListenWarn)
			Log->Print("Listening now...\n");
		
		LcbMsg Msg, Reply;
		char *pMsg = (char*)&Msg;
		int Ch = 0;
		
		bool b = LLDB.Start(true, true, true);
		Log->Print("LLDB started: %i\n", b);
		GString LBuf;
		
		while (Loop)
		{
			if (Listen.IsReadable())
			{
				// Any incoming connection from the client?
				if (Listen.Accept(&Client))
				{
					Log->Print("Client has connected.\n");
				}
			}
			else if (Client.IsOpen() && Client.IsReadable())
			{
				// Process messages from the client process...
				ssize_t Rd = Client.Read(pMsg + Ch, sizeof(Msg) - Ch);
				if (Rd > 0)
				{
					Ch += Rd;
					if (Ch >= 4 && Ch >= Msg.Length)
					{
						if (OnMsg(Reply, Msg))
						{
							auto Wr = Client.Write(&Reply, Reply.Length);
							if (Wr < Reply.Length)
								Log->Print("Error: LLDB.write shorter then reply len.\n");
							else
								Log->Print("Wrote reply to client.\n");
						}
						
						if (Ch > Msg.Length)
						{
							auto len = Msg.Length;
							int b = Ch - len;
							char *p = pMsg + len;
							memmove(pMsg, p, b);
							Ch -= len;
						}
					}
					
					Ch = 0;
				}
				else
				{
					Client.Close();
					Log->Print("Client closed connection.\n");
				}
			}
			else
			{
				// Check in on the lldb subprocess...
				auto Pk = LLDB.Peek();
				if (Pk)
				{
					char Buf[1024];
					auto Rd = LLDB.Read(Buf, MIN(sizeof(Buf), Pk));
					Log->Write(Buf, Rd);
					
					switch (State)
					{
						case LInit:
						{
							LBuf += GString(Buf, Rd);
							if (LBuf.Find("Current executable set to"))
							{
								LBuf.Empty();
								LLDB.Write("run\n");
								State = LRunning;
							}
							break;
						}
						case LRunning:
						{
							break;
						}
					}
				}
			}
		}
		
		return 0;
	}
};

//////////////////////////////////////////////////////////////////
int LgiMain(OsAppArguments &AppArgs)
{
	GApp a(AppArgs, AppName);
	if (a.IsOk())
	{
		a.AppWnd = new App;
		a.Run();
	}

	return 0;
}

