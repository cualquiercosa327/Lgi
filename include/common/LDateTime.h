/// \file
/// \author Matthew Allen

/**
 * \defgroup Time Time and date handling
 * \ingroup Lgi
 */

#ifndef __DATE_TIME_H
#define __DATE_TIME_H

#include <time.h>
#include "GStringClass.h"

#define GDTF_DEFAULT				0

/// Format the date as DD/MM/YYYY
/// \ingroup Time
#define GDTF_DAY_MONTH_YEAR			0x001

/// Format the date as MM/DD/YYYY	
/// \ingroup Time
#define GDTF_MONTH_DAY_YEAR			0x002

/// Format the date as YYYY/MM/DD
/// \ingroup Time
#define GDTF_YEAR_MONTH_DAY			0x004

/// The bit mask for the date
/// \ingroup Time
#define GDTF_DATE_MASK				0x00f

/// Format the time as HH:MM and an am/pm marker
/// \ingroup Time
#define GDTF_12HOUR					0x010

/// Format the time as 24 hour time
/// \ingroup Time
#define GDTF_24HOUR					0x020

/// The bit mask for the time
/// \ingroup Time
#define GDTF_TIME_MASK				0x0f0

/// Format the date with a leading zero
/// \ingroup Time
#define GDTF_DAY_LEADINGZ			0x100

/// Format the month with a leading zero
/// \ingroup Time
#define GDTF_MONTH_LEADINGZ			0x200

/// A date/time class
///
/// This class interacts with system times represented as 64bit ints. The various OS support different
/// formats for that 64bit int values. On windows the system times are in 100-nanosecond intervals since 
/// January 1, 1601 (UTC), as per the FILETIME structure, on Posix systems (Linux/Mac) the 64bit values
/// are in milliseconds since January 1, 1970 UTC. This is just unix time * 1000. If you are serializing
/// these 64bit values you should take that into account, they are NOT cross platform. The GDirectory class
/// uses the same 64bit values as accepted here for the file's last modified timestamp etc. To convert the
/// 64bit values to seconds, divide by LDateTime::Second64Bit, useful for calculating the time in seconds
/// between 2 LDateTime objects.
///
/// \ingroup Time
class LgiClass LDateTime // This class can't have a virtual table, because it's used in 
						 // GArray's which initialize with all zero bytes.
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
	uint16 _Format;
	/// The default formatting of datetimes
	static uint16 DefaultFormat;
	/// The default date separator character
	static char DefaultSeparator;

public:
	LDateTime(const char *Init = NULL);
	~LDateTime();

    enum 
    {
        /// Resolution of a second when using 64 bit int's
        /// \sa LDateTime::Get(int64), LDateTime::Set(int64)
        #ifdef WIN32
        Second64Bit = 10000000,
        #else
        Second64Bit = 1000,
        #endif
    };

	/// Returns true if all the components are in a valid range
	bool IsValid();
	/// Sets the date to an NULL state
	void Empty();

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
	/// Returns the timezone in hours
	double GetTimeZoneHours() { return (double)_Tz / 60.0; }

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
	void ToUtc(bool AssumeLocal = false)
	{
		if (AssumeLocal) _Tz = SystemTimeZone();
		SetTimeZone(0, true);
	}
	/// Changes the timezone to the local zone, changing other members
	/// as needed.
	void ToLocal(bool AssumeUtc = false)
	{
		if (AssumeUtc) _Tz = 0;
		SetTimeZone(SystemTimeZone(), true);
	}

	/// Gets the current formatting of the date, the format only effects
	/// the representation of the date when converted to/from a string.
	/// \returns a bit mask of (#GDTF_DAY_MONTH_YEAR or #GDTF_MONTH_DAY_YEAR or #GDTF_YEAR_MONTH_DAY) and (#GDTF_12HOUR or #GDTF_24HOUR)
	uint16 GetFormat() { return _Format; }
	/// Sets the current formatting of the date, the format only effects
	/// the representation of the date when converted to/from a string
	void SetFormat
	(
		/// a bit mask of (#GDTF_DAY_MONTH_YEAR or #GDTF_MONTH_DAY_YEAR or #GDTF_YEAR_MONTH_DAY) and (#GDTF_12HOUR or #GDTF_24HOUR)
		uint16 f
	) { _Format = f; }

	/// \returns zero based index of weekday, or -1 if not found.
	static int IsWeekDay(const char *s);
	/// \returns zero based index of month, or -1 if not found.
	static int IsMonth(const char *s);
	/// The default format for the date when formatted as a string
	static uint16 GetDefaultFormat();
	/// Sets the default format for the date when formatted as a string
	static void SetDefaultFormat(uint16 f) { DefaultFormat = f; }

	/// Gets the data and time as a GString
	GString Get();
	/// Gets the date and time as a string
	/// \sa LDateTime::GetFormat()
	void Get(char *Str, size_t SLen);
	/// Gets the data and time as a 64 bit int (os specific)
	bool Get(uint64 &s);
	/// Gets just the date as a string
	/// \sa LDateTime::GetFormat()
	/// \returns The number of characters written to 'Str'
	int GetDate(char *Str, size_t SLen);
	/// Gets just the date as a GString
	/// \sa LDateTime::GetFormat()
	GString GetDate();
	/// Gets just the time as a string
	/// \sa LDateTime::GetFormat()
	/// \returns The number of characters written to 'Str'
	int GetTime(char *Str, size_t SLen);
	/// Gets just the time as a GString
	/// \sa LDateTime::GetFormat()
	GString GetTime();
	
	/// Returns the 64bit timestamp.
	uint64 Ts();

	/// Sets the date and time to the system clock
	void SetNow();
	/// Parses a date time from a string
	/// \sa LDateTime::GetFormat()
	bool Set(const char *Str);
	/// Sets the date and time from a 64 bit int (os specific)
	bool Set(uint64 s);
	/// Sets the time from a time_t
	bool Set(time_t tt);
	/// Parses the date from a string
	/// \sa LDateTime::GetFormat()
	bool SetDate(const char *Str);
	/// Parses the time from a string
	/// \sa LDateTime::GetFormat()
	bool SetTime(const char *Str);
	/// Parses the date time from a free form string
	bool Parse(GString s);

	/// \returns true if 'd' is on the same day as this object
	bool IsSameDay(LDateTime &d);
	/// \returns true if 'd' is on the same month as this object
	bool IsSameMonth(LDateTime &d);
	/// \returns true if 'd' is on the same year as this object
	bool IsSameYear(LDateTime &d);

	/// \returns whether a year is a leap year or not
	bool IsLeapYear
	(
		/// Pass a specific year here, or ignore to return if the current Date/Time is in a leap year.
		int Year = -1
	);

	/// Returns the day of the week as an index, 0=sun, 1=mon, 2=teus etc
	int DayOfWeek();
	/// \returns the number of days in the current month
	int DaysInMonth();
	
	/// Adds a number of seconds to the current date/time
	void AddSeconds(int64 Seconds);
	/// Adds a number of minutes to the current date/time
	void AddMinutes(int64 Minutes);
	/// Adds a number of hours to the current date/time
	void AddHours(int64 Hours);
	/// Adds a number of days to the current date/time
	bool AddDays(int64 Days);
	/// Adds a number of months to the current date/time
	void AddMonths(int64 Months);

	/// The system timezone including daylight savings offset in minutes, +10 would return 600
	static int SystemTimeZone(bool ForceUpdate = false);
	/// Any daylight savings offset applied to TimeZone(), in minutes. e.g. to retreive the 
	/// timezone uneffected by DST use TimeZone() - TimeZoneOffset().
	static int SystemTimeZoneOffset();

	/// Daylight savings info record
	struct LgiClass GDstInfo
	{
		/// Timestamp where the DST timezone changes to 'Offset'
		uint64 UtcTimeStamp;
		/// The new offset in minutes (e.g. 600 = +10 hours)
		int Offset;

		LDateTime GetLocal();
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
		LDateTime &Start,
		/// [Optional In] The end of the period you want DST info for.
		LDateTime *End = 0
	);


	/// Decodes an email date into the current instance
	bool Decode(const char *In);

	/// Returns a month index from a month name
	static int MonthFromName(const char *Name);
	
	// File
	int Sizeof();
	bool Serialize(class GFile &f, bool Write);
	bool Serialize(class GDom *Props, char *Name, bool Write);

	// operators
	bool operator <(LDateTime &dt) const;
	bool operator <=(LDateTime &dt) const;
	bool operator >(LDateTime &dt) const;
	bool operator >=(LDateTime &dt) const;
	bool operator ==(const LDateTime &dt) const;
	bool operator !=(LDateTime &dt) const;
	int Compare(const LDateTime *d) const;

	LDateTime operator -(LDateTime &dt);
	LDateTime operator +(LDateTime &dt);
	int DiffMonths(LDateTime &dt);

	operator uint64()
	{
		uint64 ts = 0;
		Get(ts);
		return ts;
	}

	LDateTime &operator =(uint64 ts)
	{
		Set(ts);
		return *this;
	}

	LDateTime &operator =(struct tm *t);

	/// GDom interface.
	///
	/// Even though we don't inherit from a GDom class this class supports the same
	/// interface for ease of use. Currently there are cases where LDateTime is used
	/// in GArray's which don't implement calling a constructor (they init with all
	/// zeros).
	bool GetVariant(const char *Name, class GVariant &Value, char *Array = NULL);
	bool SetVariant(const char *Name, class GVariant &Value, char *Array = NULL);
	bool CallMethod(const char *Name, class GVariant *ReturnValue, GArray<class GVariant*> &Args);
};

/// Time zone information
struct GTimeZone
{
public:
	/// The offset from UTC
	float Offset;
	/// The name of the zone
	const char *Text;
};

/// A list of all known timezones.
extern GTimeZone GTimeZones[];

#ifdef _DEBUG
LgiFunc bool LDateTime_Test();
#endif

#endif
