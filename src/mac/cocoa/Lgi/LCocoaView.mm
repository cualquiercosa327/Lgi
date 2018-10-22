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

#define Check() if (!self.v) return
@implementation LCocoaView

- (id)init:(GView*)view
{
	if ((self = [super initWithFrame:view->GetPos()]) != nil)
	{
		self.v = view;
		[self setAutoresizingMask:NSViewNotSizable];
		[self setTranslatesAutoresizingMaskIntoConstraints:YES];
	}
	return self;
}

- (void)dealloc
{
	self.v = nil;
	[super dealloc];
}

- (void)drawRect:(NSRect)dirtyRect
{
	Check();
	GScreenDC Dc(self.v);
	self.v->OnPaint(&Dc);
}

- (void)layout
{
	Check();
	self.v->OnCocoaLayout();
}

- (void)mouseDown:(NSEvent*)ev
{
	Check();
	GMouse m;
	m.SetFromEvent(ev, self);
	self.v->OnMouseClick(m);
}

- (void)mouseUp:(NSEvent*)ev
{
	Check();
	GMouse m;
	m.SetFromEvent(ev, self);
	self.v->OnMouseClick(m);
}

- (void)rightMouseDown:(NSEvent*)ev
{
	Check();
	GMouse m;
	m.SetFromEvent(ev, self);
	self.v->OnMouseClick(m);
}

- (void)rightMouseUp:(NSEvent*)ev
{
	Check();
	GMouse m;
	m.SetFromEvent(ev, self);
	self.v->OnMouseClick(m);
}

- (void)mouseMoved:(NSEvent*)ev
{
	Check();
	GMouse m;
	m.SetFromEvent(ev, self);
	self.v->OnMouseMove(m);
}

@end
