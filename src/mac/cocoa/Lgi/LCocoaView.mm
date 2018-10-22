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

#define check() if (!self.v) return
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
	check();
	GScreenDC Dc(self.v);
	self.v->OnPaint(&Dc);
}

- (void)layout
{
	check();
	self.v->OnPosChange();
	GAutoPtr<GViewIterator> views(self.v->IterateViews());
	for (auto c = views->First(); c; c = views->Next())
	{
		OsView h = c->Handle();
		if (h)
		{
			GRect Flip = c->GetPos();
			if (h.p.superview)
				Flip = LFlip(h.p.superview, Flip);
			[h.p setFrame:Flip];
		}
	}
}

- (void)mouseDown:(NSEvent*)ev
{
	check();
	GMouse m;
	m.SetFromEvent(ev, self);
	self.v->OnMouseClick(m);
}

- (void)mouseUp:(NSEvent*)ev
{
	check();
	GMouse m;
	m.SetFromEvent(ev, self);
	self.v->OnMouseClick(m);
}

- (void)rightMouseDown:(NSEvent*)ev
{
	check();
	GMouse m;
	m.SetFromEvent(ev, self);
	self.v->OnMouseClick(m);
}

- (void)rightMouseUp:(NSEvent*)ev
{
	check();
	GMouse m;
	m.SetFromEvent(ev, self);
	self.v->OnMouseClick(m);
}

- (void)mouseMoved:(NSEvent*)ev
{
	check();
	GMouse m;
	m.SetFromEvent(ev, self);
	self.v->OnMouseMove(m);
}

@end
