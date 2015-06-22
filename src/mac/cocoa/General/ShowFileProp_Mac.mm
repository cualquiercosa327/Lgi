#include "Lgi.h"
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

