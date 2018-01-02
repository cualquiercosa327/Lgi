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

int VcCommit::IsWeekDay(const char *s)
{
	static const char *Short[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
	static const char *Long[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
	for (unsigned n=0; n<CountOf(Short); n++)
	{
		if (!_stricmp(Short[n], s) ||
			!_stricmp(Long[n], s))
			return n;
	}
	return -1;
}

int VcCommit::IsMonth(const char *s)
{
	static const char *Short[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
	static const char *Long[] = {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};
	for (unsigned n=0; n<CountOf(Short); n++)
	{
		if (!_stricmp(Short[n], s) ||
			!_stricmp(Long[n], s))
			return n;
	}
	return -1;
}

LDateTime VcCommit::ParseDate(GString s)
{
	LDateTime d;

	GString::Array a = s.Split(" ");
	for (unsigned i=0; i<a.Length(); i++)
	{
		const char *c = a[i];
		if (IsDigit(*c))
		{
			if (strchr(c, ':'))
			{
				GString::Array t = a[i].Split(":");
				if (t.Length() == 3)
				{
					d.Hours((int)t[0].Int());
					d.Minutes((int)t[1].Int());
					d.Seconds((int)t[2].Int());
				}
			}
			else if (strchr(c, '-'))
			{
				GString::Array t = a[i].Split("-");
				if (t.Length() == 3)
				{
					d.Year((int)t[0].Int());
					d.Month((int)t[1].Int());
					d.Day((int)t[2].Int());
				}
			}
			else if (a[i].Length() == 4)
				d.Year((int)a[i].Int());
			else if (!d.Day())
				d.Day((int)a[i].Int());
		}
		else if (IsAlpha(*c))
		{
			int WkDay = IsWeekDay(c);
			if (WkDay >= 0)
				continue;
				
			int Mnth = IsMonth(c);
			if (Mnth >= 0)
				d.Month(Mnth + 1);
		}
		else if (*c == '-' || *c == '+')
		{
			c++;
			if (strlen(c) == 4)
			{
				// Timezone..
				int64 Tz = a[i].Int();
				int Hrs = (int) (Tz / 100);
				int Min = (int) (Tz % 100);
				d.SetTimeZone(Hrs * 60 + Min, false);
			}
		}
	}

	return d;
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
			Ts = ParseDate(l.Split(":", 1)[1].Strip());
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
				Ts = ParseDate(a[2]);
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

