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
			{
				GRect Pr = h.p.superview.frame;
				if (Pr.Valid())
				{
					int y2 = Pr.y2 - Flip.y1;
					int y1 = y2 - Flip.Y() + 1;
					Flip.Offset(0, y1-Flip.y1);
				}
			}
			
			[h.p setFrame:Flip];
		}
	}
}

- (void)mouseDown:(NSEvent*)ev
{
	auto r = self.superview.frame;

	GMouse m;
	m.Down(true);
	m.x = (int)ev.locationInWindow.x;
	m.y = (int)(r.size.height - ev.locationInWindow.y);
	m.Left(ev.type == NSEventTypeLeftMouseDown);
	m.SetModifer((uint32)ev.modifierFlags);
	m.Double(ev.clickCount == 2);
	
	v->OnMouseClick(m);
}

- (void)mouseUp:(NSEvent*)ev
{
	auto r = self.superview.frame;

	GMouse m;
	m.Down(false);
	m.x = (int)ev.locationInWindow.x;
	m.y = (int)(r.size.height - ev.locationInWindow.y);
	m.Left(true);
	m.SetModifer((uint32)ev.modifierFlags);
	
	v->OnMouseClick(m);
}

- (void)rightMouseDown:(NSEvent*)ev
{
	auto r = self.superview.frame;

	GMouse m;
	m.Down(true);
	m.x = (int)ev.locationInWindow.x;
	m.y = (int)(r.size.height - ev.locationInWindow.y);
	m.Right(true);
	m.SetModifer((uint32)ev.modifierFlags);
	m.Double(ev.clickCount == 2);
	
	v->OnMouseClick(m);
}

- (void)rightMouseUp:(NSEvent*)ev
{
	auto r = self.superview.frame;

	GMouse m;
	m.Down(false);
	m.x = (int)ev.locationInWindow.x;
	m.y = (int)(r.size.height - ev.locationInWindow.y);
	m.Right(true);
	m.SetModifer((uint32)ev.modifierFlags);
	
	v->OnMouseClick(m);
}

- (void)mouseMoved:(NSEvent*)ev
{
	auto r = self.superview.frame;

	GMouse m;
	m.IsMove(true);
	m.x = (int)ev.locationInWindow.x;
	m.y = (int)(r.size.height - ev.locationInWindow.y);
	m.Left(true);
	m.SetModifer((uint32)ev.modifierFlags);
	
	v->OnMouseMove(m);
}

@end
