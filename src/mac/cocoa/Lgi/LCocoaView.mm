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
		[self setAutoresizingMask:NSViewNotSizable];
		[self setTranslatesAutoresizingMaskIntoConstraints:YES];
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
	// auto Cls = v->GetClass();
	GScreenDC Dc(v);
	v->OnPaint(&Dc);
}


- (void)layout
{
	v->OnPosChange();
	GAutoPtr<GViewIterator> views(v->IterateViews());
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
	GMouse m;
	m.SetFromEvent(ev, self);
	v->OnMouseClick(m);
}

- (void)mouseUp:(NSEvent*)ev
{
	GMouse m;
	m.SetFromEvent(ev, self);
	v->OnMouseClick(m);
}

- (void)rightMouseDown:(NSEvent*)ev
{
	GMouse m;
	m.SetFromEvent(ev, self);
	v->OnMouseClick(m);
}

- (void)rightMouseUp:(NSEvent*)ev
{
	GMouse m;
	m.SetFromEvent(ev, self);
	v->OnMouseClick(m);
}

- (void)mouseMoved:(NSEvent*)ev
{
	GMouse m;
	m.SetFromEvent(ev, self);
	v->OnMouseMove(m);
}

@end
