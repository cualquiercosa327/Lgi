/**
	\file
	\author Matthew Allen
	\brief Variant class.\n
	Copyright (C), <a href="mailto:fret@memecode.com">Matthew Allen</a>
 */

#ifndef __GVARIANT_H__
#define __GVARIANT_H__

#include "GDom.h"
#undef Bool
#include "GDateTime.h"
#include "GContainers.h"
#include "GHashTable.h"

/// The different types the varient can be 
enum GVariantType
{
	// Main types
	
	/// Null type
	GV_NULL,
	/// 32-bit integer
	GV_INT32,
	/// 64-bit integer
	GV_INT64,
	/// true or false boolean.
	GV_BOOL,
	/// C++ double
	GV_DOUBLE,
	/// Null terminated string value
	GV_STRING,
	/// Block of binary data
	GV_BINARY,
	/// List of GVariant
	GV_LIST,
	/// Pointer to GDom object
	GV_DOM,
	/// DOM reference, ie. a variable in a DOM object
	GV_DOMREF,
	/// Untyped pointer
	GV_VOID_PTR,
	/// GDateTime class.
	GV_DATETIME,
	/// Hash table class, containing pointers to GVariants
	GV_HASHTABLE,
	// Scripting language operator
	GV_OPERATOR,
	// Custom scripting lang type
	GV_CUSTOM,
	// Wide string
	GV_WSTRING,
	// GSurface ptr
	GV_GSURFACE,
	/// Pointer to GView
	GV_GVIEW,
	/// Pointer to GMouse
	GV_GMOUSE,
	/// Pointer to GKey
	GV_GKEY

};

/// Language operators
enum GOperator
{
	OpNull,
	OpAssign,
	OpPlus,
	OpUnaryPlus,
	OpMinus,
	OpUnaryMinus,
	OpMul,
	OpDiv,
	OpMod,
	OpLessThan,
	OpLessThanEqual,
	OpGreaterThan,
	OpGreaterThanEqual,
	OpEquals,
	OpNotEquals,
	OpPlusEquals,
	OpMinusEquals,
	OpMulEquals,
	OpDivEquals,
	OpPostInc,
	OpPostDec,
	OpPreInc,
	OpPreDec,
	OpAnd,
	OpOr,
	OpNot,
};

/// A class that can be different types
class LgiClass GVariant
{
public:
	/// The type of the variant
    GVariantType Type;
    /// User data
    uint32 User;

    /// The value of the variant
	union
    {
		/// Valid when Type == #GV_INT32
	    int Int;
		/// Valid when Type == #GV_BOOL
	    bool Bool;
		/// Valid when Type == #GV_INT64
	    int64 Int64;
		/// Valid when Type == #GV_DOUBLE
	    double Dbl;
	    /// Valid when Type == #GV_STRING
		char *String;
	    /// Valid when Type == #GV_WSTRING
		char16 *WString;
		/// Valid when Type == #GV_DOM
		GDom *Dom;
		/// Valid when Type is #GV_VOID_PTR, #GV_GVIEW, #GV_GMOUSE or #GV_GKEY
		void *Ptr;
		/// Valid when Type == #GV_BINARY
	    struct _Binary
	    {
		    int Length;
		    void *Data;
	    } Binary;
		/// Valid when Type == #GV_LIST
	    List<GVariant> *Lst;
		/// Valid when Type == #GV_HASHTABLE
	    GHashTable *Hash;
		/// Valid when Type == #GV_DATETIME
		GDateTime *Date;
		/// Valid when Type == #GV_CUSTOM
		struct _Custom
		{
			GDom *Dom;
			char *Data;

			bool operator == (_Custom &c)
			{
				return Dom == c.Dom &&
						Data == c.Data;
			}
		} Custom;
		/// Valid when Type == #GV_DOMREF
		struct _DomRef
		{
			/// The pointer to the dom object
			GDom *Dom;
			/// The name of the variable to set/get in the dom object
			char *Name;
		} DomRef;
		/// Valid when Type == #GV_OPERATOR
		GOperator Op;
		/// Valid when Type == #GV_GSURFACE
		class GSurface *Surface;
		/// Valid when Type == #GV_GVIEW
		class GView *View;
		/// Valid when Type == #GV_GMOUSE
		class GMouse *Mouse;
		/// Valid when Type == #GV_GKEY
		class GKey *Key;
	} Value;

	/// Constructor to null
	GVariant();
	/// Constructor for int
	GVariant(int i);
	/// Constructor for int64
	GVariant(int64 i);
	/// Constructor for uint64
	GVariant(uint64 i);
	/// Constructor for double
	GVariant(double i);
	/// Constructor for string
	GVariant(const char *s);
	/// Constructor for wide string
	GVariant(const char16 *s);
	/// Constructor for ptr
	GVariant(void *p);
	/// Constructor for DOM ptr
	GVariant(GDom *p);
	/// Constructor for DOM variable reference
	GVariant(GDom *p, char *name);
	/// Constructor for date
	GVariant(GDateTime *d);
	/// Constructor for variant
	GVariant(GVariant const &v);
	/// Construtor for operator
	GVariant(GOperator Op);
	/// Destructor
	~GVariant();

	/// Assign int value
	GVariant &operator =(int i);
	/// Assign bool value
	GVariant &operator =(bool i);
	/// Assign int value
	GVariant &operator =(int64 i);
	/// Assign double value
	GVariant &operator =(double i);
	/// Assign string value (makes a copy)
	GVariant &operator =(const char *s);
	/// Assign a wide string value (makes a copy)
	GVariant &operator =(const char16 *s);
	/// Assign another variant value
	GVariant &operator =(GVariant const &i);
	/// Assign value to a void ptr
	GVariant &operator =(void *p);
	/// Assign value to DOM ptr
	GVariant &operator =(GDom *p);
	/// Assign value to be a date/time
	GVariant &operator =(GDateTime *d);

	GVariant &operator =(class GView *p);
	GVariant &operator =(class GMouse *p);
	GVariant &operator =(class GKey *k);

	bool operator ==(GVariant &v);

	/// Sets the value to a DOM variable reference
	bool SetDomRef(GDom *obj, char *name);
	/// Sets the value to a copy of	block of binary data
	bool SetBinary(int Len, void *Data, bool Own = false);
	/// Sets the value to a copy of the list
	bool SetList(List<GVariant> *Lst = 0);
	/// Sets the value to a hashtable
	bool SetHashTable(GHashTable *Table = 0, bool Copy = true);

	/// Returns the string if valid (will convert a GV_WSTRING to utf)
	char *Str();
	/// Returns a wide string if valid (will convert a GV_STRING to wide)
	char16 *WStr();
	/// Returns the string, releasing ownership of the memory to caller and
	/// changing this GVariant to GV_NULL.
	char *ReleaseStr();
	/// Returns the wide string, releasing ownership of the memory to caller and
	/// changing this GVariant to GV_NULL.
	char16 *ReleaseWStr();
	/// Sets the variant to a heap string and takes ownership of it
	void OwnStr(char *s);
	/// Sets the variant to a wide heap string and takes ownership of it
	void OwnStr(char16 *s);
	/// Sets the variant to NULL
	void Empty();

	/// True if currently a int
	bool IsInt();
	/// True if currently a bool
	bool IsBool();
	/// True if currently a double
	bool IsDouble();
	/// True if currently a string
	bool IsString();
	/// True if currently a binary block
	bool IsBinary();
	/// True if currently null
	bool IsNull();

	/// Changes the variant's type, maintaining the value where possible. If
	/// no conversion is available then nothing happens.
	GVariant &Cast(GVariantType NewType);
	/// Casts the value to int, from whatever source type. The
	/// GVariant type does not change after calling this.
	int32 CastInt32();
	/// Casts the value to a 64 bit int, from whatever source type. The
	/// GVariant type does not change after calling this.
	int64 CastInt64();
	/// Casts the value to double, from whatever source type. The
	/// GVariant type does not change after calling this.
	double CastDouble();
	/// Cast to a string from whatever source type, the GVariant will
	/// take the type GV_STRING after calling this. This is because
	/// returning a static string is not thread safe.
	char *CastString();
	/// Casts to a DOM ptr
	GDom *CastDom();
	/// Casts to a boolean
	bool CastBool();
	/// Returns the pointer if available.
	void *CastVoidPtr();
};

#endif
