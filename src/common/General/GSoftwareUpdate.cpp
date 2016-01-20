#include "Lgi.h"
#include "GSoftwareUpdate.h"
#include "INet.h"
#include "IHttp.h"
#include "GProgressDlg.h"
#include "GTextLabel.h"
#include "GButton.h"

static char sHttpDownloadFailed[] = "HTTP download failed.";
static char sSocketConnectFailed[] = "Socket connect failed.";
static char sNoUpdateUri[] = "No update URI.";
static char sXmlParsingFailed[] = "XML parsing failed.";
static char sUnexpectedXml[] = "Unexpected XML.";
static char sUpdateError[] = "Update script error: %s";

struct GSoftwareUpdatePriv
{
	GAutoString Name;
	GAutoString UpdateUri;
	GAutoString Proxy;
	GAutoString Error;
	GAutoString TempPath;

	void SetError(int Id, const char *Def = 0)
	{
		Error.Reset(NewStr(LgiLoadString(Id, Def)));
	}

	class UpdateThread : public GThread
	{
		GSoftwareUpdatePriv *d;
		GSocket *s;
		GSoftwareUpdate::UpdateInfo *Info;
		IHttp Http;
		bool IncBetas;

	public:
		bool Status;

		UpdateThread(GSoftwareUpdatePriv *priv, GSoftwareUpdate::UpdateInfo *info, bool betas) :
		    GThread("SoftwareUpdateThread")
		{
			Info = info;
			d = priv;
			s = 0;
			IncBetas = betas;
			Status = false;

			Run();
		}

		~UpdateThread()
		{
			LgiAssert(IsExited());
		}

		void Cancel()
		{
			Http.Close();
			Info->Cancel = true;
		}

		int Main()
		{
			if (d->UpdateUri)
			{
				GUri Uri(d->UpdateUri);
				char Dir[256];
				sprintf_s(Dir, sizeof(Dir), "%s?name=%s&os=%s&betas=%i", Uri.Path, (char*)d->Name, LgiGetOsName(), IncBetas);
				DeleteArray(Uri.Path);
				Uri.Path = NewStr(Dir);

				GAutoString GetUri = Uri.GetUri();
				
				// LgiTrace("UpdateURI=%s\n", GetUri.Get());
				
				if (d->Proxy)
				{
				    GUri Proxy(d->Proxy);
				    if (Proxy.Host)
					    Http.SetProxy(Proxy.Host, Proxy.Port?Proxy.Port:HTTP_PORT);
				}
				
				GStringPipe RawXml;
				int ProtocolStatus = 0;
				GAutoPtr<GSocketI> s(new GSocket);
				if (Http.Open(s, Uri.Host, Uri.Port))
				{
					IHttp::ContentEncoding Enc;
					if (Http.Get(GetUri, NULL, &ProtocolStatus, &RawXml, &Enc))
					{
						GXmlTree Tree;
						GXmlTag Root;
						if (Tree.Read(&Root, &RawXml))
						{
							GXmlTag *StatusCode;
							if (Root.IsTag("software") &&
								(StatusCode = Root.GetChildTag("status")))
							{
								if (StatusCode->Content &&
									atoi(StatusCode->Content) > 0)
								{
									GXmlTag *t;
									if ((t = Root.GetChildTag("version")))
										Info->Version.Reset(NewStr(t->Content));
									if ((t = Root.GetChildTag("revision")))
										Info->Build.Reset(NewStr(t->Content));
									if ((t = Root.GetChildTag("uri")))
										Info->Uri.Reset(NewStr(t->Content));
									if ((t = Root.GetChildTag("date")))
									{
										Info->Date.SetFormat(GDTF_YEAR_MONTH_DAY);
										Info->Date.Set(t->Content);
									}

									Status = true;
								}
								else
								{
									GXmlTag *Msg = Root.GetChildTag("msg");
									GStringPipe p;
									p.Print(LgiLoadString(L_ERROR_UPDATE, sUpdateError), Msg?Msg->Content:(char*)"Unknown");
									d->Error.Reset(p.NewStr());
								}
							}
							else d->SetError(L_ERROR_UNEXPECTED_XML, sUnexpectedXml);
						}
						else d->SetError(L_ERROR_XML_PARSE, sXmlParsingFailed);
					}
					else d->SetError(L_ERROR_HTTP_FAILED, sHttpDownloadFailed);
				}
				else d->SetError(L_ERROR_CONNECT_FAILED, sSocketConnectFailed);
			}
			else d->SetError(L_ERROR_NO_URI, sNoUpdateUri);

			return 0;
		}
	};

	class Spinner : public GDialog
	{
		UpdateThread *Watch;

	public:
		Spinner(GViewI *par, UpdateThread *watch)
		{
			Watch = watch;
			SetParent(par);
			GRect r(0, 0, 330, 400);
			SetPos(r);
			Name("Software Update");

			GRect c = GetClient();
			GText *t = new GText(-1,
								10,
								10,
								c.X()-20,
								-1,
								LgiLoadString(L_SOFTUP_CHECKING, "Checking for software update..."));
			if (t)
			{
				AddView(t);
				GButton *btn = new GButton(	IDCANCEL,
											(c.X()-70)/2,
											t->GetPos().y2 + 10,
											70,
											-1,
											LgiLoadString(L_BTN_CANCEL, "Cancel"));
				if (btn)
				{
					AddView(btn);
					r.y2 = (r.Y()-c.Y()) + 30 + t->Y() + btn->Y();
					SetPos(r);
					MoveToCenter();
					DoModal();
				}
			}
		}

		void OnCreate()
		{
			SetPulse(100);
		}

		void OnPulse()
		{
			if (Watch->IsExited())
			{
				EndModal(0);
			}
		}

		int OnNotify(GViewI *c, int f)
		{
			switch (c->GetId())
			{
				case IDCANCEL:
				{
					Watch->Cancel();
					break;
				}
			}

			return GDialog::OnNotify(c, f);
		}
	};

	class UpdateDownload : public GThread
	{
		GSoftwareUpdate::UpdateInfo *Info;
		GUri *Uri;
		GUri *Proxy;
		GStream *Local;
		GAutoString *Err;
		int *Status;

	public:
		UpdateDownload( GSoftwareUpdate::UpdateInfo *info,
		                GUri *uri,
		                GUri *proxy,		                
		                GStream *local,
		                GAutoString *err,
		                int *status) : GThread("UpdateDownload")
		{
			Info = info;
			Uri = uri;
			Proxy = proxy;
			Local = local;
			Err = err;
			Status = status;

			Run();
		}

		int Main()
		{
			IHttp Http;
			if (Proxy->Host)
			{
				Http.SetProxy(Proxy->Host, Proxy->Port?Proxy->Port:HTTP_PORT);
			}

			GAutoPtr<GSocketI> s(new GSocket);
			IHttp::ContentEncoding Enc;
			if (!Http.Open(s, Uri->Host, Uri->Port))
			{
				Err->Reset(NewStr(LgiLoadString(L_ERROR_CONNECT_FAILED, sSocketConnectFailed)));
			}
			else if (!Http.Get(Info->Uri, 0, Status, Local, &Enc))
			{
				Err->Reset(NewStr(LgiLoadString(L_ERROR_HTTP_FAILED, sHttpDownloadFailed)));
			}

			return 0;
		}
	};
};

GSoftwareUpdate::GSoftwareUpdate(const char *SoftwareName,
								const char *UpdateUri,
								const char *ProxyUri,
								const char *OptionalTempPath)
{
	d = new GSoftwareUpdatePriv;
	d->Name.Reset(NewStr(SoftwareName));
	d->UpdateUri.Reset(NewStr(UpdateUri));
	d->Proxy.Reset(NewStr(ProxyUri));
	d->TempPath.Reset(NewStr(OptionalTempPath));
}

GSoftwareUpdate::~GSoftwareUpdate()
{
	DeleteObj(d);
}

bool GSoftwareUpdate::CheckForUpdate(UpdateInfo &Info, GViewI *WithUi, bool IncBetas)
{
	GSoftwareUpdatePriv::UpdateThread Update(d, &Info, IncBetas);
	
	if (WithUi)
	{
		GSoftwareUpdatePriv::Spinner s(WithUi, &Update);
	}
	else
	{
		while (!Update.IsExited())
		{
			LgiYield();
			LgiSleep(10);
		}
	}

	return Update.Status;
}

bool GSoftwareUpdate::ApplyUpdate(UpdateInfo &Info, bool DownloadOnly, GViewI *WithUi)
{
	if (!Info.Uri)
	{
		d->SetError(L_ERROR_NO_URI, sNoUpdateUri);
		return false;
	}

	GUri Uri(Info.Uri);
	if (!Uri.Path)
	{
		d->SetError(L_ERROR_URI_ERROR, "No path in URI.");
		return false;
	}

	char *File = strrchr(Uri.Path, '/');
	if (!File) File = Uri.Path;
	else File++;

	char Tmp[MAX_PATH];
	if (d->TempPath)
		LgiMakePath(Tmp, sizeof(Tmp), d->TempPath, File);
	else
	{
		LgiGetTempPath(Tmp, sizeof(Tmp));
		LgiMakePath(Tmp, sizeof(Tmp), Tmp, File);
	}

	GFile Local;
	if (!Local.Open(Tmp, O_WRITE))
	{
		d->SetError(L_ERROR_OPENING_TEMP_FILE, "Can't open local temp file.");
		return false;
	}
	Local.SetSize(0);

	GProgressDlg *Dlg = new GProgressDlg;
	Dlg->SetDescription(LgiLoadString(L_SOFTUP_DOWNLOADING, "Downloading..."));
	Dlg->SetType("KiB");
	Dlg->SetScale(1.0 / 1024.0);

	int HttpStatus = 0;
	int64 Size = 0;
	GUri Proxy(d->Proxy);
	GSoftwareUpdatePriv::UpdateDownload Thread(&Info, &Uri, &Proxy, &Local, &d->Error, &HttpStatus);
	while (!Thread.IsExited())
	{
		LgiYield();
		LgiSleep(50);

		if (!Size)
		{
			Size = Local.GetSize();
			if (Size)
			{
				Dlg->SetLimits(0, Size);
			}
		}
		else
		{
			int64 Cur = Local.GetPos();
			if (Cur > Dlg->Value())
			{
				Dlg->Value(Cur);
			}
		}
	}

	Local.Close();
	Dlg->EndModeless();

	if (HttpStatus != 200)
	{
		FileDev->Delete(Tmp);
		d->SetError(L_ERROR_HTTP_FAILED, sHttpDownloadFailed);
		return false;
	}

	if (!DownloadOnly)
	{
		char *Ext = LgiGetExtension(Tmp);
		if (Ext)
		{
			if (!_stricmp(Ext, "exe"))
			{
				// Execute to install...
				LgiExecute(Tmp);
				return true;
			}
			else
			{
				// Calculate the local path...
				char Path[MAX_PATH];
				LgiGetExePath(Path, sizeof(Path));
				LgiMakePath(Path, sizeof(Path), Path, File);

				if (!_stricmp(Ext, "dll"))
				{
					// Copy to local folder...
					if (!FileDev->Copy(Tmp, Path))
					{
						d->SetError(L_ERROR_COPY_FAILED, "Failed to copy file from temp folder to local folder.");
					}
				}
				else if (!_stricmp(Ext, "gz"))
				{
					// Unpack to local folder...
				}

				// Cleanup
				FileDev->Delete(Tmp);
			}
		}
	}

	return false;
}

char *GSoftwareUpdate::GetErrorMessage()
{
	return d->Error;
}
