/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include "JSDocumentFragment.h"

#include "ExceptionCode.h"
#include "JSNodeOrString.h"
#include "JSShadowRoot.h"

using namespace JSC;

namespace WebCore {

JSValue JSDocumentFragment::prepend(ExecState& state)
{
    ExceptionCode ec = 0;
    wrapped().prepend(toNodeOrStringVector(state), ec);
    setDOMException(&state, ec);

    return jsUndefined();
}

JSValue JSDocumentFragment::append(ExecState& state)
{
    ExceptionCode ec = 0;
    wrapped().append(toNodeOrStringVector(state), ec);
    setDOMException(&state, ec);

    return jsUndefined();
}

JSValue toJSNewlyCreated(ExecState*, JSDOMGlobalObject* globalObject, Ref<DocumentFragment>&& impl)
{
    if (impl->isShadowRoot())
        return createWrapper<ShadowRoot>(globalObject, WTFMove(impl));
    return createWrapper<DocumentFragment>(globalObject, WTFMove(impl));
}

JSValue toJS(ExecState* state, JSDOMGlobalObject* globalObject, DocumentFragment& impl)
{
    return wrap(state, globalObject, impl);
}

} // namespace WebCore
