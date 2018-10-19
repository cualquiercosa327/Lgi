//
//  LCocoaView.m
//  LgiTest
//
//  Created by Matthew Allen on 20/10/18.
//  Copyright © 2018 Matthew Allen. All rights reserved.
//

#import "Lgi.h"
#import <Foundation/Foundation.h>
#import "LCocoaView.h"

@implementation LCocoaView

- (id)init:(GView*)view
{
	if ((self = [super initWithFrame:view->GetPos()]) != nil)
	{
		v = view;
	}
	return self;
}

- (void)dealloc
{
	v = nil;
	[super dealloc];
}

- (void)drawRect:(NSRect)dirtyRect
{
	GScreenDC Dc(v);
	v->OnPaint(&Dc);
}

@end
