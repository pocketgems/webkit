/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#include "config.h"
#include "JSDOMPromise.h"

#include "ExceptionCode.h"
#include "JSDOMError.h"
#include <runtime/Exception.h>
#include <runtime/JSONObject.h>

using namespace JSC;

namespace WebCore {

DeferredWrapper::DeferredWrapper(ExecState*, JSDOMGlobalObject* globalObject, JSPromiseDeferred* promiseDeferred)
    : ActiveDOMCallback(globalObject->scriptExecutionContext())
    , m_deferred(promiseDeferred)
    , m_globalObject(globalObject)
{
    globalObject->vm().heap.writeBarrier(globalObject, promiseDeferred);
    m_globalObject->deferredWrappers().add(this);
}

DeferredWrapper::~DeferredWrapper()
{
    clear();
}

void DeferredWrapper::clear()
{
    ASSERT(!m_deferred || m_globalObject);
    if (m_deferred && m_globalObject)
        m_globalObject->deferredWrappers().remove(this);
    m_deferred.clear();
}

void DeferredWrapper::contextDestroyed()
{
    ActiveDOMCallback::contextDestroyed();
    clear();
}

JSC::JSValue DeferredWrapper::promise() const
{
    ASSERT(m_deferred);
    return m_deferred->promise();
}

void DeferredWrapper::callFunction(ExecState& exec, JSValue function, JSValue resolution)
{
    if (!canInvokeCallback())
        return;

    CallData callData;
    CallType callType = getCallData(function, callData);
    ASSERT(callType != CallType::None);

    MarkedArgumentBuffer arguments;
    arguments.append(resolution);

    call(&exec, function, callType, callData, jsUndefined(), arguments);

    clear();
}

void DeferredWrapper::reject(ExceptionCode ec, const String& message)
{
    if (isSuspended())
        return;

    ASSERT(m_deferred);
    ASSERT(m_globalObject);
    JSC::ExecState* state = m_globalObject->globalExec();
    JSC::JSLockHolder locker(state);
    reject(*state, createDOMException(state, ec, message));
}

void rejectPromiseWithExceptionIfAny(JSC::ExecState& state, JSDOMGlobalObject& globalObject, JSPromiseDeferred& promiseDeferred)
{
    VM& vm = state.vm();
    auto scope = DECLARE_CATCH_SCOPE(vm);

    if (!scope.exception())
        return;

    JSValue error = scope.exception()->value();
    scope.clearException();

    DeferredWrapper::create(&state, &globalObject, &promiseDeferred)->reject(error);
}

static inline JSC::JSValue parseAsJSON(JSC::ExecState* state, const String& data)
{
    JSC::JSLockHolder lock(state);
    return JSC::JSONParse(state, data);
}

void fulfillPromiseWithJSON(Ref<DeferredWrapper>&& promise, const String& data)
{
    JSC::JSValue value = parseAsJSON(promise->globalObject()->globalExec(), data);
    if (!value)
        promise->reject(SYNTAX_ERR);
    else
        promise->resolve(value);
}

void fulfillPromiseWithArrayBuffer(Ref<DeferredWrapper>&& promise, ArrayBuffer* arrayBuffer)
{
    if (!arrayBuffer) {
        promise->reject<JSValue>(createOutOfMemoryError(promise->globalObject()->globalExec()));
        return;
    }
    promise->resolve(arrayBuffer);
}

void fulfillPromiseWithArrayBuffer(Ref<DeferredWrapper>&& promise, const void* data, size_t length)
{
    fulfillPromiseWithArrayBuffer(WTFMove(promise), ArrayBuffer::tryCreate(data, length).get());
}

}
