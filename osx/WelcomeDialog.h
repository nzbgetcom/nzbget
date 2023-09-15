/*
 *  This file is part of nzbget. See <http://nzbget.com>.
 *
 *  Copyright (C) 2007-2016 Andrey Prygunkov <hugbug@users.sourceforge.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#import <Cocoa/Cocoa.h>

@interface WelcomeDialog : NSWindowController {
	IBOutlet NSButton *_okButton;
	IBOutlet NSButton *_dontShowAgainButton;
	IBOutlet NSImageView *_badgeView;
	IBOutlet NSImageView * _imageView;
	IBOutlet NSScrollView * _messageTextScrollView;
	IBOutlet NSTextView * _messageText;
    id _mainDelegate;
}

- (void)setMainDelegate:(id)mainDelegate;

- (IBAction)okButtonPressed:(id)sender;

- (void)showDialog;

@end
