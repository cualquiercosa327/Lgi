#include "Lgi.h"
#include "GGrowl.h"
#include "INet.h"

class GGrowlPriv : public GThread, public GSemaphore
{
	bool Loop;
	bool Async;
	GAutoString AppName;
	GAutoPtr<GGrowl::GRegister> Reg;
	GArray<GGrowl::GNotify*> Notes;
	
public:
	GGrowlPriv(bool async) : GThread("GGrowl"), GSemaphore("GGrowlSem")
	{
	    Async = async;
		Loop = true;
		
		if (Async)
		    Run();
	}
	
	~GGrowlPriv()
	{
		Loop = false;
		
		while (Async && !IsExited())
		{
			LgiSleep(10);
		}
	}
	
	bool Register(GAutoPtr<GGrowl::GRegister> p)
	{
		if (!Lock(_FL))
			return false;

		bool Status = true;
		if (Async)
		{
		    Reg = p;
		}
		else
		{
		    GAutoPtr<GGrowl::GNotify> n;
		    Status = Process(p, n);
		}
		
		Unlock();
		return Status;
	}

	bool Notify(GAutoPtr<GGrowl::GNotify> n)
	{
		if (!Lock(_FL))
			return false;

	    bool Status = true;
		if (Async)
		{
		    Notes.Add(n.Release());
		}
		else
		{
		    GAutoPtr<GGrowl::GRegister> r;
		    Status = Process(r, n);
		}
		
		Unlock();
		return Status;
	}
	
	bool Process(GAutoPtr<GGrowl::GRegister> &reg,
			     GAutoPtr<GGrowl::GNotify> &note)
	{
		GAutoPtr<GSocket> Sock(new GSocket);
		Sock->SetTimeout(5000);
		if (!Sock->Open("localhost", 23053))
		    return false;
		    
		GStringPipe p(256);
		p.Print("GNTP/1.0 %s NONE\r\n", reg ? "REGISTER" : "NOTIFY");
		if (reg)
		{
			AppName.Reset(NewStr(reg->App));
			p.Print("Application-Name: %s\r\n", reg->App.Get());
			if (reg->IconUrl)
				p.Print("Application-Icon: %s\r\n", reg->IconUrl.Get());
			p.Print("Notifications-Count: %i\r\n", reg->Types.Length());
			
			for (int i=0; i<reg->Types.Length(); i++)
			{
				GGrowl::GNotifyType &t = reg->Types[i];
				p.Print("\r\n"
						"Notification-Name: %s\r\n"
						"Notification-Enabled: %s\r\n",
						t.Name.Get(),
						t.Enabled ? "True" : "False");
				
				if (t.DisplayName)
					p.Print("Notification-Display-Name: %s\r\n", t.DisplayName.Get());
				if (t.IconUrl)
					p.Print("Notification-Icon: %s\r\n", t.IconUrl.Get());
			}
		}
		else if (AppName)
		{
			p.Print("Application-Name: %s\r\n"
					"Notification-Name: %s\r\n"
					"Notification-Title: %s\r\n"
					"Notification-Sticky: %s\r\n"
					"Notification-Priority: %i\r\n",
					AppName.Get(),
					note->Name.Get(),
					note->Title.Get(),
					note->Sticky ? "True" : "False",
					note->Priority);

			if (note->Id)
				p.Print("Notification-ID: %s\r\n", note->Id.Get());
			if (note->Text)
				p.Print("Notification-Text: %s\r\n", note->Text.Get());
			if (note->IconUrl)
				p.Print("Notification-Icon: %s\r\n", note->IconUrl.Get());
			if (note->CoalescingID)
				p.Print("Notification-Coalescing-ID: %s\r\n", note->CoalescingID.Get());
			if (note->CallbackData)
				p.Print("Notification-Callback-Context: %s\r\n"
						"Notification-Callback-Context-Type: %s\r\n",
						note->CallbackData.Get(),
						note->CallbackDataType.Get());
			if (note->CallbackTarget)
				p.Print("Notification-Callback-Target: %s\r\n", note->CallbackTarget.Get());
		}
		else LgiAssert(0);
		
		p.Print("\r\n");
		
		GAutoString Cmd(p.NewStr());
		int CmdLen = strlen(Cmd);
		int w = Sock->Write(Cmd, CmdLen);
		if (w == CmdLen)
		{
			char In[256];
			while (Sock->IsOpen())
			{
				int r = Sock->Read(In, sizeof(In));
				if (r <= 0)
					break;
				p.Write(In, r);
			}
			GAutoString Resp(p.NewStr());
		}
		
		return true;
	}
	
	int Main()
	{
		char Str[256];
		
		while (Loop)
		{
			GAutoPtr<GGrowl::GRegister> reg;
			GAutoPtr<GGrowl::GNotify> note;

			if (LockWithTimeout(100, _FL))
			{
				if (Reg)
					reg = Reg;
				else if (Notes.Length())
				{
					note.Reset(Notes[0]);
					Notes.DeleteAt(0, true);
				}
				Unlock();
			}

			if (reg || note)
			{
			    Process(reg, note);
			}
			else
			{
				LgiSleep(20);
			}
		}
		
		return 0;
	}
};
	
GGrowl::GGrowl(bool async)
{
	d = new GGrowlPriv(async);
}

GGrowl::~GGrowl()
{
	DeleteObj(d);
}

bool GGrowl::Register(GAutoPtr<GRegister> r)
{
	return d->Register(r);
}

bool GGrowl::Notify(GAutoPtr<GNotify> n)
{
	return d->Notify(n);
}


