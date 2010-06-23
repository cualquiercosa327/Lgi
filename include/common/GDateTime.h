/// \file
/// \author Matthew Allen
#ifndef __DATE_TIME_H
#define __DATE_TIME_H

#include <time.h>

#define GDTF_DEFAULT				0

/// Format the date as DD/MM/YYYY	
#define GDTF_DAY_MONTH_YEAR			0x001
/// Format the date as MM/DD/YYYY	
#define GDTF_MONTH_DAY_YEAR			0x002
/// Format the date as YYYY/MM/DD
#define GDTF_YEAR_MONTH_DAY			0x004
#define GDTF_DATE_MASK				0x00f

/// Format the time as HH:MM and an am/pm marker
#define GDTF_12HOUR					0x010
/// Format the time as 24 hour time
#define GDTF_24HOUR					0x020
#define GDTF_TIME_MASK				0x0f0

/// A date/time class
class LgiClass GDateTime
{
	/// 1 - DaysInMonth
	int16 _Day;
	/// ####
	int16 _Year;
	/// Milliseconds: 0-999
	int16 _Thousands;

	/// 1-12
	int16 _Month;
	/// 0-59
	int16 _Seconds;
	/// 0-59
	int16 _Minutes;
	/// 0-23 (24hr)
	int16 _Hours;

	/// The current timezone of this object, defaults to the system timezone
	int16 _Tz;			// in minutes (+10 == 600 etc)

	/// Combination of (#GDTF_DAY_MONTH_YEAR or #GDTF_MONTH_DAY_YEAR or #GDTF_YEAR_MONTH_DAY) and (#GDTF_12HOUR or #GDTF_24HOUR)
	uchar _Format;
	/// The default formatting of datetimes
	static uchar DefaultFormat;
	/// The default date separator character
	static char DefaultSeparator;

public:
	GDateTime();
	~GDateTime();

    enum 
    {
        /// Resolution of a second when using 64 bit int's
        /// \sa GDateTime::Get(int64), GDateTime::Set(int64)
        #ifdef WIN32
        Second64Bit = 10000000,
        #else
        Second64Bit = 1000,
        #endif
    };

	/// Returns the day
	int Day() { return _Day; }
	/// Sets the day
	void Day(int d) { _Day = d; }
	/// Returns the month
	int Month() { return _Month; }
	/// Sets the month
	void Month(int m) { _Month = m; }
	/// Sets the month by it's name
	void Month(char *m);
	/// Returns the year
	int Year() { return _Year; }
	/// Sets the year
	void Year(int y) { _Year = y; }

	/// Returns the millisecond part of the time
	int Thousands() { return _Thousands; }
	/// Sets the millisecond part of the time
	void Thousands(int t) { _Thousands = t; }
	/// Returns the seconds part of the time
	int Seconds() { return _Seconds; }
	/// Sets the seconds part of the time
	void Seconds(int s) { _Seconds = s; }
	/// Returns the minutes part of the time
	int Minutes() { return _Minutes; }
	/// Sets the minutes part of the time
	void Minutes(int m) { _Minutes = m; }
	/// Returns the hours part of the time
	int Hours() { return _Hours; }
	/// Sets the hours part of the time
	void Hours(int h) { _Hours = h; }

	/// Returns the timezone of this current date time object in minutes (+10 = 600)
	int GetTimeZone() { return _Tz; }
	/// Sets the timezone of this current object.in minutes (+10 = 600)
	void SetTimeZone
	(
		/// The new timezone
		int Tz,
		/// True if you want to convert the date and time to the new zone,
		/// False if you want to leave the date/time as it is.
		bool ConvertTime
	);
	/// Set this object to UTC timezone, changing the other members as
	/// needed
	void ToUtc() { SetTimeZone(0, true); }
	/// Changes the timezone to the local zone, changing other members
	/// as needed.
	void ToLocal() { SetTimeZone(SystemTimeZone(), true); }

	/// Gets the current formatting of the date, the format only effects
	/// the representation of the date when converted to/from a string.
	/// \returns a bit mask of (#GDTF_DAY_MONTH_YEAR or #GDTF_MONTH_DAY_YEAR or #GDTF_YEAR_MONTH_DAY) and (#GDTF_12HOUR or #GDTF_24HOUR)
	uchar GetFormat() { return _Format; }
	/// Sets the current formatting of the date, the format only effects
	/// the representation of the date when converted to/from a string
	void SetFormat
	(
		/// a bit mask of (#GDTF_DAY_MONTH_YEAR or #GDTF_MONTH_DAY_YEAR or #GDTF_YEAR_MONTH_DAY) and (#GDTF_12HOUR or #GDTF_24HOUR)
		uchar f
	) { _Format = f; }
	/// The default format for the date when formatted as a string
	static uchar GetDefaultFormat();
	/// Sets the default format for the date when formatted as a string
	static void SetDefaultFormat(uchar f) { DefaultFormat = f; }

	/// Returns the day of the week as an index, 0=sun, 1=mon, 2=teus etc
	int DayOfWeek();

	/// Gets the date and time as a string
	/// \sa GDateTime::GetFormat()
	void Get(char *Str);
	/// Gets the data and time as a 64 bit int (os specific)
	bool Get(uint64 &s);
	/// Gets just the date as a string
	/// \sa GDateTime::GetFormat()
	void GetDate(char *Str);
	/// Gets just the time as a string
	/// \sa GDateTime::GetFormat()
	void GetTime(char *Str);

	/// Sets the date and time to the system clock
	void SetNow();
	/// Parses a date time from a string
	/// \sa GDateTime::GetFormat()
	bool Set(char *Str);
	/// Sets the date and time from a 64 bit int (os specific)
	bool Set(uint64 s);
	/// Sets the time from a time_t
	bool Set(time_t tt);
	/// Parses the date from a string
	/// \sa GDateTime::GetFormat()
	bool SetDate(char *Str);
	/// Parses the time from a string
	/// \sa GDateTime::GetFormat()
	bool SetTime(char *Str);

	/// \returns whether a year is a leap year or not
	bool IsLeapYear
	(
		/// Pass a specific year here, or ignore to return if the current Date/Time is in a leap year.
		int Year = -1
	);
	/// \returns true if 'd' is on the same day as this object
	bool IsSameDay(GDateTime &d);
	/// \returns the number of days in the current month
	int DaysInMonth();
	
	/// Adds a number of seconds to the current date/time
	void AddSeconds(int64 Seconds);
	/// Adds a number of minutes to the current date/time
	void AddMinutes(int Minutes);
	/// Adds a number of hours to the current date/time
	void AddHours(int Hours);
	/// Adds a number of days to the current date/time
	void AddDays(int Days);
	/// Adds a number of months to the current date/time
	void AddMonths(int Months);

	/// The system timezone including daylight savings offset in minutes, +10 would return 600
	static int SystemTimeZone(bool ForceUpdate = false);
	/// Any daylight savings offset applied to TimeZone(), in minutes. e.g. to retreive the 
	/// timezone uneffected by DST use TimeZone() - TimeZoneOffset().
	static int SystemTimeZoneOffset();

	/// Daylight savings info record
	struct LgiClass GDstInfo
	{
		/// Timestamp where the DST timezone changes to 'Offset'
		int64 UtcTimeStamp;
		/// The new offset in minutes (e.g. 600 = +10 hours)
		int Offset;

		GDateTime GetLocal();
	};

	/// Retreives daylight savings start and end events for a given period. One event will be emitted
	/// for the current DST/TZ setting at the datetime specified by 'Start', followed by any changes that occur
	/// between that and the 'End' datetime. To retreive just the DST info for start, use NULL for end.
	static bool
	GetDaylightSavingsInfo
	(
		/// [Out] The array to receive DST info. At minimum one record will be returned
		/// matching the TZ in place for the start datetime.
		GArray<GDstInfo> &Out,
		/// [In] The start date that you want DST info for.
		GDateTime &Start,
		/// [Optional In] The end of the period you want DST info for.
		GDateTime *End = 0
	);
	
	// File
	int Sizeof();
	bool Serialize(class GFile &f, bool Write);
	bool Serialize(class GDom *Props, char *Name, bool Write);

	// operators
	bool operator <(GDateTime &dt);
	bool operator <=(GDateTime &dt);
	bool operator >(GDateTime &dt);
	bool operator >=(GDateTime &dt);
	bool operator ==(GDateTime &dt);
	bool operator !=(GDateTime &dt);
	int Compare(GDateTime *d);

	GDateTime operator -(GDateTime &dt);
	GDateTime operator +(GDateTime &dt);
	GDateTime DiffMonths(GDateTime &dt);

	operator uint64()
	{
		uint64 ts = 0;
		Get(ts);
		return ts;
	}

	GDateTime &operator =(uint64 ts)
	{
		Set(ts);
		return *this;
	}
};

/// Time zone information
struct GTimeZone
{
public:
	/// The offset from UTC
	float Offset;
	/// The name of the zone
	char *Text;
};

/// A list of all known timezones.
extern GTimeZone GTimeZones[];

#endif
