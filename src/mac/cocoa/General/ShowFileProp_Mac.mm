#include "Lgi.h"
#include <stdlib.h>
#import <Cocoa/Cocoa.h>

void LgiShowFileProperties(OsView Parent, const char *Filename)
{
	NSPasteboard *pboard = [NSPasteboard pasteboardWithUniqueName];
	[pboard declareTypes:[NSArray arrayWithObject:NSStringPboardType] owner:nil];

	NSMutableArray *fileList = [NSMutableArray new];

	//Add as many as file's path in the fileList array
	NSString *fn = [NSString stringWithUTF8String:Filename];
	[fileList addObject:fn];

	[pboard setPropertyList:fileList forType:NSFilenamesPboardType];
	NSPerformService(@"Finder/Show Info", pboard);
}

bool LgiBrowseToFile(const char *Filename)
{
	GString s;
	s.Printf("/usr/bin/osascript "
			"-e 'set asd to POSIX file \"%s\" as string\n"
			"tell application \"Finder\"\n"
			"	reveal asd\n"
			"	activate\n"
			"end tell'",
			Filename);
	// printf("s='%s'\n", s.Get());
	return system(s) >= 0;
}
