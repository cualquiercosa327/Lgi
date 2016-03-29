#ifndef _STORE3_DEFS_H_
#define _STORE3_DEFS_H_

enum EmailFlags
{
	// Mail flags
	MAIL_SENT = 					0x00000001,
	MAIL_RECEIVED =					0x00000002,
	MAIL_CREATED =					0x00000004,
	MAIL_FORWARDED =				0x00000008,
	MAIL_REPLIED =					0x00000010,
	MAIL_ATTACHMENTS =				0x00000020,
	MAIL_READ = 					0x00000040,
	// #define MAIL_MARK			0x00000080,	// Deprecated
	MAIL_READY_TO_SEND =			0x00000100,	// If this flag is set then the user
												// wants to send the mail on the next
												// run. When the user just saves a new
												// message in the outbox this isn't set
												// and isn't sent until they go in and
												// say "send". At which point this flag
												// is set and the message sent
	MAIL_READ_RECEIPT =				0x00000200,
	MAIL_IGNORE =					0x00000400,
	MAIL_FIXED_WIDTH_FONT =			0x00000800,
	MAIL_BOUNCED =					0x00001000,	// The bounce source mail
	MAIL_BOUNCE =					0x00002000,	// The outgoing copy of a bounced mail
	MAIL_SHOW_IMAGES =				0x00004000,	// User selected to show images in HTML
	MAIL_BAYES_HAM =				0x00010000,	// Bayesian classified originally as ham
	MAIL_BAYES_SPAM =				0x00020000,	// Bayesian classified originally as spam
	MAIL_NEW =	                    0x00040000, // Mail is new, cleared after the OnNew event happens
};

enum EmailAddressType
{
	MAIL_ADDR_TO = 0,
	MAIL_ADDR_CC = 1,
	MAIL_ADDR_BCC = 2,
	MAIL_ADDR_FROM = 3,
};

enum EmailPriority
{
	// FIELD_PRIORITY is equivilent to the header field: X-Priority
	//	1 - High
	//	...
	//	5 - Low
	MAIL_PRIORITY_HIGH = 1,
	MAIL_PRIORITY_NORMAL = 3,
	MAIL_PRIORITY_LOW = 5,
};

enum CalendarReminderType
{
	CalEmail,
	CalPopup,
};

enum CalendarReminderUnits
{
	CalMinutes,
	CalHours,
	CalDays,
	CalWeeks
};

enum CalendarShowTimeAs
{
	CalFree				= 0,
	CalTentative		= 1,
	CalBusy				= 2,
	CalOut				= 3
};

enum CalendarType
{
	CalEvent,
	CalTodo,
	CalJournal,
	CalRequest,
	CalReply,
};


enum CalendarPrivacyType
{
	CalDefaultPriv		= 0,
	CalPublic,
	CalPrivate
};

enum Store3ItemTypes
{
	MAGIC_NONE						= 0x00000000,
	MAGIC_BASE						= 0xAAFF0000,
	MAGIC_MAIL						= (MAGIC_BASE+1),	// Mail item
	MAGIC_CONTACT					= (MAGIC_BASE+2),	// Contact item
	MAGIC_FOLDER_OLD				= (MAGIC_BASE+3),	// Old Store1 folder of items
	MAGIC_MAILBOX					= (MAGIC_BASE+4),	// Root of mail tree (nothing-abstract)
	MAGIC_ATTACHMENT				= (MAGIC_BASE+5),	// Mail attachment
	MAGIC_ANY						= (MAGIC_BASE+6),	// Trash folder type (accepts any object)
	MAGIC_FILTER					= (MAGIC_BASE+7),	// Used to match messages against
	MAGIC_FOLDER					= (MAGIC_BASE+8),	// Folder v2
	MAGIC_CONDITION					= (MAGIC_BASE+9),	// Filter condition
	MAGIC_ACTION					= (MAGIC_BASE+10),	// Filter action
	MAGIC_CALENDAR					= (MAGIC_BASE+11),	// Calendar event
	MAGIC_ATTENDEE					= (MAGIC_BASE+12),	// Event attendee
	MAGIC_GROUP						= (MAGIC_BASE+13),	// Group of contacts
	MAGIC_ADDRESS					= (MAGIC_BASE+14),	// Group of contacts
	MAGIC_MAX						= (MAGIC_BASE+15)	// One past the end
};

/// GDataI load state
enum Store3DataState
{
	Store3None,
	Store3Headers,
	Store3Loaded,
};

/// Folder system type
enum Store3SystemFolder
{
	Store3SystemNone,
	Store3SystemInbox,
	Store3SystemTrash,
	Store3SystemOutbox,
	Store3SystemSent,
	Store3SystemCalendar,
	Store3SystemContacts,
	Store3SystemSpam,
};

// When setting this via GDataI::SetInt the return value is:
// - true if you need to mark the object dirty so that it gets saved
// - false if the flags is stored elsewhere and you don't have to save.
enum Store3Fields
{
	FIELD_NULL = 0,
	FIELD_FLAGS = 1,					// (EmailFlags) The message flags
	FIELD_TO = 2,						// (DIterator<Store3Addr>) List of recipients
	FIELD_CC = 3,						// (EmailAddressType)
	FIELD_FROM = 4,						// (Store3Addr*) The From address
	FIELD_REPLY = 5,					// (Store3Addr*) The Reply-To address
	FIELD_SUBJECT = 6,					// (char*) Subject of the message
	FIELD_TEXT = 7,						// (char*) Textual body
	FIELD_MESSAGE_ID = 8,				// (char*) The message ID
	FIELD_DATE_RECEIVED = 9,			// (GDateTime*) Received date
	FIELD_INTERNET_HEADER = 10,			// (char*) The internet headers

	// Contact fields
	FIELD_FIRST_NAME = 11,				// (char*) First name
	FIELD_LAST_NAME = 12,				// (char*) Last/surname
	FIELD_EMAIL = 13,					// (char*) The default email address
										// \sa 'FIELD_ALT_EMAIL'
	FIELD_HOME_STREET = 14,				// (char*)
	FIELD_HOME_SUBURB = 15,				// (char*)
	FIELD_HOME_POSTCODE = 16,			// (char*)
	FIELD_HOME_STATE = 17,				// (char*)
	FIELD_HOME_COUNTRY = 18,			// (char*)
	FIELD_WORK_PHONE = 19,				// (char*) This was already defined
	FIELD_HOME_PHONE = 20,				// (char*)
	FIELD_HOME_MOBILE = 21,				// (char*)
	FIELD_HOME_IM = 22,					// (char*)
	FIELD_HOME_FAX = 23,				// (char*)
	FIELD_HOME_WEBPAGE = 24,			// (char*)
	FIELD_NICK = 25,					// (char*)
	FIELD_SPOUSE = 26,					// (char*)
	FIELD_NOTE = 27,					// (char*)
	FIELD_PLUGIN_ASSOC = 28,			// (char*)

	// More fields
	FIELD_SIZE = 29,					// (uint64)
	FIELD_DATE_SENT = 30,				// (GDateTime*)
	FIELD_COLUMN = 31,					// (uint64)
	// FIELD_BCC = 32,					// Deprecated
	FIELD_MIME_TYPE = 33,				// (char*) The MIME type
	FIELD_PRIORITY = 34,				// (EmailPriority)
	FIELD_FOLDER_OPEN = 35,				// (bool) True if the folder is expanded to show child folders
	// FIELD_CODE_PAGE = 36,			// Deprecated
	FIELD_COLOUR = 37,					// (uint32) Rgba32
	FIELD_ALTERNATE_HTML = 38,			// (char*) The HTML part of the message
	FIELD_CONTENT_ID = 39,				// (char*) An attachment content ID

	// Filter fields
	FIELD_FILTER_NAME = 40,				// (char*)
	// FIELD_ACT_TYPE = 46,				// Deprecated
	// FIELD_ACT_ARG = 47,				// Deprecated
	// FIELD_DIGEST_INDEX = 48,			// Deprecated
	FIELD_COMBINE_OP = 49,
	FIELD_FILTER_INDEX = 50,
	FIELD_FILTER_SCRIPT = 52,
	FIELD_STOP_FILTERING = 54,
	FIELD_FILTER_INCOMING = 55,
	FIELD_FILTER_OUTGOING = 56,
	FIELD_FILTER_CONDITIONS_XML = 57,
	FIELD_FILTER_ACTIONS_XML = 58,
	FIELD_FILTER_INTERNAL = 59,

	// Calendar fields
	// FIELD_CAL_START_LOCAL = 60,		// **deprecated**
	// FIELD_CAL_END_LOCAL = 61,		// **deprecated**
	FIELD_CAL_SUBJECT = 62,				// (char*) event title
	FIELD_CAL_LOCATION = 63,			// (char*) location of event
	FIELD_CAL_REMINDER_TIME = 64,		// (int64) minutes before event to remind user
	FIELD_CAL_REMINDER_ACTION = 65,		// (CalendarReminderType) The reminder type
	FIELD_CAL_REMINDER_UNIT = 66,		// (CalendarReminderUnits) The unit of time
	FIELD_CAL_SHOW_TIME_AS = 67,		// (CalendarShowTimeAs) Busy/Free etc

	FIELD_CAL_RECUR_FREQ = 68,			// (CalRecurFreq) Base time unit of recurring: e.g. Day/Week/Month/Year.
	FIELD_CAL_RECUR_INTERVAL = 69,		// (int) Number of FIELD_CAL_RECUR_FREQ units of time between recurring events.

	FIELD_CAL_RECUR_FILTER_DAYS = 70 ,	// (int) Bitmask of days: Bit 0 = Sunday, Bit 1 = Monday etc
	FIELD_CAL_RECUR_FILTER_MONTHS = 71,	// (int) Bitmask of months: Bit 0 = January, Bit 1 = Feburary etc
	FIELD_CAL_RECUR_FILTER_YEARS = 72,	// (char*) Comma separated list of years to filter on.

	FIELD_CAL_NOTES = 73,				// (char*) Textual notes
	FIELD_CAL_TYPE = 74,				// (CalendarType) The type of event
	FIELD_CAL_COMPLETED = 75,			// (int) 0 -> 100%
	FIELD_CAL_START_UTC = 76,			// (GDateTime*) The start time and date
	FIELD_CAL_END_UTC = 77,				// (GDateTime*) The end time and date

	FIELD_CAL_RECUR_FILTER_POS = 78,	// (char*) Comma separated list of integers defining positions in the month to filter on.
	FIELD_CAL_RECUR_END_DATE = 79,		// (GDateTime*) The end of recurence if FIELD_CAL_RECUR_END_TYPE == CalEndOnDate.
	FIELD_CAL_RECUR_END_COUNT = 80,		// (int) Times to recur if FIELD_CAL_RECUR_END_TYPE == CalEndOnCount.
	FIELD_CAL_RECUR_END_TYPE = 81,		// (CalRecurEndType) Which ending to use... needs an enum
	FIELD_CAL_RECUR = 82,				// (int) true if the event recurs.

	FIELD_CAL_TIMEZONE = 83,			// (char*) The timezone as text
	FIELD_CAL_PRIVACY = 84,				// (CalendarPrivacyType) The privacy setting

	// Attendee fields
	FIELD_ATTENDEE_NAME = 85,			// (char*)
	FIELD_ATTENDEE_EMAIL = 86,			// (char*)
	FIELD_ATTENDEE_ATTENDENCE = 87,		// (int)
	FIELD_ATTENDEE_NOTE = 88,			// (char*)
	FIELD_ATTENDEE_RESPONSE = 89,		// (int)

	// 2nd lot of contact fields
	FIELD_WORK_STREET = 90,				// (char*)
	FIELD_WORK_SUBURB = 91,				// (char*)
	FIELD_WORK_POSTCODE = 92,			// (char*)
	FIELD_WORK_STATE = 93,				// (char*)
	FIELD_WORK_COUNTRY = 94,			// (char*)
	// #define FIELD_WORK_PHONE is previously defined
	FIELD_WORK_MOBILE = 95,				// (char*)
	FIELD_WORK_IM = 96,					// (char*)
	FIELD_WORK_FAX = 97,				// (char*)
	FIELD_WORK_WEBPAGE = 98,			// (char*)
	FIELD_COMPANY = 99,					// (char*)

	FIELD_CONTACT_CUST_FLD1 = 100,		// (char*)
	FIELD_CONTACT_CUST_VAL1 = 101,		// (char*)
	FIELD_CONTACT_CUST_FLD2 = 102,		// (char*)
	FIELD_CONTACT_CUST_VAL2 = 103,		// (char*)
	FIELD_CONTACT_CUST_FLD3 = 104,		// (char*)
	FIELD_CONTACT_CUST_VAL3 = 105,		// (char*)
	FIELD_CONTACT_CUST_FLD4 = 106,		// (char*)
	FIELD_CONTACT_CUST_VAL4 = 107,		// (char*)
	FIELD_CONTACT_IMAGE	= 108,			// (GSurface*)

	// Misc additional fields
	FIELD_LABEL = 110,					// (char*) Mail label
	FIELD_CHARSET = 111,				// (char*) A character set
	FIELD_ALT_EMAIL = 112,				// (char*) Comma separated list of alternative,
										// non-default email addresses. The default addr 
										// is stored under 'FIELD_EMAIL'
	FIELD_UID = 113,					// (char*)
	FIELD_TITLE = 114,					// (char*)
	FIELD_TIMEZONE = 115,				// (char*)
	FIELD_REFERENCES = 116,				// (char*)
	FIELD_SERVER_UID = 117,				// (char*) Server identifier
	FIELD_FOLDER_PERM_READ = 118,		// (int64)
	FIELD_FOLDER_PERM_WRITE = 119,		// (int64)
	FIELD_GROUP_NAME = 120,				// (char*) The name of a contact group
	FIELD_HTML_CHARSET = 121,			// (char*) The character set of the HTML body part
	FIELD_POSITION = 122,				// (char*) Role in organisation
	FIELD_GROUP_LIST = 123,				// (char*)
	FIELD_FOLDER_THREAD = 124,			// (int64)
	FIELD_ACCOUNT_ID = 125,				// (int64) The ID of an account
	FIELD_FROM_CONTACT_NAME = 126,		// (char*) Not used at the Store3 level
	FIELD_FWD_MSG_ID = 127,				// (char*) Mail::FwdMsgId

	FIELD_FOLDER_TYPE = 128,			// (Store3ItemTypes)
	FIELD_FOLDER_NAME = 129,			// (char*) Name of the folder
	FIELD_UNREAD = 130,					// (int64) Count of unread items
	FIELD_SORT = 131,					// (int64) Sort setting

	FIELD_STATUS = 132,					// (int64)
	FIELD_VERSION = 133,				// (int64)
	FIELD_ID = 134,						// (int64)
	FIELD_READONLY = 135,				// (bool)
	FIELD_NAME = 136,					// (char*)
	FIELD_WIDTH = 137,					// (int64)
	// #define FIELD_ATTACHMENT_CONTENT	139	- Deprecated
	// #define FIELD_ATTACHMENTS		140 - Deprecated, use FIELD_MIME_SEG instead
	FIELD_CACHE_FILENAME = 141,			// (char*) IMAP backend: the filename
	FIELD_CACHE_FLAGS = 142,			// (char*) IMAP backend: IMAP flags
	FIELD_DONT_SHOW_PREVIEW = 143,		// (bool)
	FIELD_STORE_PASSWORD = 144,			// (char*)
	FIELD_LOADED = 145,					// (Store3DataState)
	FIELD_DEBUG = 146,					// (char*)
	FIELD_MIME_SEG = 147,				// (GDataIt)
	FIELD_IS_IMAP = 148,				// (bool)
	FIELD_BOUNCE_MSG_ID = 149,			// (char*)
	FIELD_FOLDER_INDEX = 150,			// (int64)
	FIELD_FORMAT = 151,					// (int64)
	FIELD_SYSTEM_FOLDER = 152,			// (Store3SystemFolder)
	FIELD_ERROR = 153,					// (char*) An error message
	FIELD_TYPE = 154,					// (char*) The type of the object

	FIELD_MAX = 157,
};

#endif