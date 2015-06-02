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
	FIELD_WORK_PHONE = 19,				// this was already defined
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
	FIELD_CAL_START_LOCAL = 60,			// **deprecated**
	FIELD_CAL_END_LOCAL = 61,			// **deprecated**
	FIELD_CAL_SUBJECT = 62,				// (char*) event title
	FIELD_CAL_LOCATION = 63,			// (char*) location of event
	FIELD_CAL_REMINDER_TIME = 64,		// (int) minutes before event to remind user
	FIELD_CAL_REMINDER_ACTION = 65,		// (CalReminderType) ??
	FIELD_CAL_REMINDER_ARG = 66,		// (char*) usused anywhere in code
	FIELD_CAL_SHOW_TIME_AS = 67,		// (ScribeShowTimeAs) Busy/Free etc

	FIELD_CAL_RECUR_FREQ = 68,			// (int)
	FIELD_CAL_RECUR_INTERVAL = 69,		// (int)

	FIELD_CAL_RECUR_FILTER_DAYS = 70 ,	// (int)
	FIELD_CAL_RECUR_FILTER_MONTHS = 71,	// (int)
	FIELD_CAL_RECUR_FILTER_YEARS = 72,	// (char*)

	FIELD_CAL_NOTES = 73,				// (char*) Textual notes
	FIELD_CAL_TYPE = 74,				// (CalendarType) The type of event
	FIELD_CAL_COMPLETED = 75,			// (int) 0 -> 100%
	FIELD_CAL_START_UTC = 76,			// (GDateTime*) The start time and date
	FIELD_CAL_END_UTC = 77,				// (GDateTime*) The end time and date

	FIELD_CAL_RECUR_FILTER_POS = 78,	// (char*) ??
	FIELD_CAL_RECUR_END_DATE = 79,		// (GDateTime*) The end of recurence.
	FIELD_CAL_RECUR_END_COUNT = 80,		// (int) Times to recur?
	FIELD_CAL_RECUR_END_TYPE = 81,		// (int) Which ending to use... needs an enum
	FIELD_CAL_RECUR = 82,				// (int) Boolean if the event recurs?

	FIELD_CAL_TIMEZONE = 83,			// (char*) The timezone as text
	FIELD_CAL_PRIVACY = 84,				// (ScribeCalPrivacy) The privacy setting

	// Attendee fields
	FIELD_ATTENDEE_NAME = 85,			// (char*)
	FIELD_ATTENDEE_EMAIL = 86,			// (char*)
	FIELD_ATTENDEE_ATTENDENCE = 87,		// (int)
	FIELD_ATTENDEE_NOTE = 88,			// (char*)
	FIELD_ATTENDEE_RESPONSE = 89,		// (int)

	// 2nd lot of contact fields
	FIELD_WORK_STREET = 90,
	FIELD_WORK_SUBURB = 91,
	FIELD_WORK_POSTCODE = 92,
	FIELD_WORK_STATE = 93,
	FIELD_WORK_COUNTRY = 94,
	// #define FIELD_WORK_PHONE is previously defined
	FIELD_WORK_MOBILE = 95,
	FIELD_WORK_IM = 96,
	FIELD_WORK_FAX = 97,
	FIELD_WORK_WEBPAGE = 98,
	FIELD_COMPANY = 99,

	FIELD_CONTACT_CUST_FLD1 = 100,
	FIELD_CONTACT_CUST_VAL1 = 101,
	FIELD_CONTACT_CUST_FLD2 = 102,
	FIELD_CONTACT_CUST_VAL2 = 103,
	FIELD_CONTACT_CUST_FLD3 = 104,
	FIELD_CONTACT_CUST_VAL3 = 105,
	FIELD_CONTACT_CUST_FLD4 = 106,
	FIELD_CONTACT_CUST_VAL4 = 107,

	// Misc additional fields
	FIELD_LABEL = 110,					// Mail label
	FIELD_CHARSET = 111,
	FIELD_ALT_EMAIL = 112,				// alternative, non-deafult email addresses
										// the default addr is stored under 'FIELD_EMAIL'
	FIELD_UID = 113,
	FIELD_TITLE = 114,
	FIELD_TIMEZONE = 115,
	FIELD_REFERENCES = 116,
	FIELD_SERVER_UID = 117,				// char *Mail::ServerUid
	FIELD_FOLDER_PERM_READ = 118,
	FIELD_FOLDER_PERM_WRITE = 119,
	FIELD_GROUP_NAME = 120,
	FIELD_HTML_CHARSET = 121,
	FIELD_ADDRESSED_TO = 122,			// deprecated
	FIELD_GROUP_LIST = 123,
	FIELD_FOLDER_THREAD = 124,
	FIELD_ACCOUNT_ID = 125,
	FIELD_FROM_CONTACT_NAME = 126,
	FIELD_FWD_MSG_ID = 127,				// Mail::FwdMsgId

	FIELD_FOLDER_TYPE = 128,			// enum ScribeItemTypes
	FIELD_FOLDER_NAME = 129,
	FIELD_UNREAD = 130,
	FIELD_SORT = 131,

	FIELD_STATUS = 132,
	FIELD_VERSION = 133,
	FIELD_ID = 134,
	FIELD_READONLY = 135,
	FIELD_NAME = 136,
	FIELD_WIDTH = 137,
	// #define FIELD_ATTACHMENT_CONTENT	139	- Deprecated
	// #define FIELD_ATTACHMENTS		140 - Deprecated, use FIELD_MIME_SEG instead
	FIELD_CACHE_FILENAME = 141,
	FIELD_CACHE_FLAGS = 142,
	FIELD_DONT_SHOW_PREVIEW = 143,
	FIELD_STORE_PASSWORD = 144,
	FIELD_LOADED = 145,					// returns a Store3DataState
	FIELD_DEBUG = 146,
	FIELD_MIME_SEG = 147,
	FIELD_IS_IMAP = 148,
	FIELD_BOUNCE_MSG_ID = 149,
	FIELD_FOLDER_INDEX = 150,
	FIELD_FORMAT = 151,
	FIELD_SYSTEM_FOLDER = 152,			// Store3SystemFolder
	FIELD_ERROR = 153,
	FIELD_TYPE = 154,
	FIELD_WEB_LOGIN_URI = 155,

	FIELD_MAX = 157,
};

#endif