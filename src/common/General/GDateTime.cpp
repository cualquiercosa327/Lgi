/*
**	FILE:		GDateTime.cpp
**	AUTHOR:		Matthew Allen
**	DATE:		11/11/98
**	DESCRIPTION:	Scribe Date Time Object
**
**	Copyright (C) 1998, Matthew Allen
**		fret@memecode.com
*/

#define _INTEGRAL_MAX_BITS 64
#include <time.h>

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/timeb.h>
#ifdef MAC
#include <sys/time.h>
#endif

#include "Lgi.h"
#include "GDateTime.h"
#include "GToken.h"

//////////////////////////////////////////////////////////////////////////////
uint16 GDateTime::DefaultFormat = GDTF_DEFAULT;
char GDateTime::DefaultSeparator = '/';

uint16 GDateTime::GetDefaultFormat()
{
	if (DefaultFormat == GDTF_DEFAULT)
	{
		#ifdef WIN32

		char s[80] = "1";
		GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_IDATE, s, sizeof(s));
		switch (atoi(s))
		{
			case 0:
				DefaultFormat = GDTF_MONTH_DAY_YEAR;
				break;
			default:
			case 1:
				DefaultFormat = GDTF_DAY_MONTH_YEAR;
				break;
			case 2:
				DefaultFormat = GDTF_YEAR_MONTH_DAY;
				break;
		}

		GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_ITIME, s, sizeof(s));
		if (atoi(s) == 1)
		{
			DefaultFormat |= GDTF_24HOUR;
		}
		else
		{
			DefaultFormat |= GDTF_12HOUR;
		}

		if (GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_SDATE, s, sizeof(s)))
			DefaultSeparator = s[0];

		if (GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_SSHORTDATE, s, sizeof(s)))
		{
			char Sep[] = { DefaultSeparator, '/', '\\', '-', '.', 0 };
			GToken t(s, Sep);
			for (int i=0; i<t.Length(); i++)
			{
				if (!stricmp(t[i], "mm"))
					DefaultFormat |= GDTF_MONTH_LEADINGZ;
				else if (!stricmp(t[i], "dd"))
					DefaultFormat |= GDTF_DAY_LEADINGZ;
			}
		}

		#else

		// get some user setting from the system??
		DefaultFormat = GDTF_DAY_MONTH_YEAR | GDTF_12HOUR;

		#endif
	}

	return DefaultFormat;
}

//////////////////////////////////////////////////////////////////////////////
#define NO_ZONE				-1000
static int CurTz			= NO_ZONE;
static int CurTzOff			= NO_ZONE;

GDateTime::GDateTime()
{
	_Day = 0;
	_Month = 0;
	_Year = 0;
	_Thousands = 0;
	_Seconds = 0;
	_Minutes = 0;
	_Hours = 0;
	
	if (CurTz == NO_ZONE)
		_Tz = SystemTimeZone();
	else
		_Tz = CurTz + CurTzOff;

	_Format = GetDefaultFormat();
}

GDateTime::~GDateTime()
{
}

#define InRange(v, low, high) ((v) >= low && (v) <= high)
bool GDateTime::IsValid()
{
	return	InRange(_Day, 1, 31) &&
			InRange(_Year, 1600, 2100) &&
			InRange(_Thousands, 0, 999) &&
			InRange(_Month, 1, 12) &&
			InRange(_Seconds, 0, 59) &&
			InRange(_Minutes, 0, 59) &&
			InRange(_Hours, 0, 23) &&
			InRange(_Tz, -720, 720);
}

void GDateTime::SetTimeZone(int NewTz, bool ConvertTime)
{
	if (ConvertTime AND NewTz != _Tz)
	{
		AddMinutes(NewTz - _Tz);
	}

	_Tz = NewTz;
}

int GDateTime::SystemTimeZone(bool ForceUpdate)
{
	if (ForceUpdate || CurTz == NO_ZONE)
	{
		CurTz = 0;
		CurTzOff = 0;

		#ifdef MAC

		/*
		struct timeval tp;
		struct timezone tzp;
		if (!gettimeofday(&tp, &tzp))
		{
			// FIXME: check this works in DST as well... e.g. tzp.tz_dsttime;
			CurTz = -tzp.tz_minuteswest;
		}
		else
		{
			// oh great...
			LgiAssert(0);
			LgiTrace("%s:%i - SystemTimeZone() failed badly.\n", __FILE__, __LINE__);
		}
		*/
		
		CFTimeZoneRef tz = CFTimeZoneCopySystem();
		CFAbsoluteTime now = CFAbsoluteTimeGetCurrent();
		Boolean dst = CFTimeZoneIsDaylightSavingTime(tz, now);
		if (dst)
		{
			CFAbsoluteTime next = CFTimeZoneGetNextDaylightSavingTimeTransition(tz, now);
			CurTz = CFTimeZoneGetSecondsFromGMT(tz, next + 100) / 60;
		}
		else
		{
			CurTz = CFTimeZoneGetSecondsFromGMT(tz, now) / 60;
		}

		CurTzOff = CFTimeZoneGetDaylightSavingTimeOffset(tz, now) / 60;
		CFRelease(tz);
		
		#else
		
		timeb tbTime;
		ftime(&tbTime);
		CurTz = -tbTime.timezone;

		#ifdef WIN32
		TIME_ZONE_INFORMATION Tzi;
		if (GetTimeZoneInformation(&Tzi) == TIME_ZONE_ID_DAYLIGHT)
		{
			CurTzOff = -Tzi.DaylightBias;
		}
		#endif
		
		#endif
	}
	
	return CurTz + CurTzOff;
}

int GDateTime::SystemTimeZoneOffset()
{
	if (CurTz == NO_ZONE)
		SystemTimeZone();

	return CurTzOff;
}

#if defined WIN32
GDateTime ConvertSysTime(SYSTEMTIME &st, int year)
{
	GDateTime n;
	if (st.wYear)
	{
		n.Year(st.wYear);
		n.Month(st.wMonth);
		n.Day(st.wDay);
	}
	else
	{
		n.Year(year);
		n.Month(st.wMonth);
		
		// Find the 'nth' matching weekday, starting from the first day in the month
		n.Day(1);
		GDateTime c = n;
		for (int i=0; i<st.wDay && c.Month() == n.Month(); i++)
		{
			while (c.DayOfWeek() != st.wDayOfWeek)
				c.AddDays(1);
			if (c.Month() != n.Month())
				break;
			n = c;
		}
	}

	n.Hours(st.wHour);
	n.Minutes(st.wMinute);
	n.Seconds(st.wSecond);
	n.Thousands(st.wMilliseconds);
	return n;
}

int GDateCmp(GDateTime *a, GDateTime *b)
{
	return a->Compare(b);
}

#elif defined POSIX

static bool ParseValue(char *s, GAutoString &var, GAutoString &val)
{
	if (!s)
		return false;
	char *e = strchr(s, '=');
	if (!e)
		return false;
	
	*e++ = 0;
	var.Reset(NewStr(s));
	val.Reset(NewStr(e));
	*e = '=';
	return var != 0 && val != 0;
}

#endif

/* Testing code...
	GDateTime Start, End;
	GArray<GDateTime::GDstInfo> Info;

	Start.Set("1/1/2010");
	End.Set("31/12/2014");
	GDateTime::GetDaylightSavingsInfo(Info, Start, &End);

	GStringPipe p;
	for (int i=0; i<Info.Length(); i++)
	{
		GDateTime dt;
		dt = Info[i].UtcTimeStamp;
		char s[64];
		dt.Get(s);
		p.Print("%s, %i\n", s, Info[i].Offset);
	}
	GAutoString s(p.NewStr());
	LgiMsg(0, s, "Test");
*/

GDateTime GDateTime::GDstInfo::GetLocal()
{
	GDateTime d;
	d = UtcTimeStamp;
	d.SetTimeZone(0, false);
	d.SetTimeZone(Offset, true);
	return d;
}

bool GDateTime::GetDaylightSavingsInfo(GArray<GDstInfo> &Info, GDateTime &Start, GDateTime *End)
{
	bool Status = false;
	
	#if defined(WIN32)

	TIME_ZONE_INFORMATION Tzi;
	if (GetTimeZoneInformation(&Tzi) == TIME_ZONE_ID_DAYLIGHT)
	{
		Info.Length(0);

		// Find the DST->Normal date in the same year as Start
		GDateTime n = ConvertSysTime(Tzi.StandardDate, Start.Year());
		// Find the Normal->DST date in the same year as Start
		GDateTime d = ConvertSysTime(Tzi.DaylightDate, Start.Year());

		// Create initial Info entry
		Info[0].UtcTimeStamp = Start;
		bool IsDst = (n < d) ^ !(Start < n || Start > d);
		if (IsDst)
			// Start is DST
			Info[0].Offset = -(Tzi.Bias + Tzi.DaylightBias);
		else
			// Start is normal
			Info[0].Offset = -(Tzi.Bias + Tzi.StandardBias);

		if (End)
		{
			// Build list of DST change dates
			GArray<GDateTime> c;
			c.Add(n);
			c.Add(d);
			for (int y = Start.Year() + 1; y <= End->Year(); y++)
			{
				// Calculate the dates for the following years if required
				c.Add(ConvertSysTime(Tzi.StandardDate, y));
				c.Add(ConvertSysTime(Tzi.DaylightDate, y));			
			}
			c.Sort(GDateCmp);

			// Itererate over the list to generate further Info entries
			for (int i=0; i<c.Length(); i++)
			{
				GDateTime &dt = c[i];
				if (dt > Start && dt < *End)
				{
					IsDst = !IsDst;

					GDstInfo &inf = Info.New();
					if (IsDst)
						inf.Offset = -(Tzi.Bias + Tzi.DaylightBias);
					else
						inf.Offset = -(Tzi.Bias + Tzi.StandardBias);
					dt.SetTimeZone(inf.Offset, false);
					dt.SetTimeZone(0, true);
					inf.UtcTimeStamp = dt;
				}
			}
		}

		Status = true;
	}
	
	#elif defined(MAC) || defined(LINUX)
	
	FILE *f = popen("zdump -v /etc/localtime", "r");
	if (f)
	{
		char s[256];
		size_t r;
		GStringPipe p(1024);
		while ((r = fread(s, 1, sizeof(s), f)) > 0)
		{
			p.Write(s, r);
		}		
		fclose(f);
		
		GHashTbl<const char*,int> Lut(0, false);
		Lut.Add("Jan", 1);
		Lut.Add("Feb", 2);
		Lut.Add("Mar", 3);
		Lut.Add("Apr", 4);
		Lut.Add("May", 5);
		Lut.Add("Jun", 6);
		Lut.Add("Jul", 7);
		Lut.Add("Aug", 8);
		Lut.Add("Sep", 9);
		Lut.Add("Oct", 10);
		Lut.Add("Nov", 11);
		Lut.Add("Dec", 12);

		GAutoString ps(p.NewStr());
		GToken t(ps, "\r\n");
		GDateTime Prev;
		int PrevOff = 0;
		for (int i=0; i<t.Length(); i++)
		{
			GToken l(t[i], " \t");
			if (l.Length() >= 16 &&
				!stricmp(l[0], "/etc/localtime"))
			{
				// /etc/localtime  Sat Oct  3 15:59:59 2037 UTC = Sun Oct  4 01:59:59 2037 EST isdst=0 gmtoff=36000
				// 0               1   2    3 4        5    6   7 8   9    10 11      12   13  14      15
				GDateTime Utc;
				Utc.Year(atoi(l[5]));
				GToken Tm(l[4], ":");
				if (Tm.Length() == 3)
				{
					Utc.Hours(atoi(Tm[0]));
					Utc.Minutes(atoi(Tm[1]));
					Utc.Seconds(atoi(Tm[2]));
					if (Utc.Minutes() == 0)
					{
						int m = Lut.Find(l[2]);
						if (m)
						{
							Utc.Day(atoi(l[3]));
							Utc.Month(m);

							GAutoString Var, Val;
							if (ParseValue(l[14], Var, Val) &&
								!stricmp(Var, "isdst"))
							{
								int IsDst = atoi(Val);
								if (ParseValue(l[15], Var, Val) &&
									!stricmp(Var, "gmtoff"))
								{
									int Off = atoi(Val) / 60;

									if (Prev.Year() &&
										Prev < Start &&
										Start < Utc)
									{
										LgiAssert(Info.Length() == 0);

										// Emit initial entry for 'start'
										Info[0].UtcTimeStamp = Start;
										Info[0].Offset = PrevOff;
										Status = true;
									}
									
									if (Utc > Start && End && Utc < *End)
									{
										// Emit furthur entries for DST events between start and end.
										GDstInfo &inf = Info.New();
										inf.UtcTimeStamp = Utc;
										inf.Offset = Off;
									}

									Prev = Utc;
									PrevOff = Off;
								}
								else printf("%s:%i - Unknown value for isdst\n", _FL);
							}
							else printf("%s:%i - Unknown value for isdst\n", _FL);
						}
						else printf("%s:%i - Unknown month '%s'\n", _FL, l[2]);
					}
					// else printf("%s:%i - UTC min wrong %s.\n", _FL, l[4]);
				}
				else printf("%s:%i - Tm has wrong parts.\n", _FL);
			}
		}
	}
	
	#else

	LgiAssert(!"Not implemented.");

	#endif

	return Status;
}

int GDateTime::DayOfWeek()
{
	int Index = 0;
	int Day = IsLeapYear() ? 29 : 28;

	switch (_Year / 100)
	{
		case 19:
		{
			Index = 3;
			break;
		}
		case 20:
		{
			Index = 2;
			break;
		}
	}

	// get year right
	int y = _Year % 100;
	int r = y % 12;
	Index = (Index + (y / 12) + r + (r / 4)) % 7;
	
	// get month right
	if (_Month % 2 == 0)
	{
		// even month
		if (_Month > 2) Day = _Month;
	}
	else
	{
		// odd month
		switch (_Month)
		{
			case 1:
			{
				Day = 31;
				if (IsLeapYear())
				{
					Index = Index > 0 ? Index - 1 : Index + 6;
				}
				break;
			}
			case 11:
			case 3:
			{
				Day = 7;
				break;
			}
			case 5:
			{
				Day = 9;
				break;
			}
			case 7:
			{
				Day = 11;
				break;
			}
			case 9:
			{
				Day = 5;
				break;
			}
		}
	}

	// get day right
	int Diff = Index - (Day - _Day);
	while (Diff < 0) Diff += 7;

	return Diff % 7;
}

bool GDateTime::Set(time_t tt)
{
	struct tm *t;
	#if _MSC_VER < 1400
	t = localtime(&tt);
	if (t)
	#else
	struct tm tmp;
	if (_localtime64_s(t = &tmp, &tt) == 0)
	#endif
	{
		_Year = t->tm_year + 1900;
		_Month = t->tm_mon + 1;
		_Day = t->tm_mday;

		_Hours = t->tm_hour;
		_Minutes = t->tm_min;
		_Seconds = t->tm_sec;
		_Thousands = 0;

		return true;
	}

	return false;
}

bool GDateTime::Set(uint64 s)
{
	bool Status = false;

	#if defined WIN32
	FILETIME Utc, Local;
	SYSTEMTIME System;
	Utc.dwHighDateTime = s >> 32;
	Utc.dwLowDateTime = s & 0xffffffff;
	if (FileTimeToLocalFileTime(&Utc, &Local) AND
		FileTimeToSystemTime(&Local, &System))
	{
		_Year = System.wYear;
		_Month = System.wMonth;
		_Day = System.wDay;

		_Hours = System.wHour;
		_Minutes = System.wMinute;
		_Seconds = System.wSecond;
		_Thousands = System.wMilliseconds;

		Status = true;
	}
	#else

	Set((time_t)(s / 1000));
	_Thousands = s % 1000;
	
	#endif

	return Status;
}

void GDateTime::SetNow()
{
    #ifdef WIN32

    SYSTEMTIME stNow;
    FILETIME ftNow;

    GetSystemTime(&stNow);
    SystemTimeToFileTime(&stNow, &ftNow);
    uint64 i64 = ((uint64)ftNow.dwHighDateTime << 32) | ftNow.dwLowDateTime;
    Set(i64);
    
    #else

	time_t now;
	time(&now);
	struct tm *time = localtime(&now);
	if (time)
	{
    	_Seconds = time->tm_sec;
    	_Minutes = time->tm_min;
    	_Hours = time->tm_hour;
    	_Day = time->tm_mday;
    	_Month = time->tm_mon + 1;
    	_Year = time->tm_year + 1900;
    }
	#ifndef LGI_STATIC
    else
    {
    	LgiTrace("%s:%i - Error: localtime failed, now=%u\n", _FL, now);
    }
	#endif
	
	#endif
}

#define Convert24HrTo12Hr(h)			( (h) == 0 ? 12 : (h) > 12 ? (h) % 12 : (h) )
#define Convert24HrToAmPm(h)		( (h) >= 12 ? "p" : "a" )

void GDateTime::GetDate(char *Str)
{
	if (!Str)
		return;

	int c = 0;
	switch (_Format & GDTF_DATE_MASK)
	{
		case GDTF_MONTH_DAY_YEAR:
			c += sprintf(Str+c, _Format&GDTF_MONTH_LEADINGZ?"%02.2i"  :"%i"  , _Month);
			c += sprintf(Str+c, _Format&GDTF_DAY_LEADINGZ  ?"%c%02.2i":"%c%i", DefaultSeparator, _Day);
			c += sprintf(Str+c, "%c%i", DefaultSeparator, _Year);
			break;
		default:
		case GDTF_DAY_MONTH_YEAR:
			c += sprintf(Str+c, _Format&GDTF_DAY_LEADINGZ  ?"%02.2i"  :"%i"  , _Day);
			c += sprintf(Str+c, _Format&GDTF_MONTH_LEADINGZ?"%c%02.2i":"%c%i", DefaultSeparator, _Month);
			c += sprintf(Str+c, "%c%i", DefaultSeparator, _Year);
			break;
		case GDTF_YEAR_MONTH_DAY:
			c += sprintf(Str+c, "%i", _Year);
			c += sprintf(Str+c, _Format&GDTF_MONTH_LEADINGZ?"%c%02.2i":"%c%i", DefaultSeparator, _Month);
			c += sprintf(Str+c, _Format&GDTF_DAY_LEADINGZ  ?"%c%02.2i":"%c%i", DefaultSeparator, _Day);
			break;
	}
}

void GDateTime::GetTime(char *Str)
{
	if (Str)
	{
		switch (_Format & GDTF_TIME_MASK)
		{
			case GDTF_12HOUR:
			default:
			{
				sprintf(Str, "%i:%02.2i:%02.2i%s", Convert24HrTo12Hr(_Hours), _Minutes, _Seconds, Convert24HrToAmPm(_Hours));
				break;
			}
			case GDTF_24HOUR:
				sprintf(Str, "%i:%02.2i:%02.2i", _Hours, _Minutes, _Seconds);
				break;
		}
	}
}

bool GDateTime::Get(uint64 &s)
{
	bool Status = false;

	#ifdef WIN32
	FILETIME Utc, Local;
	SYSTEMTIME System;

	System.wYear = _Year;
	System.wMonth = _Month;
	System.wDay = _Day;
	System.wHour = _Hours;
	System.wMinute = _Minutes;
	System.wSecond = _Seconds;
	System.wMilliseconds = _Thousands;
	System.wDayOfWeek = DayOfWeek();

	BOOL b1, b2;
	if ((b1 = SystemTimeToFileTime(&System, &Local)) AND
		(b2 = LocalFileTimeToFileTime(&Local, &Utc)))
	{
		s = ((int64)Utc.dwHighDateTime << 32) | Utc.dwLowDateTime;
		Status = true;
	}
	else
	{
	    DWORD Err = GetLastError();
	    s = 0;
	    LgiAssert(!"SystemTimeToFileTime failed."); 
	}
	
	#else
	
	struct tm t;
	ZeroObj(t);
	t.tm_year = _Year - 1900;
	t.tm_mon = _Month - 1;
	t.tm_mday = _Day;

	t.tm_hour = _Hours;
	t.tm_min = _Minutes;
	t.tm_sec = _Seconds;
	
	time_t sec = mktime(&t);
	
	s = (uint64)sec * 1000 + _Thousands;
	
	#endif

	return Status;
}

void GDateTime::Get(char *Str)
{
	if (Str)
	{
		GetDate(Str);
		strcat(Str, " ");
		GetTime(Str+strlen(Str));
	}
}

bool GDateTime::Set(char *Str)
{
	bool Status = false;

	if (Str)
	{
		char Local[256];
		strcpy(Local, Str);
		char *Sep = strchr(Local, ' ');
		if (Sep)
		{
			*Sep++ = 0;
			Status |= SetTime(Sep);
		}

		Status |= SetDate(Local);
	}

	return Status;
}

void GDateTime::Month(char *m)
{
	#define IsMonth(a, i) else if (!stricmp(#a, m)) _Month = i;

	if (!m) return;
	IsMonth(jan, 1)
	IsMonth(feb, 2)
	IsMonth(mar, 3)
	IsMonth(apr, 4)
	IsMonth(may, 5)
	IsMonth(jun, 6)
	IsMonth(jul, 7)
	IsMonth(aug, 8)
	IsMonth(sep, 9)
	IsMonth(oct, 10)
	IsMonth(nov, 11)
	IsMonth(dec, 12)
}

bool GDateTime::SetDate(char *Str)
{
	bool Status = false;

	if (Str)
	{
		GToken T(Str, "/-.,_\\");
		if (T.Length() == 3)
		{
			switch (_Format & GDTF_DATE_MASK)
			{
				case GDTF_MONTH_DAY_YEAR:
				{
					_Month = atoi(T[0]);
					_Day = atoi(T[1]);
					_Year = atoi(T[2]);
					break;
				}
				case GDTF_DAY_MONTH_YEAR:
				{
					_Day = atoi(T[0]);
					_Month = atoi(T[1]);
					_Year = atoi(T[2]);
					break;
				}
				case GDTF_YEAR_MONTH_DAY:
				{
					_Year = atoi(T[0]);
					_Month = atoi(T[1]);
					_Day = atoi(T[2]);
					break;
				}
				default:
				{
					int n[3] =
					{
						atoi(T[0]),
						atoi(T[1]),
						atoi(T[2])
					};

					if (n[0] > 1000)
					{
						// yyyy/m/d
						_Year = n[0];
						_Month = n[1];
						_Day = n[2];
					}
					else if (n[2] > 1000)
					{
						_Year = n[2];
						if (n[0] > 12)
						{
							// d/m/yyyy
							_Day = n[0];
							_Month = n[1];
						}
						else if (n[1] > 12)
						{
							// m/d/yyyy
							_Day = n[1];
							_Month = n[0];
						}
						else if ((DefaultFormat & GDTF_DATE_MASK) == GDTF_MONTH_DAY_YEAR)
						{
							// Assume m/d/yyyy
							_Day = n[1];
							_Month = n[0];
						}
						else
						{
							// Who knows???
							// Assume d/m/yyyy
							_Day = n[0];
							_Month = n[1];
						}
					}
					break;
				}
			}

			if (_Year < 100)
			{
				if (_Year >= 80)
				{
					_Year += 1900;
				}
				else
				{
					_Year += 2000;
				}
			}

			Status = true;
		}
	}

	return Status;
}

bool GDateTime::SetTime(char *Str)
{
	bool Status = false;

	if (Str)
	{
		GToken T(Str, ":.");
		if (T.Length() >= 2 && T.Length() <= 4)
		{
			_Hours = atoi(T[0]);
			_Minutes = atoi(T[1]);
			
			char *s = T[2];
			if (s) _Seconds = atoi(s);
			else _Seconds = 0;
			
			s = T[T.Length()-1];
			if (s)
			{
				if (strchr(s, 'p') OR strchr(s, 'P'))
				{
					if (_Hours != 12)
					{
						_Hours += 12;
					}
				}
				else if (strchr(s, 'a') OR strchr(s, 'A'))
				{
					if (_Hours == 12)
					{
						_Hours -= 12;
					}
				}
			}

			_Thousands = (T.Length() > 3) ? atoi(T[3]) : 0;
			Status = true;
		}
	}

	return Status;
}

int GDateTime::Sizeof()
{
	return sizeof(int) * 7;
}

bool GDateTime::Serialize(GFile &f, bool Write)
{
	int32 i;

	if (Write)
	{
		#define wf(fld) i = fld; f << i;
		wf(_Day);
		wf(_Month);
		wf(_Year);
		wf(_Thousands);
		wf(_Seconds);
		wf(_Minutes);
		wf(_Hours);
	}
	else
	{
		#define rf(fld) f >> i; fld = i;
		rf(_Day);
		rf(_Month);
		rf(_Year);
		rf(_Thousands);
		rf(_Seconds);
		rf(_Minutes);
		rf(_Hours);
	}

	return true;
}

/*
bool GDateTime::Serialize(ObjProperties *Props, char *Name, bool Write)
{
	#ifndef LGI_STATIC
	if (Props AND Name)
	{
		struct _Date
		{
			uint8 Day;
			uint8 Month;
			int16 Year;
			uint8 Hour;
			uint8 Minute;
			uint16 ThouSec;
		};

		LgiAssert(sizeof(_Date) == 8);

		if (Write)
		{
			_Date d;

			d.Day = _Day;
			d.Month = _Month;
			d.Year = _Year;
			d.Hour = _Hours;
			d.Minute = _Minutes;
			d.ThouSec = (_Seconds * 1000) + _Thousands;

			return Props->Set(Name, &d, sizeof(d));
		}
		else // Read
		{
			void *Ptr;
			int Len;

			if (Props->Get(Name, Ptr, Len) AND
				sizeof(_Date) == Len)
			{
				_Date *d = (_Date*) Ptr;

				_Day = d->Day;
				_Month = d->Month;
				_Year = d->Year;
				_Hours = d->Hour;
				_Minutes = d->Minute;
				_Seconds = d->ThouSec / 1000;
				_Thousands = d->ThouSec % 1000;

				return true;
			}
		}
	}
	#endif

	return false;
}
*/

int GDateTime::Compare(GDateTime *d)
{
	int c = 0;
	if (d)
	{
		c = _Year - d->_Year;
		if (!c)
		{
			c = _Month - d->_Month;
			if (!c)
			{
				c = _Day - d->_Day;
				if (!c)
				{
					c = _Hours - d->_Hours;
					if (!c)
					{
						c = _Minutes - d->_Minutes;
						if (!c)
						{
							c = _Seconds - d->_Seconds;
							if (!c)
							{
								c = _Thousands - d->_Thousands;
							}
						}
					}
				}
			}
		}
	}
	return c;
}

bool GDateTime::operator <(GDateTime &dt)
{
	if (_Year < dt._Year) return true;
	else if (_Year > dt._Year) return false;

	if (_Month < dt._Month) return true;
	else if (_Month > dt._Month) return false;

	if (_Day < dt._Day) return true;
	else if (_Day > dt._Day) return false;

	if (_Hours < dt._Hours) return true;
	else if (_Hours > dt._Hours) return false;

	if (_Minutes < dt._Minutes) return true;
	else if (_Minutes > dt._Minutes) return false;

	if (_Seconds < dt._Seconds) return true;
	else if (_Seconds > dt._Seconds) return false;

	if (_Thousands < dt._Thousands) return true;
	else if (_Thousands > dt._Thousands) return false;

	return false;
}

bool GDateTime::operator <=(GDateTime &dt)
{
	return !(*this > dt);
}

bool GDateTime::operator >(GDateTime &dt)
{
	if (_Year > dt._Year) return true;
	else if (_Year < dt._Year) return false;

	if (_Month > dt._Month) return true;
	else if (_Month < dt._Month) return false;

	if (_Day > dt._Day) return true;
	else if (_Day < dt._Day) return false;

	if (_Hours > dt._Hours) return true;
	else if (_Hours < dt._Hours) return false;

	if (_Minutes > dt._Minutes) return true;
	else if (_Minutes < dt._Minutes) return false;

	if (_Seconds > dt._Seconds) return true;
	else if (_Seconds < dt._Seconds) return false;

	if (_Thousands > dt._Thousands) return true;
	else if (_Thousands < dt._Thousands) return false;

	return false;
}

bool GDateTime::operator >=(GDateTime &dt)
{
	return !(*this < dt);
}

bool GDateTime::operator ==(GDateTime &dt)
{
	return	_Year == dt._Year AND
			_Month == dt._Month AND
			_Day == dt._Day AND
			_Hours == dt._Hours AND
			_Minutes == dt._Minutes AND
			_Seconds == dt._Seconds AND
			_Thousands == dt._Thousands;
}

bool GDateTime::operator !=(GDateTime &dt)
{
	return	_Year != dt._Year OR
			_Month != dt._Month OR
			_Day != dt._Day OR
			_Hours != dt._Hours OR
			_Minutes != dt._Minutes OR
			_Seconds != dt._Seconds OR
			_Thousands != dt._Thousands;
}

GDateTime GDateTime::DiffMonths(GDateTime &dt)
{
	GDateTime s;

	int Months = 0;
	s._Year = _Year;
	s._Month = _Month;
	s._Day = _Day;

	while (s._Year != dt._Year)
	{
		if (s._Year > dt._Year)
		{
			s._Year--;
			Months += 12;
		}
		else
		{
			Months -= 12;
			s._Year++;
		}
	}

	while (s._Month != dt._Month)
	{
		if
		(
			s._Month > dt._Month + 1
			OR
			(
				s._Month > dt._Month
				AND
				s._Day >= dt._Day
			)
		)
		{
			s._Month--;
			Months++;
		}
		else if
		(
			s._Month < dt._Month - 1
			OR
			(
				s._Month < dt._Month
				AND
				s._Day <= dt._Day
			)
		)
		{
			Months--;
			s._Month++;
		}
		else break;
	}

	s._Month = Months;
	s._Day = 0;
	s._Year = 0;

	return s;
}

GDateTime GDateTime::operator -(GDateTime &dt)
{
    uint64 a, b;
    Get(a);
    dt.Get(b);

    /// Resolution of a second when using 64 bit timestamps
    int64 Sec = Second64Bit;
    int64 Min = 60 * Sec;
    int64 Hr = 60 * Min;
    int64 Day = 24 * Hr;
    
    uint64 d = a - b;
    GDateTime r;
    r._Day = d / Day;
    d -= r._Day * Day;
    r._Hours = d / Hr;
    d -= r._Hours * Hr;
    r._Minutes = d / Min;
    d -= r._Minutes * Min;
    r._Seconds = d / Sec;
	#ifdef WIN32
    d -= r._Seconds * Sec;
    r._Thousands = d / 10000;
	#else
	r._Thousands = 0;
	#endif

	return r;
}

GDateTime GDateTime::operator +(GDateTime &dt)
{
	GDateTime s = *this;

	s.AddMonths(dt.Month());
	s.AddDays(dt.Day());
	s.AddHours(dt.Hours());
	s.AddMinutes(dt.Minutes());
	// s.AddSeconds(dt.Seconds());

	return s;
}

bool GDateTime::IsSameDay(GDateTime &d)
{
	return	Day() == d.Day() AND
			Month() == d.Month() AND
			Year() == d.Year();
}

bool GDateTime::IsLeapYear(int Year)
{
	if (Year < 0) Year = _Year;

	if (Year % 4 != 0)
	{
		return false;
	}

	if (Year % 400 == 0)
	{
		return true;
	}

	if (Year % 100 == 0)
	{
		return false;
	}

	return true;
}

int GDateTime::DaysInMonth()
{
	if (_Month == 2 AND
		IsLeapYear())
	{
		return 29;
	}

	short DaysInMonth[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
	return _Month >= 1 AND _Month <= 12 ? DaysInMonth[_Month-1] : 0;
}

#define MinutesInDay (60*24)

void GDateTime::AddSeconds(int64 Seconds)
{
    int64 s = (int64)_Seconds + Seconds;
    
    if (s < 0)
    {
        int m = (-s + 59) / 60;
        AddMinutes(-m);
        s += m * 60;
    }
    else if (s >= 60)
    {
        int m = s / 60;
        AddMinutes(m);
        s -= m * 60;
    }
    
    _Seconds = s;
	LgiAssert(_Seconds >= 0 && _Seconds < 60);
}

void GDateTime::AddMinutes(int Minutes)
{
	int m = (int)_Minutes + Minutes;

	if (m < 0)
	{
	    int h = (-m + 59) / 60;
		AddHours(-h);
		m += h * 60;
	}
	else if (m >= 60)
	{
	    int h = m / 60;
		AddHours(h);
		m -= h * 60;
	}

	_Minutes = m;
	LgiAssert(_Minutes >= 0 && _Minutes < 60);
}

void GDateTime::AddHours(int Hours)
{
	int h = _Hours + Hours;

	if (h < 0)
	{
	    int d = (-h + 23) / 24;
		AddDays(-d);
		h += d * 24;
	}
	else if (h >= 24)
	{
	    int d = h / 24;
		AddDays(d);
		h -= d * 24;
	}

	_Hours = h;
	LgiAssert(_Hours >= 0 && _Hours < 24);
}

void GDateTime::AddDays(int Days)
{
	_Day += Days;

	do
	{
		if (_Day < 1)
		{
			AddMonths(-1);
			_Day += DaysInMonth();
		}
		else if (_Day > DaysInMonth())
		{
			_Day -= DaysInMonth();
			AddMonths(1);
		}
		else
		{
			break;
		}
	}
	while (1);
}

void GDateTime::AddMonths(int Months)
{
	int m = _Month + Months;

	do
	{
		if (m < 1)
		{
			_Year--;
			m += 12;
		}
		else if (m > 12)
		{
			_Year++;
			m -= 12;
		}
		else
		{
			break;
		}
	}
	while (1);

	_Month = m;
}


