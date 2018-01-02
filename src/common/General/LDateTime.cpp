/*
**	FILE:		LDateTime.cpp
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
#ifdef WINDOWS
#include <tchar.h>
#endif

#include "Lgi.h"
#include "LDateTime.h"
#include "GToken.h"
#include "GDocView.h"

//////////////////////////////////////////////////////////////////////////////
uint16 LDateTime::DefaultFormat = GDTF_DEFAULT;
char LDateTime::DefaultSeparator = '/';

uint16 LDateTime::GetDefaultFormat()
{
	if (DefaultFormat == GDTF_DEFAULT)
	{
		#ifdef WIN32

		TCHAR s[80] = _T("1");
		GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_IDATE, s, CountOf(s));
		switch (_tstoi(s))
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
		if (_tstoi(s) == 1)
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
			GString Str = s;
			GToken t(Str, Sep);
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

LDateTime::LDateTime(const char *Init)
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
	if (Init)
		Set(Init);
}

LDateTime::~LDateTime()
{
}

void LDateTime::Empty()
{
	_Year = 0;
	_Month = 0;
	_Day = 0;
	_Minutes = 0;
	_Hours = 0;
	_Seconds = 0;
	_Thousands = 0;
}

#define InRange(v, low, high) ((v) >= low && (v) <= high)
bool LDateTime::IsValid()
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

void LDateTime::SetTimeZone(int NewTz, bool ConvertTime)
{
	if (ConvertTime && NewTz != _Tz)
	{
		// printf("SetTimeZone: %i\n", NewTz - _Tz);
		AddMinutes(NewTz - _Tz);
	}

	_Tz = NewTz;
}

int LDateTime::SystemTimeZone(bool ForceUpdate)
{
	if (ForceUpdate || CurTz == NO_ZONE)
	{
		CurTz = 0;
		CurTzOff = 0;

		#ifdef MAC

			#ifdef COCOA

				LgiAssert(!"Fixme");

			#else
			
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
			
			#endif
		
		#elif defined(WIN32)
		
			timeb tbTime;
			ftime(&tbTime);
			CurTz = -tbTime.timezone;

			TIME_ZONE_INFORMATION Tzi;
			if (GetTimeZoneInformation(&Tzi) == TIME_ZONE_ID_DAYLIGHT)
				CurTzOff = -Tzi.DaylightBias;
		
		#elif defined(LINUX)

			int six_months = (365 * 24 * 60 * 60) / 2;
			time_t now = 0, then = 0;
			time (&now);
			then = now - six_months;
			
			tm now_tz, then_tz;
			tm *t = localtime_r(&now, &now_tz);
			if (t)
			{
				localtime_r(&then, &then_tz);
				
				CurTz = now_tz.tm_gmtoff / 60;
				if (now_tz.tm_isdst)
				{
					CurTzOff = (now_tz.tm_gmtoff - then_tz.tm_gmtoff) / 60;
					CurTz = then_tz.tm_gmtoff / 60;
				}
				else
					CurTzOff = (then_tz.tm_gmtoff - now_tz.tm_gmtoff) / 60;
			}
			else return NO_ZONE;
		
		#else
		
			#error "Impl me."
		
		#endif
	}
	
	return CurTz + CurTzOff;
}

int LDateTime::SystemTimeZoneOffset()
{
	if (CurTz == NO_ZONE)
		SystemTimeZone();

	return CurTzOff;
}

#if defined WIN32
LDateTime ConvertSysTime(SYSTEMTIME &st, int year)
{
	LDateTime n;
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
		LDateTime c = n;
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

int GDateCmp(LDateTime *a, LDateTime *b)
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
	LDateTime Start, End;
	GArray<LDateTime::GDstInfo> Info;

	Start.Set("1/1/2010");
	End.Set("31/12/2014");
	LDateTime::GetDaylightSavingsInfo(Info, Start, &End);

	GStringPipe p;
	for (int i=0; i<Info.Length(); i++)
	{
		LDateTime dt;
		dt = Info[i].UtcTimeStamp;
		char s[64];
		dt.Get(s);
		p.Print("%s, %i\n", s, Info[i].Offset);
	}
	GAutoString s(p.NewStr());
	LgiMsg(0, s, "Test");
*/

LDateTime LDateTime::GDstInfo::GetLocal()
{
	LDateTime d;
	d = UtcTimeStamp;
	d.SetTimeZone(0, false);
	d.SetTimeZone(Offset, true);
	return d;
}

struct MonthHash : public GHashTbl<const char*,int>
{
	MonthHash() : GHashTbl<const char*,int>(0, false)
	{
		Add("Jan", 1);
		Add("Feb", 2);
		Add("Mar", 3);
		Add("Apr", 4);
		Add("May", 5);
		Add("Jun", 6);
		Add("Jul", 7);
		Add("Aug", 8);
		Add("Sep", 9);
		Add("Oct", 10);
		Add("Nov", 11);
		Add("Dec", 12);

		Add("January", 1);
		Add("February", 2);
		Add("March", 3);
		Add("April", 4);
		Add("May", 5);
		Add("June", 6);
		Add("July", 7);
		Add("August", 8);
		Add("September", 9);
		Add("October", 10);
		Add("November", 11);
		Add("December", 12);
	}
};

GString::Array Zdump;

bool LDateTime::GetDaylightSavingsInfo(GArray<GDstInfo> &Info, LDateTime &Start, LDateTime *End)
{
	bool Status = false;
	
	#if defined(WIN32)

	TIME_ZONE_INFORMATION Tzi;
	if (GetTimeZoneInformation(&Tzi) == TIME_ZONE_ID_DAYLIGHT)
	{
		Info.Length(0);

		// Find the DST->Normal date in the same year as Start
		LDateTime n = ConvertSysTime(Tzi.StandardDate, Start.Year());
		// Find the Normal->DST date in the same year as Start
		LDateTime d = ConvertSysTime(Tzi.DaylightDate, Start.Year());

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
			GArray<LDateTime> c;
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
				LDateTime &dt = c[i];
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

	if (!Zdump.Length())
	{	
		FILE *f = popen("zdump -v /etc/localtime", "r");
		if (f)
		{
			char s[256];
			size_t r;
			GStringPipe p(1024);
			while ((r = fread(s, 1, sizeof(s), f)) > 0)
			{
				p.Write(s, (int)r);
			}		
			fclose(f);
			
			GString ps = p.NewGStr();
			Zdump = ps.Split("\n");
		}
	}
		
	MonthHash Lut;
	LDateTime Prev;
	int PrevOff = 0;
	for (int i=0; i<Zdump.Length(); i++)
	{
		char *Line = Zdump[i];
		GToken l(Line, " \t");
		if (l.Length() >= 16 &&
			!stricmp(l[0], "/etc/localtime"))
		{
			// /etc/localtime  Sat Oct  3 15:59:59 2037 UTC = Sun Oct  4 01:59:59 2037 EST isdst=0 gmtoff=36000
			// 0               1   2    3 4        5    6   7 8   9    10 11      12   13  14      15
			LDateTime Utc;
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
							// int IsDst = atoi(Val);
							if (ParseValue(l[15], Var, Val) &&
								!stricmp(Var, "gmtoff"))
							{
								int Off = atoi(Val) / 60;

								if (Prev.Year() &&
									Prev < Start &&
									Start < Utc)
								{
									/*
									char Tmp[64];
									Utc.Get(Tmp, sizeof(Tmp));
									printf("[%i] Utc=%s\n", Info.Length(), Tmp);
									Prev.Get(Tmp, sizeof(Tmp));
									printf("[%i] Prev=%s\n", Info.Length(), Tmp);
									Start.Get(Tmp, sizeof(Tmp));
									printf("[%i] Start=%s\n", Info.Length(), Tmp);
									*/
								
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
			else printf("%s:%i - Tm '%s' has wrong parts: %s\n", _FL, l[4], Line);
		}
	}
	
	#elif defined BEOS
	
	
	
	#else

	LgiAssert(!"Not implemented.");

	#endif

	return Status;
}

int LDateTime::DayOfWeek()
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

void LDateTime::SetNow()
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
		*this = time;
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

GString LDateTime::GetDate()
{
	char s[32];
	int Ch = GetDate(s, sizeof(s));
	return GString(s, Ch);
}

int LDateTime::GetDate(char *Str, size_t SLen)
{
	int Ch = 0;

	if (Str && SLen > 0)
	{
		switch (_Format & GDTF_DATE_MASK)
		{
			case GDTF_MONTH_DAY_YEAR:
				Ch += sprintf_s(Str+Ch, SLen-Ch, _Format&GDTF_MONTH_LEADINGZ?"%2.2i"  :"%i"  , _Month);
				Ch += sprintf_s(Str+Ch, SLen-Ch, _Format&GDTF_DAY_LEADINGZ  ?"%c%2.2i":"%c%i", DefaultSeparator, _Day);
				Ch += sprintf_s(Str+Ch, SLen-Ch, "%c%i", DefaultSeparator, _Year);
				break;
			default:
			case GDTF_DAY_MONTH_YEAR:
				Ch += sprintf_s(Str+Ch, SLen-Ch, _Format&GDTF_DAY_LEADINGZ  ?"%2.2i"  :"%i"  , _Day);
				Ch += sprintf_s(Str+Ch, SLen-Ch, _Format&GDTF_MONTH_LEADINGZ?"%c%2.2i":"%c%i", DefaultSeparator, _Month);
				Ch += sprintf_s(Str+Ch, SLen-Ch, "%c%i", DefaultSeparator, _Year);
				break;
			case GDTF_YEAR_MONTH_DAY:
				Ch += sprintf_s(Str+Ch, SLen-Ch, "%i", _Year);
				Ch += sprintf_s(Str+Ch, SLen-Ch, _Format&GDTF_MONTH_LEADINGZ?"%c%2.2i":"%c%i", DefaultSeparator, _Month);
				Ch += sprintf_s(Str+Ch, SLen-Ch, _Format&GDTF_DAY_LEADINGZ  ?"%c%2.2i":"%c%i", DefaultSeparator, _Day);
				break;
		}
	}
	
	return Ch;
}

GString LDateTime::GetTime()
{
	char s[32];
	int Ch = GetTime(s, sizeof(s));
	return GString(s, Ch);
}

int LDateTime::GetTime(char *Str, size_t SLen)
{
	int Ch = 0;

	if (Str && SLen > 0)
	{
		switch (_Format & GDTF_TIME_MASK)
		{
			case GDTF_12HOUR:
			default:
			{
				Ch += sprintf_s(Str, SLen, "%i:%2.2i:%2.2i%s", Convert24HrTo12Hr(_Hours), _Minutes, _Seconds, Convert24HrToAmPm(_Hours));
				break;
			}
			case GDTF_24HOUR:
			{
				Ch += sprintf_s(Str, SLen, "%i:%2.2i:%2.2i", _Hours, _Minutes, _Seconds);
				break;
			}
		}
	}
	
	return Ch;
}

uint64 LDateTime::Ts()
{
	uint64 ts;
	Get(ts);
	return ts;
}

bool LDateTime::Set(uint64 s)
{
	#if defined WIN32
	FILETIME Utc;
	SYSTEMTIME System;

	// Adjust to the desired timezone
	uint64 u = s + ((int64)_Tz * 60 * Second64Bit);

	Utc.dwHighDateTime = u >> 32;
	Utc.dwLowDateTime = u & 0xffffffff;
	if (FileTimeToSystemTime(&Utc, &System))
	{
		_Year = System.wYear;
		_Month = System.wMonth;
		_Day = System.wDay;

		_Hours = System.wHour;
		_Minutes = System.wMinute;
		_Seconds = System.wSecond;
		_Thousands = System.wMilliseconds;

		return true;
	}
	
	return false;

	#else

	Set((time_t)(s / Second64Bit));
	_Thousands = s % Second64Bit;
	return true;
	
	#endif
}

bool LDateTime::Set(time_t tt)
{
	struct tm *t;
	#if !defined(_MSC_VER) || _MSC_VER < _MSC_VER_VS2005
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
		
		_Tz = SystemTimeZone();

		return true;
	}

	return false;
}

bool LDateTime::Get(uint64 &s)
{
	#ifdef WIN32
	FILETIME Utc;
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
	if (b1 = SystemTimeToFileTime(&System, &Utc))
	{
		// Convert to 64bit
		s = ((uint64)Utc.dwHighDateTime << 32) | Utc.dwLowDateTime;

		// Adjust for timezone
		s -= (int64)_Tz * 60 * Second64Bit;

		return true;
	}

    DWORD Err = GetLastError();
    s = 0;
    LgiAssert(!"SystemTimeToFileTime failed."); 
	return false;
	
	#else
	
	struct tm t;
	ZeroObj(t);
	t.tm_year = _Year - 1900;
	t.tm_mon = _Month - 1;
	t.tm_mday = _Day;

	t.tm_hour = _Hours;
	t.tm_min = _Minutes;
	t.tm_sec = _Seconds;
	t.tm_isdst = -1;
	
	/*
	mktime assumes input is in localtime. This is fine if CurTz == _Tz but if it's different
	we need to adjust the output to give the correct value.
	*/
	time_t sec = mktime(&t);
	if (sec == -1)
		return false;
	
	/*
	int CurTz = SystemTimeZone();
	if (CurTz != _Tz)
	{
		// Adjust the output to the correct time zone..
		int Diff = _Tz - CurTz;
		sec += Diff * 60;
		printf("Adjusting += %i (%i -> %i)\n", Diff * 60, CurTz, _Tz);
	}
	else
		printf("No Adjusting\n");
		*/
	
	s = (uint64)sec * Second64Bit + _Thousands;
	
	return true;
	
	#endif
}

GString LDateTime::Get()
{
	char buf[32];
	int Ch = GetDate(buf, sizeof(buf));
	buf[Ch++] = ' ';
	Ch += GetTime(buf+Ch, sizeof(buf)-Ch);
	return GString(buf, Ch);
}

void LDateTime::Get(char *Str, size_t SLen)
{
	if (Str)
	{
		GetDate(Str, SLen);
		size_t len = strlen(Str);
		if (len < SLen - 1)
		{
			Str[len++] = ' ';
			GetTime(Str+len, SLen-len);
		}
	}
}

bool LDateTime::Set(const char *Str)
{
	bool Status = false;

	if (Str)
	{
		char Local[256];
		strcpy_s(Local, sizeof(Local), Str);
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

void LDateTime::Month(char *m)
{
	int i = IsMonth(m);
	if (i >= 0)
		_Month = i + 1;	
}

bool LDateTime::SetDate(const char *Str)
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
		else
		{
			// Fall back to fuzzy matching
			GToken T(Str, " ,");
			MonthHash Lut;
			int FMonth = 0;
			int FDay = 0;
			int FYear = 0;
			for (unsigned i=0; i<T.Length(); i++)
			{
				char *p = T[i];
				if (IsDigit(*p))
				{
					int i = atoi(p);
					if (i > 0)
					{
						if (i >= 1000)
						{
							FYear = i;
						}
						else if (i < 32)
						{
							FDay = i;
						}
					}
				}
				else
				{
					int i = Lut.Find(p);
					if (i)
						FMonth = i;
				}
			}
			
			if (FMonth && FDay)
			{
				Day(FDay);
				Month(FMonth);
			}
			if (FYear)
			{
				Year(FYear);
			}
			else
			{
				LDateTime Now;
				Now.SetNow();
				Year(Now.Year());
			}
		}
	}

	return Status;
}

bool LDateTime::SetTime(const char *Str)
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
				if (strchr(s, 'p') || strchr(s, 'P'))
				{
					if (_Hours != 12)
					{
						_Hours += 12;
					}
				}
				else if (strchr(s, 'a') || strchr(s, 'A'))
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

int LDateTime::IsWeekDay(const char *s)
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

int LDateTime::IsMonth(const char *s)
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

bool LDateTime::Parse(GString s)
{
	GString::Array a = s.Split(" ");

	Empty();

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
					Hours((int)t[0].Int());
					Minutes((int)t[1].Int());
					Seconds((int)t[2].Int());
				}
			}
			else if (strchr(c, '-'))
			{
				GString::Array t = a[i].Split("-");
				if (t.Length() == 3)
				{
					Year((int)t[0].Int());
					Month((int)t[1].Int());
					Day((int)t[2].Int());
				}
			}
			else if (a[i].Length() == 4)
				Year((int)a[i].Int());
			else if (!Day())
				Day((int)a[i].Int());
		}
		else if (IsAlpha(*c))
		{
			int WkDay = IsWeekDay(c);
			if (WkDay >= 0)
				continue;
				
			int Mnth = IsMonth(c);
			if (Mnth >= 0)
				Month(Mnth + 1);
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
				SetTimeZone(Hrs * 60 + Min, false);
			}
		}
	}

	return IsValid();
}

int LDateTime::Sizeof()
{
	return sizeof(int) * 7;
}

bool LDateTime::Serialize(GFile &f, bool Write)
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
bool LDateTime::Serialize(ObjProperties *Props, char *Name, bool Write)
{
	#ifndef LGI_STATIC
	if (Props && Name)
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

			if (Props->Get(Name, Ptr, Len) &&
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

int LDateTime::Compare(const LDateTime *d)
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

bool LDateTime::operator <(LDateTime &dt)
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

bool LDateTime::operator <=(LDateTime &dt)
{
	return !(*this > dt);
}

bool LDateTime::operator >(LDateTime &dt)
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

bool LDateTime::operator >=(LDateTime &dt)
{
	return !(*this < dt);
}

bool LDateTime::operator ==(LDateTime &dt)
{
	return	_Year == dt._Year &&
			_Month == dt._Month &&
			_Day == dt._Day &&
			_Hours == dt._Hours &&
			_Minutes == dt._Minutes &&
			_Seconds == dt._Seconds &&
			_Thousands == dt._Thousands;
}

bool LDateTime::operator !=(LDateTime &dt)
{
	return	_Year != dt._Year ||
			_Month != dt._Month ||
			_Day != dt._Day ||
			_Hours != dt._Hours ||
			_Minutes != dt._Minutes ||
			_Seconds != dt._Seconds ||
			_Thousands != dt._Thousands;
}

LDateTime LDateTime::DiffMonths(LDateTime &dt)
{
	LDateTime s;

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
			||
			(
				s._Month > dt._Month
				&&
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
			||
			(
				s._Month < dt._Month
				&&
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

LDateTime LDateTime::operator -(LDateTime &dt)
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
    LDateTime r;
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

LDateTime LDateTime::operator +(LDateTime &dt)
{
	LDateTime s = *this;

	s.AddMonths(dt.Month());
	s.AddDays(dt.Day());
	s.AddHours(dt.Hours());
	s.AddMinutes(dt.Minutes());
	// s.AddSeconds(dt.Seconds());

	return s;
}

LDateTime &LDateTime::operator =(struct tm *time)
{
	if (time)
	{
		_Seconds = time->tm_sec;
		_Minutes = time->tm_min;
		_Hours = time->tm_hour;
		_Day = time->tm_mday;
		_Month = time->tm_mon + 1;
		_Year = time->tm_year + 1900;
	}
	else Empty();

	return *this;
}

bool LDateTime::IsSameDay(LDateTime &d)
{
	return	Day() == d.Day() &&
			Month() == d.Month() &&
			Year() == d.Year();
}

bool LDateTime::IsLeapYear(int Year)
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

int LDateTime::DaysInMonth()
{
	if (_Month == 2 &&
		IsLeapYear())
	{
		return 29;
	}

	short DaysInMonth[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
	return _Month >= 1 && _Month <= 12 ? DaysInMonth[_Month-1] : 0;
}

#define MinutesInDay (60*24)

void LDateTime::AddSeconds(int64 Seconds)
{
	uint64 i;
	if (Get(i))
	{
		i += Seconds * Second64Bit;
		Set(i);
	}
}

void LDateTime::AddMinutes(int64 Minutes)
{
	uint64 i;
	if (Get(i))
	{
		int64 delta = Minutes * 60 * Second64Bit;
		uint64 n = i + delta;
		// printf("AddMin " LGI_PrintfInt64 " + " LGI_PrintfInt64 " = " LGI_PrintfInt64 "\n", i, delta, n);
		Set(n);
	}
}

void LDateTime::AddHours(int64 Hours)
{
	uint64 i;
	if (Get(i))
	{
		i += Hours * 3600 * Second64Bit;
		Set(i);
	}
}

bool LDateTime::AddDays(int64 Days)
{
	if (!Days)
		return true;

	uint64 Ts;
	if (!Get(Ts))
		return false;

	uint64 DayTicks = (uint64)LDateTime::Second64Bit * 60 * 60 * 24;
	Ts += Days * DayTicks;
	bool b = Set(Ts);
	return b;
}

void LDateTime::AddMonths(int64 Months)
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
	if (_Day > DaysInMonth())
		_Day = DaysInMonth();
}

int LDateTime::MonthFromName(const char *Name)
{
	if (Name)
	{
		const char *MonthName[] = {
			"Jan", "Feb", "Mar", "Apr", "May", "Jun",
			"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

		for (int m=0; m<12; m++)
		{
			if (strnicmp(Name, MonthName[m], strlen(MonthName[m])) == 0)
			{
				return m + 1;
				break;
			}
		}
	}
	
	return -1;
}

bool LDateTime::Decode(const char *In)
{
	// Test data:
	//
	//		Tue, 6 Dec 2005 1:25:32 -0800
	Empty();

	bool Status = false;
	if (In)
	{
		// Tokenize delimited by whitespace
		GToken T(In);
		if (T.Length() >= 2)
		{
			bool GotDate = false;

			for (int i=0; i<T.Length(); i++)
			{
				char *s = T[i];
				
				if
				(
					*s &&
					(
						strchr(s+1, '.') ||
						strchr(s+1, '-')
					)
				)
				{
					// whole date
					GToken Date(s, ".-");
					if (Date.Length() == 3)
					{
						Day(atoi(Date[0]));
						if (IsDigit(Date[1][0]))
						{
							Month(atoi(Date[1]));
						}
						else
						{
							int m = MonthFromName(Date[1]);
							if (m > 0)
							{
								Month(m);
							}
						}

						int Yr = atoi(Date[2]);
						if (Yr >= 100)
						{
							Year(Yr);
						}
						else
						{
							LDateTime Now;
							Now.SetNow();
							if (Yr + 2000 <= Now.Year())
							{
								Year(2000 + Yr);
							}
							else
							{
								Year(1900 + Yr);
							}
							// else ... ?
						}
						
						GotDate = true;
						Status = true;
					}
				}
				else if (strchr(s, ':'))
				{
					// whole time
					GToken Time(s, ":");
					if (Time.Length() == 2 ||
						Time.Length() == 3)
					{
						// Hour (24hr time)
						Hours(atoi(Time[0]));

						// Minute
						Minutes(atoi(Time[1]));

						if (Time.Length() == 3)
						{
							// Second
							Seconds(atoi(Time[2]));
						}
						
						Status = true;
					}
				}
				else if (IsAlpha(*s))
				{
					// text
					int m = MonthFromName(s);
					if (m > 0)
					{
						Month(m);
					}
				}
				else if (IsDigit(*s))
				{
					int Count = 0;
					for (char *c = s; *c; c++)
					{
						if (!IsDigit(*c))
						{
							break;
						}
						Count++;
					}
					
					if (Count <= 2)
					{
						if (Day())
						{
							// We already have a day... so this might be
							// a 2 digit year...
							LDateTime Now;
							Now.SetNow();
							int Yr = atoi(s);
							if (2000 + Yr <= Now.Year())
							{
								Year(2000 + Yr);
							}
							else
							{
								Year(1900 + Yr);
							}
						}
						else
						{
							// A day number (hopefully)?
							Day(atoi(s));
						}
					}
					else if (Count == 4)
					{
						if (!Year())
						{
							// A year!
							Year(atoi(s));
							Status = true;
						}
						else
						{
							goto DoTimeZone;
						}
						
						// My one and only Y2K fix
						// d.Year((Yr < 100) ? (Yr > 50) ? 1900+Yr : 2000+Yr : Yr);
					}
				}
				else if (strchr("+-", *s))
				{
					// timezone
					DoTimeZone:
					LDateTime Now;
					double OurTmz = (double)Now.SystemTimeZone() / 60;

					if (s &&
						strchr("-+", *s) &&
						strlen(s) == 5)
					{
						#if 1

						int i = atoi(s);
						int hr = i / 100;
						int min = i % 100;
						SetTimeZone(hr * 60 + min, false);

						#else

						// adjust for timezone
						char Buf[32];
						memcpy(Buf, s, 3);
						Buf[3] = 0;

						double TheirTmz = atof(Buf);
						memcpy(Buf+1, s + 3, 2);

						TheirTmz += (atof(Buf) / 60);
						if (Tz)
						{
							*Tz = TheirTmz;
						}

						double AdjustHours = OurTmz - TheirTmz;

						AddMinutes((int) (AdjustHours * 60));

						#endif
					}
					else
					{
						// assume GMT
						AddMinutes((int) (OurTmz * 60));
					}
				}
			}
		}
	}
	return Status;
}

bool LDateTime::GetVariant(const char *Name, GVariant &Dst, char *Array)
{
	GDomProperty p = LgiStringToDomProp(Name);
	switch (p)
	{
		case DateYear: // Type: Int32
			Dst = Year();
			break;
		case DateMonth: // Type: Int32
			Dst = Month();
			break;
		case DateDay: // Type: Int32
			Dst = Day();
			break;
		case DateHour: // Type: Int32
			Dst = Hours();
			break;
		case DateMin: // Type: Int32
			Dst = Minutes();
			break;
		case DateSec: // Type: Int32
			Dst = Seconds();
			break;
		case DateDate: // Type: String
		{
			char s[32];
			GetDate(s, sizeof(s));
			Dst = s;
			break;
		}
		case DateTime: // Type: String
		{
			char s[32];
			GetTime(s, sizeof(s));
			Dst = s;
			break;
		}
		case DateDateAndTime: // Type: String
		{
			char s[32];
			Get(s, sizeof(s));
			Dst = s;
			break;
		}
		case DateInt64: // Type: Int64
		{
			uint64 i = 0;
			Get(i);
			Dst = (int64)i;
			break;
		}
		default:
		{
			return false;
		}
	}
	return true;
}

bool LDateTime::SetVariant(const char *Name, GVariant &Value, char *Array)
{
	GDomProperty p = LgiStringToDomProp(Name);
	switch (p)
	{
		case DateYear:
			Year(Value.CastInt32());
			break;
		case DateMonth:
			Month(Value.CastInt32());
			break;
		case DateDay:
			Day(Value.CastInt32());
			break;
		case DateHour:
			Hours(Value.CastInt32());
			break;
		case DateMin:
			Minutes(Value.CastInt32());
			break;
		case DateSec:
			Seconds(Value.CastInt32());
			break;
		case DateDate:
			SetDate(Value.Str());
			break;
		case DateTime:
			SetTime(Value.Str());
			break;
		case DateDateAndTime:
			Set(Value.Str());
			break;
		case DateInt64:
			Set((uint64)Value.CastInt64());
			break;
		default:
			return false;
	}
	
	return true;
}

bool LDateTime::CallMethod(const char *Name, GVariant *ReturnValue, GArray<GVariant*> &Args)
{
	switch (LgiStringToDomProp(Name))
	{
		case DateSetNow:
			SetNow();
			if (ReturnValue)
				*ReturnValue = true;
			break;
		case DateSetStr:
			if (Args.Length() < 1)
				return false;

			bool Status;
			if (Args[0]->Type == GV_INT64)
				Status = Set((uint64) Args[0]->Value.Int64);
			else
				Status = Set(Args[0]->Str());
			if (ReturnValue)
				*ReturnValue = Status;
			break;
		case DateGetStr:
		{
			char s[256] = "";
			Get(s, sizeof(s));
			if (ReturnValue)
				*ReturnValue = s;
			break;
		}
		default:
			return false;
	}

	return true;
}

#ifdef _DEBUG
#define DATE_ASSERT(i) \
	if (!(i)) \
	{ \
		LgiAssert(!"LDateTime unit test failed."); \
		return false; \
	}

bool LDateTime_Test()
{
	// Check 64bit get/set
	LDateTime t("1/1/2017 0:0:0");
	uint64 i;
	DATE_ASSERT(t.Get(i));
	LgiTrace("Get='%s'\n", t.Get().Get());
	uint64 i2 = i + (24ULL * 60 * 60 * LDateTime::Second64Bit);
	LDateTime t2;
	t2.Set(i2);
	GString s = t2.Get();
	LgiTrace("Set='%s'\n", s.Get());
	DATE_ASSERT(!stricmp(s, "2/1/2017 12:00:00a") ||
				!stricmp(s, "2/01/2017 12:00:00a"));
	
	t.SetNow();
	LgiTrace("Now.Local=%s Tz=%.2f\n", t.Get().Get(), t.GetTimeZoneHours());
	t2 = t;
	t2.ToUtc();
	LgiTrace("Now.Utc=%s Tz=%.2f\n", t2.Get().Get(), t2.GetTimeZoneHours());
	t2.ToLocal();
	LgiTrace("Now.Local=%s Tz=%.2f\n", t2.Get().Get(), t2.GetTimeZoneHours());
	DATE_ASSERT(t == t2);
	
	return true;
}
#endif
