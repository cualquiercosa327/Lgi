#include "Lgi.h"
#include "AddFtpFile.h"
#include "resdefs.h"
#include "FtpFile.h"

AddFtpFile::AddFtpFile(GViewI *p, char *ftp)
{
	SetParent(p);
	Base = new GUri(ftp);
	Thread = 0;
	Files = Log = 0;
	if (LoadFromResource(IDD_FTP_FILE))
	{
		MoveToCenter();
		if (GetViewById(IDC_FILES, Files) &&
			GetViewById(IDC_LOG, Log))
		{
			FtpThread *t = GetFtpThread();
			if (t)
			{
				FtpCmd *cmd = new FtpCmd(FtpList, this);
				if (cmd)
				{
					cmd->Watch = Log;
					cmd->Uri = NewStr(ftp);
					t->Post(cmd);
					// Thread = new FtpThread(ftp, Files, Log);
				}
			}
		}
	}
}

AddFtpFile::~AddFtpFile()
{
	Uris.DeleteArrays();
}

void AddFtpFile::OnCmdComplete(FtpCmd *Cmd)
{
	if (Cmd && Base)
	{
		for (auto &e : Cmd->Dir)
		{
			if (e->Name && !e->IsDir())
			{
				GUri fu(Cmd->Uri);
				char path[256];
				if (Base->Path)
					sprintf(path, "%s/%s", Base->Path, e->Name.Get());
				else
					sprintf(path, "/%s", e->Name.Get());
				DeleteArray(fu.Path);
				fu.Path = NewStr(path);
				GAutoString Uri = fu.GetUri();

				Files->Insert(new FtpFile(e, Uri));
			}
		}

		Cmd->Dir.DeleteObjects();
	}
}

int AddFtpFile::OnNotify(GViewI *c, int f)
{
	switch (c->GetId())
	{
		case IDOK:
		{
			List<LListItem> a;
			if (Files)
			{
				Files->GetAll(a);
				for (LListItem *i=a.First(); i; i=a.Next())
				{
					FtpFile *f=dynamic_cast<FtpFile*>(i);
					if (!f) break;
					char *u = f->DetachUri();
					if (u)
						Uris.Add(u);
				}
			}
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
