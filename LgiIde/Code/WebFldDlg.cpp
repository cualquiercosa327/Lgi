#include "Lgi.h"
#include "WebFldDlg.h"
#include "resdefs.h"
#include "INet.h"

//////////////////////////////////////////////////////////////////////////////////
WebFldDlg::WebFldDlg(GViewI *p, char *name, char *ftp, char *www)
{
	Name = 0;
	Www = 0;
	SetParent(p);
	LoadFromResource(IDD_WEB_FOLDER);

	if (ftp)
	{
		GUri u(ftp);
		SetCtrlName(IDC_HOST, u.Host);
		SetCtrlName(IDC_USERNAME, u.User);
		SetCtrlName(IDC_PASSWORD, u.Pass);
		SetCtrlName(IDC_PATH, u.Path);
	}

	SetCtrlName(IDC_NAME, name);
	SetCtrlName(IDC_WWW, www);
	MoveToCenter();
}

WebFldDlg::~WebFldDlg()
{
	DeleteArray(Name);
	DeleteArray(Www);
}

int WebFldDlg::OnNotify(GViewI *v, int f)
{
	switch (v->GetId())
	{
		case IDOK:
		{
			GUri u;
			u.Host = NewStr(GetCtrlName(IDC_HOST));
			u.User = NewStr(GetCtrlName(IDC_USERNAME));
			u.Pass = NewStr(GetCtrlName(IDC_PASSWORD));
			u.Path = NewStr(GetCtrlName(IDC_PATH));
			u.Protocol = NewStr("ftp");
			Ftp = u.GetUri();
			Www = NewStr(GetCtrlName(IDC_WWW));
			Name = NewStr(GetCtrlName(IDC_NAME));
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

