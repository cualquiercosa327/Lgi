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
#include "LDateTime.h"
#include "GContainers.h"
#include "LHashTable.h"
#include "GString.h"

class GCompiledCode;

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
	/// LDateTime class.
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
	GV_GKEY,
	/// Pointer to GStream
	GV_STREAM,
	/// The maximum value for the variant type.
	/// (This is used by the scripting engine to refer to a GVariant itself)
	GV_MAX,
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

class LgiClass GCustomType : public GDom
{
protected:
	struct CustomField : public GDom
	{
		ssize_t Offset;
		ssize_t Bytes;
		ssize_t ArrayLen;
		GVariantType Type;
		GString Name;
		GCustomType *Nested;

		ssize_t Sizeof();
		bool GetVariant(const char *Name, GVariant &Value, char *Array = NULL);
	};

public:
	struct Method : public GDom
	{
		GString Name;
		GArray<GString> Params;
		size_t Address;
		int FrameSize;
		
		Method()
		{
			Address = -1;
			FrameSize = -1;
		}
	};

protected:
	// Global vars
	int Pack;
	size_t Size;
	GString Name;

	// Fields
	GArray<CustomField*> Flds;
	LHashTbl<ConstStrKey<char,false>, int> FldMap;
	
	// Methods
	GArray<Method*> Methods;
	LHashTbl<ConstStrKey<char,false>, Method*> MethodMap;
	
	// Private methods
	ssize_t PadSize();

public:
	GCustomType(const char *name, int pack = 1);
	GCustomType(const char16 *name, int pack = 1);
	~GCustomType();
	
	size_t Sizeof();
	const char *GetName() { return Name; }
	ssize_t Members() { return Flds.Length(); }
	int AddressOf(const char *Field);
	int IndexOf(const char *Field);
	bool DefineField(const char *Name, GVariantType Type, int Bytes, int ArrayLen = 1);
	bool DefineField(const char *Name, GCustomType *Type, int ArrayLen = 1);
	Method *DefineMethod(const char *Name, GArray<GString> &Params, size_t Address);
	Method *GetMethod(const char *Name);

	// Field access. You can't use the GDom interface to get/set member variables because
	// there is no provision for the 'This' pointer.
	bool Get(int Index, GVariant &Out, uint8 *This, int ArrayIndex = 0);
	bool Set(int Index, GVariant &In, uint8 *This, int ArrayIndex = 0);
	
	// Dom access. However the DOM can be used to access information about the type itself.
	// Which doesn't need a 'This' pointer.
	bool GetVariant(const char *Name, GVariant &Value, char *Array = NULL);
	bool SetVariant(const char *Name, GVariant &Value, char *Array = NULL);
	bool CallMethod(const char *MethodName, GVariant *ReturnValue, GArray<GVariant*> &Args);
};

/// A class that can be different types
class LgiClass GVariant
{
public:
	typedef LHashTbl<ConstStrKey<char>,GVariant*> LHash;

	/// The type of the variant
    GVariantType Type;

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
		    ssize_t Length;
		    void *Data;
	    } Binary;
		/// Valid when Type == #GV_LIST
	    List<GVariant> *Lst;
		/// Valid when Type == #GV_HASHTABLE
	    LHash *Hash;
		/// Valid when Type == #GV_DATETIME
		LDateTime *Date;
		/// Valid when Type == #GV_CUSTOM
		struct _Custom
		{
			GCustomType *Dom;
			uint8 *Data;

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
		struct
		{
		    class GSurface *Ptr;
		    bool Own;
		} Surface;
		/// Valid when Type == #GV_STREAM
		struct
		{
			class GStream *Ptr;
			bool Own;
		} Stream;
		/// Valid when Type == #GV_GVIEW
		class GView *View;
		/// Valid when Type == #GV_GMOUSE
		class GMouse *Mouse;
		/// Valid when Type == #GV_GKEY
		class GKey *Key;
	} Value;

	/// Constructor to null
	GVariant();
	/// Constructor for integers
	GVariant(int32 i);
	GVariant(uint32 i);
	GVariant(int64 i);
	GVariant(uint64 i);
	#ifndef _MSC_VER
	GVariant(size_t i);
	#if LGI_64BIT || defined(MAC)
	GVariant(ssize_t i);
	#endif
	#endif
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
	GVariant(LDateTime *d);
	/// Constructor for variant
	GVariant(GVariant const &v);
	/// Construtor for operator
	GVariant(GOperator Op);
	/// Destructor
	~GVariant();

	/// Assign bool value
	GVariant &operator =(bool i);
	/// Assign an integer value
	GVariant &operator =(int32 i);
	GVariant &operator =(uint32 i);
	GVariant &operator =(int64 i);
	GVariant &operator =(uint64 i);
	#ifndef _MSC_VER
	GVariant &operator =(size_t i);
	#if LGI_64BIT || defined(MAC)
	GVariant &operator =(ssize_t i);
	#endif
	#endif
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
	GVariant &operator =(LDateTime *d);

	GVariant &operator =(class GView *p);
	GVariant &operator =(class GMouse *p);
	GVariant &operator =(class GKey *k);
	GVariant &operator =(class GStream *s);

	bool operator ==(GVariant &v);
	bool operator !=(GVariant &v) { return !(*this == v); }

	/// Sets the value to a DOM variable reference
	bool SetDomRef(GDom *obj, char *name);
	/// Sets the value to a copy of	block of binary data
	bool SetBinary(ssize_t Len, void *Data, bool Own = false);
	/// Sets the value to a copy of the list
	bool SetList(List<GVariant> *Lst = 0);
	/// Sets the value to a hashtable
	bool SetHashTable(LHash *Table = 0, bool Copy = true);
    /// Set the value to a surface
    bool SetSurface(class GSurface *Ptr, bool Own);
    /// Set the value to a stream
    bool SetStream(class GStream *Ptr, bool Own);

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
	bool OwnStr(char *s);
	/// Sets the variant to a wide heap string and takes ownership of it
	bool OwnStr(char16 *s);
	/// Sets the variant to NULL
	void Empty();
	/// Returns the byte length of the data
	int64 Length();

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
	/// Casts to a boolean. You probably DON'T want to use this function. The
	/// behaviour for strings -> bool is such that if the string is value it
	/// always evaluates to true, and false if it's not a valid string. Commonly
	/// what you want is to evaluate whether the string is zero or non-zero in
	/// which cast you should use "CastInt32() != 0" instead.
	bool CastBool();
	/// Returns the pointer if available.
	void *CastVoidPtr();
	/// Returns a GView
	GView *CastView() { return Type == GV_GVIEW ? Value.View : NULL; }

	/// List insert
	bool Add(GVariant *v, int Where = -1);	
	
	/// Converts the varient type to a string
	static const char *TypeToString(GVariantType t);
	/// Converts an operator to a string
	static const char *OperatorToString(GOperator op);
	/// Converts the varient value to a string
	GString ToString();
};

#endif
