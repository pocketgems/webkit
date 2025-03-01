/*
 * Copyright (C) 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import <Foundation/Foundation.h>

@class WebEvent;

// Listing of methods that are currently thread-safe.

// FIXME: It's not OK to have these additional definitions of all these classes,
// which can get out of sync with the originals, and also unclear whether there is
// any reason we need them any more. Should use the real headers instead.

@interface WAKView : NSObject
@end

@interface WebView : WAKView
- (void)reload:(id)sender;
- (void)stopLoading:(id)sender;
- (BOOL)canGoBack;
- (void)goBack;
- (BOOL)canGoForward;
- (void)goForward;
@end

@interface WebFrame : NSObject
- (void)loadRequest:(NSURLRequest *)request;
@end

@interface WAKResponder : NSObject
@end

@interface WAKWindow : WAKResponder
- (void)sendEvent:(WebEvent *)event;
@end

@protocol WebPolicyDecisionListener <NSObject>
- (void)use;
@end
