/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ConsoleClient_h
#define ConsoleClient_h

#include <JSExportMacros.h>
#include "ConsoleTypes.h"
#include <wtf/Forward.h>

namespace Inspector {
class ScriptArguments;
}

namespace JSC {

class ExecState;

class ConsoleClient {
public:
    virtual ~ConsoleClient() { }

    JS_EXPORT_PRIVATE static void printConsoleMessage(MessageSource, MessageType, MessageLevel, const String& message, const String& url, unsigned lineNumber, unsigned columnNumber);
    JS_EXPORT_PRIVATE static void printConsoleMessageWithArguments(MessageSource, MessageType, MessageLevel, JSC::ExecState*, RefPtr<Inspector::ScriptArguments>&&);

    void logWithLevel(ExecState*, RefPtr<Inspector::ScriptArguments>&&, MessageLevel);
    void clear(ExecState*);
    void dir(ExecState*, RefPtr<Inspector::ScriptArguments>&&);
    void dirXML(ExecState*, RefPtr<Inspector::ScriptArguments>&&);
    void table(ExecState*, RefPtr<Inspector::ScriptArguments>&&);
    void trace(ExecState*, RefPtr<Inspector::ScriptArguments>&&);
    void assertion(ExecState*, RefPtr<Inspector::ScriptArguments>&&);
    void group(ExecState*, RefPtr<Inspector::ScriptArguments>&&);
    void groupCollapsed(ExecState*, RefPtr<Inspector::ScriptArguments>&&);
    void groupEnd(ExecState*, RefPtr<Inspector::ScriptArguments>&&);

    virtual void messageWithTypeAndLevel(MessageType, MessageLevel, JSC::ExecState*, RefPtr<Inspector::ScriptArguments>&&) = 0;
    virtual void count(ExecState*, RefPtr<Inspector::ScriptArguments>&&) = 0;
    virtual void profile(ExecState*, const String& title) = 0;
    virtual void profileEnd(ExecState*, const String& title) = 0;
    virtual void takeHeapSnapshot(ExecState*, const String& title) = 0;
    virtual void time(ExecState*, const String& title) = 0;
    virtual void timeEnd(ExecState*, const String& title) = 0;
    virtual void timeStamp(ExecState*, RefPtr<Inspector::ScriptArguments>&&) = 0;

private:
    enum ArgumentRequirement { ArgumentRequired, ArgumentNotRequired };
    void internalMessageWithTypeAndLevel(MessageType, MessageLevel, JSC::ExecState*, RefPtr<Inspector::ScriptArguments>&&, ArgumentRequirement);
};

} // namespace JSC

#endif // ConsoleClient_h
