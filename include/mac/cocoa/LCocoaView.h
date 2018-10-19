//
//  LCocoaView.h
//  LgiCocoa
//
//  Created by Matthew Allen on 20/10/18.
//  Copyright © 2018 Memecode. All rights reserved.
//

#ifndef LCocoaView_h
#define LCocoaView_h

// This class wraps a Cocoa NSView and redirects all the calls to LGI's GView object.
@interface LCocoaView : NSView
{
	GView *v;
}
- (id)init:(GView*)view;
- (void)dealloc;
- (void)drawRect:(NSRect)dirtyRect;
@end


#endif /* LCocoaView_h */
