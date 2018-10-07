// StoreConvert1To2.h

// Parent:
//		Pointer to the parent window, or NULL.
//
// InFile:
//		Filename of the v1 folder file to convert, or NULL to ask the
//		user for the filename.
//
// OutFile:
//		If this points to an empty string it is assumed to point to a 
//		buffer that can accept the file name selected by the user.
//		If it points to a filename, then that filename is used to write
//		the v2 folder file to.
//		If NULL then the user is asked for a filename and the new file is
//		not returned.
extern bool ConvertStorage1To2(GView *Parent, char *InFile = 0, char *OutFile = 0);
