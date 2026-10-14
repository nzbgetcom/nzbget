/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2007-2016 Andrey Prygunkov <hugbug@users.sourceforge.net>
 *  Copyright (C) 2026 Denis <denis@nzbget.net>
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
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#import <Cocoa/Cocoa.h>
#import "RPC.h"

@implementation RPC

NSString* rpcUrl;

- (id)initWithMethod:(NSString*)method
			receiver:(id)receiver
			 success:(SEL)successCallback
			 failure:(SEL)failureCallback {
	NSString* urlStr = [rpcUrl stringByAppendingString:method];
	self = [super initWithURLString:urlStr receiver:receiver success:successCallback failure:failureCallback];
	return self;
}

- (id)initWithMethod:(NSString*)method
			  params:(NSArray*)params
			receiver:(id)receiver
			 success:(SEL)successCallback
			 failure:(SEL)failureCallback {
	NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:[NSURL URLWithString:rpcUrl]];
	[request setHTTPMethod:@"POST"];
	[request setValue:@"application/json" forHTTPHeaderField:@"Content-Type"];
	
	NSDictionary *reqObj = @{ @"version": @"1.1", @"method": method, @"params": params ?: @[] };
	NSData *reqData = [NSJSONSerialization dataWithJSONObject:reqObj options:0 error:nil];
	[request setHTTPBody:reqData];
	
	self = [super initWithURLRequest:request receiver:receiver success:successCallback failure:failureCallback];
	return self;
}

+ (void)setRpcUrl:(NSString*)url {
	rpcUrl = url;
}

- (void)success {
    NSError *error = nil;
    id dataObj = [NSJSONSerialization
				  JSONObjectWithData:data
				  options:0
				  error:&error];
	
    if (error || ![dataObj isKindOfClass:[NSDictionary class]]) {
		/* JSON was malformed, act appropriately here */
		failureCode = 999;
		[self failure];
		return;
	}

	id result = [dataObj valueForKey:@"result"];
	SuppressPerformSelectorLeakWarning([_receiver performSelector:_successCallback withObject:result];);
}

@end
