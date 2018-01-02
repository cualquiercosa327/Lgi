#include "Lvc.h"

VcCommit::VcCommit(AppPriv *priv)
{
	d = priv;
	Current = false;
}

char *VcCommit::GetRev()
{
	return Rev;
}

void VcCommit::SetCurrent(bool b)
{
	Current = b;
}

char *VcCommit::GetText(int Col)
{
	switch (Col)
	{
		case 0:
			return Current ? "***" : NULL;
		case 1:
			return Rev;
		case 2:
			return Author;
		case 3:
			TsCache = Ts.Get();
			return TsCache;
		case 4:
			return Msg;
	}

	return NULL;
}

bool VcCommit::GitParse(GString s)
{
	GString::Array lines = s.Split("\n");
	if (lines.Length() <= 3)
		return false;

	for (unsigned ln = 0; ln < lines.Length(); ln++)
	{
		GString &l = lines[ln];
		if (ln == 0)
			Rev = l.Strip();
		else if (l.Find("Author:") >= 0)
			Author = l.Split(":", 1)[1].Strip();
		else if (l.Find("Date:") >= 0)
			Ts.Parse(l.Split(":", 1)[1].Strip());
		else if (l.Strip().Length() > 0)
		{
			if (Msg)
				Msg += "\n";
			Msg += l.Strip();
		}
	}

	return Author && Msg && Rev;
}

bool VcCommit::SvnParse(GString s)
{
	GString::Array lines = s.Split("\n");
	if (lines.Length() < 3)
		return false;

	for (unsigned ln = 0; ln < lines.Length(); ln++)
	{
		GString &l = lines[ln];
		if (ln == 0)
		{
			GString::Array a = l.Split("|");
			if (a.Length() > 3)
			{
				Rev = a[0].Strip(" \tr");
				Author = a[1].Strip();
				Ts.Parse(a[2]);
			}
		}
		else
		{
			if (Msg)
				Msg += "\n";
			Msg += l.Strip();
		}
	}

	return Author && Msg && Rev;
}

