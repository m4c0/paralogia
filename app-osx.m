#import <AppKit/AppKit.h>

#include "mtl.h"

@interface POCWindow : NSWindow
@end
@implementation POCWindow
- (void)mouseEvent:(NSEvent *)event callback:(void(*)(int, int))cb {
  NSView * v = self.contentViewController.view;
  CGPoint liw = [event locationInWindow];
  CGPoint p = [v convertPoint:liw fromView:nil];
  cb(p.x, v.frame.size.height - p.y);
}
- (void)mouseDown:(NSEvent *)event {
  [self mouseEvent:event callback:g3d_mouse_down];
}
- (void)mouseUp:(NSEvent *)event {
  [self mouseEvent:event callback:g3d_mouse_up];
}
- (void)mouseMoved:(NSEvent *)event {
  [self mouseEvent:event callback:g3d_mouse_move];
}
- (void)mouseDragged:(NSEvent *)event {
  [self mouseEvent:event callback:g3d_mouse_move];
}

- (void)keyEvent:(NSEvent *)event down:(int)state {
  NSString * chrs = event.charactersIgnoringModifiers;
  if (chrs.length != 1) return;

  unichar c = [chrs characterAtIndex:0];
  switch (c) {
    case NSLeftArrowFunctionKey:  g3d_key(g3d_key_left,  state); break;
    case NSRightArrowFunctionKey: g3d_key(g3d_key_right, state); break;
    case NSUpArrowFunctionKey:    g3d_key(g3d_key_up,    state); break;
    case NSDownArrowFunctionKey:  g3d_key(g3d_key_down,  state); break;

    case 13: g3d_key(g3d_key_action, state); break;
    case 27: g3d_key(g3d_key_cancel, state); break;
    case 32: g3d_key(g3d_key_action, state); break;
  }
}
- (void)keyDown:(NSEvent *)event {
  [self keyEvent:event down:1];
}
- (void)keyUp:(NSEvent *)event {
  [self keyEvent:event down:0];
}

- (void)scrollWheel:(NSEvent *)event {
  // TODO should we consider hasPreciseScrollingDeltas?
  g3d_scroll(event.scrollingDeltaX, event.scrollingDeltaY);
}

- (void)magnifyWithEvent:(NSEvent *)event {
  g3d_zoom(event.magnification * 4.f);
}
@end

@interface POCAppDelegate : NSObject<NSApplicationDelegate>
@end
@implementation POCAppDelegate
- (void)applicationWillTerminate:(NSApplication *)app {
  g3d_deinit();
}
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)app {
  return YES;
}
@end

static void run() {
  NSViewController * vc = [NSViewController new];
  vc.view = [POCViewDelegate new];

  POCWindow * w = [POCWindow new];
  w.acceptsMouseMovedEvents = YES;
  w.contentViewController = vc;
  w.styleMask = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable;

  NSRect crect = NSMakeRect(0, 0, 800, 600);
  NSRect frect = [w frameRectForContentRect:crect];
  [w setFrame:frect display:YES];
  [w center];
  [w makeKeyAndOrderFront:w];

  // Apple menu
  NSMenu * menu = [NSMenu new];
  [menu       addItem:[[NSMenuItem alloc]
        initWithTitle:@"Quit Chesstor"
               action:@selector(terminate:)
        keyEquivalent:@"q"]];

  NSMenuItem * item = [NSMenuItem new];
  item.submenu = menu;

  NSMenu * bar = [NSMenu new];
  [bar addItem:item];

  NSApplication * a = [NSApplication sharedApplication];
  a.delegate = [POCAppDelegate new];
  a.mainMenu = bar;
  [a activateIgnoringOtherApps:YES];
  [a run];
}

int main() {
  @autoreleasepool {
    run();
  }
}
