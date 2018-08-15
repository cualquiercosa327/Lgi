#include "Lgi.h"
#include "Lvc.h"
#include "GTextLog.h"

#define OPT_WND_STATE		"BlameUiState"
struct BlameUiPriv
{
	GString Output;
	AppPriv *Priv;
	GTextLog *Log;
};

BlameUi::BlameUi(AppPriv *priv, VersionCtrl Vc, GString Output)
{
	d = new BlameUiPriv;
	d->Log = NULL;
	d->Output = Output;
	d->Priv = priv;
	Name("Lvc Blame");

	if (!SerializeState(&d->Priv->Opts, OPT_WND_STATE, true))
	{
		GRect r(0, 0, 800, 500);
		SetPos(r);
		MoveToCenter();
	}

	if (Attach(0))
	{
		AddView(d->Log = new GTextLog(100));
		AttachChildren();
		Visible(true);

		d->Log->Name(Output);
	}
}

BlameUi::~BlameUi()
{
	SerializeState(&d->Priv->Opts, OPT_WND_STATE, false);
	DeleteObj(d);
}