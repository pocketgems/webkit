/*
 *  Copyright (C) 1999-2002 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003-2009, 2015-2016 Apple Inc. All rights reserved.
 *  Copyright (C) 2007 Cameron Zwarich (cwzwarich@uwaterloo.ca)
 *  Copyright (C) 2007 Maks Orlovich
 *  Copyright (C) 2015 Canon Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "JSFunction.h"

#include "ClonedArguments.h"
#include "CodeBlock.h"
#include "CommonIdentifiers.h"
#include "CallFrame.h"
#include "ExceptionHelpers.h"
#include "FunctionPrototype.h"
#include "GeneratorPrototype.h"
#include "GetterSetter.h"
#include "JSArray.h"
#include "JSBoundFunction.h"
#include "JSCInlines.h"
#include "JSFunctionInlines.h"
#include "JSGlobalObject.h"
#include "Interpreter.h"
#include "ObjectConstructor.h"
#include "ObjectPrototype.h"
#include "Parser.h"
#include "PropertyNameArray.h"
#include "StackVisitor.h"

namespace JSC {

EncodedJSValue JSC_HOST_CALL callHostFunctionAsConstructor(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    return throwVMError(exec, scope, createNotAConstructorError(exec, exec->callee()));
}

const ClassInfo JSFunction::s_info = { "Function", &Base::s_info, 0, CREATE_METHOD_TABLE(JSFunction) };

bool JSFunction::isHostFunctionNonInline() const
{
    return isHostFunction();
}

JSFunction* JSFunction::create(VM& vm, FunctionExecutable* executable, JSScope* scope)
{
    return create(vm, executable, scope, scope->globalObject(vm)->functionStructure());
}

JSFunction* JSFunction::create(VM& vm, FunctionExecutable* executable, JSScope* scope, Structure* structure)
{
    JSFunction* result = createImpl(vm, executable, scope, structure);
    executable->singletonFunction()->notifyWrite(vm, result, "Allocating a function");
    return result;
}

#if ENABLE(WEBASSEMBLY)
JSFunction* JSFunction::create(VM& vm, WebAssemblyExecutable* executable, JSScope* scope)
{
    JSFunction* function = new (NotNull, allocateCell<JSFunction>(vm.heap)) JSFunction(vm, executable, scope);
    ASSERT(function->structure(vm)->globalObject());
    function->finishCreation(vm);
    return function;
}
#endif

JSFunction* JSFunction::create(VM& vm, JSGlobalObject* globalObject, int length, const String& name, NativeFunction nativeFunction, Intrinsic intrinsic, NativeFunction nativeConstructor)
{
    NativeExecutable* executable = vm.getHostFunction(nativeFunction, intrinsic, nativeConstructor, name);
    JSFunction* function = new (NotNull, allocateCell<JSFunction>(vm.heap)) JSFunction(vm, globalObject, globalObject->functionStructure());
    // Can't do this during initialization because getHostFunction might do a GC allocation.
    function->finishCreation(vm, executable, length, name);
    return function;
}

JSFunction::JSFunction(VM& vm, JSGlobalObject* globalObject, Structure* structure)
    : Base(vm, globalObject, structure)
    , m_executable()
{
}

void JSFunction::finishCreation(VM& vm, NativeExecutable* executable, int length, const String& name)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
    m_executable.set(vm, this, executable);
    // Some NativeExecutable functions, like JSBoundFunction, decide to lazily allocate their name string.
    if (!name.isNull())
        putDirect(vm, vm.propertyNames->name, jsString(&vm, name), ReadOnly | DontEnum);
    putDirect(vm, vm.propertyNames->length, jsNumber(length), ReadOnly | DontEnum);
}

JSFunction* JSFunction::createBuiltinFunction(VM& vm, FunctionExecutable* executable, JSGlobalObject* globalObject)
{
    JSFunction* function = create(vm, executable, globalObject);
    function->putDirect(vm, vm.propertyNames->name, jsString(&vm, executable->name().string()), ReadOnly | DontEnum);
    function->putDirect(vm, vm.propertyNames->length, jsNumber(executable->parameterCount()), ReadOnly | DontEnum);
    return function;
}

JSFunction* JSFunction::createBuiltinFunction(VM& vm, FunctionExecutable* executable, JSGlobalObject* globalObject, const String& name)
{
    JSFunction* function = create(vm, executable, globalObject);
    function->putDirect(vm, vm.propertyNames->name, jsString(&vm, name), ReadOnly | DontEnum);
    function->putDirect(vm, vm.propertyNames->length, jsNumber(executable->parameterCount()), ReadOnly | DontEnum);
    return function;
}

FunctionRareData* JSFunction::allocateRareData(VM& vm)
{
    ASSERT(!m_rareData);
    FunctionRareData* rareData = FunctionRareData::create(vm);

    // A DFG compilation thread may be trying to read the rare data
    // We want to ensure that it sees it properly allocated
    WTF::storeStoreFence();

    m_rareData.set(vm, this, rareData);
    return m_rareData.get();
}

FunctionRareData* JSFunction::allocateAndInitializeRareData(ExecState* exec, size_t inlineCapacity)
{
    ASSERT(!m_rareData);
    VM& vm = exec->vm();
    JSObject* prototype = jsDynamicCast<JSObject*>(get(exec, vm.propertyNames->prototype));
    if (!prototype)
        prototype = globalObject(vm)->objectPrototype();
    FunctionRareData* rareData = FunctionRareData::create(vm);
    rareData->initializeObjectAllocationProfile(vm, prototype, inlineCapacity);

    // A DFG compilation thread may be trying to read the rare data
    // We want to ensure that it sees it properly allocated
    WTF::storeStoreFence();

    m_rareData.set(vm, this, rareData);
    return m_rareData.get();
}

FunctionRareData* JSFunction::initializeRareData(ExecState* exec, size_t inlineCapacity)
{
    ASSERT(!!m_rareData);
    VM& vm = exec->vm();
    JSObject* prototype = jsDynamicCast<JSObject*>(get(exec, vm.propertyNames->prototype));
    if (!prototype)
        prototype = globalObject(vm)->objectPrototype();
    m_rareData->initializeObjectAllocationProfile(vm, prototype, inlineCapacity);
    return m_rareData.get();
}

String JSFunction::name(VM& vm)
{
    if (isHostFunction()) {
        NativeExecutable* executable = jsCast<NativeExecutable*>(this->executable());
        return executable->name();
    }
    const Identifier identifier = jsExecutable()->name();
    if (identifier == vm.propertyNames->builtinNames().starDefaultPrivateName())
        return emptyString();
    return identifier.string();
}

String JSFunction::displayName(VM& vm)
{
    JSValue displayName = getDirect(vm, vm.propertyNames->displayName);
    
    if (displayName && isJSString(displayName))
        return asString(displayName)->tryGetValue();
    
    return String();
}

const String JSFunction::calculatedDisplayName(VM& vm)
{
    const String explicitName = displayName(vm);
    
    if (!explicitName.isEmpty())
        return explicitName;
    
    const String actualName = name(vm);
    if (!actualName.isEmpty() || isHostOrBuiltinFunction())
        return actualName;
    
    return jsExecutable()->inferredName().string();
}

const SourceCode* JSFunction::sourceCode() const
{
    if (isHostOrBuiltinFunction())
        return 0;
    return &jsExecutable()->source();
}
    
void JSFunction::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    JSFunction* thisObject = jsCast<JSFunction*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);

    visitor.append(&thisObject->m_executable);
    if (thisObject->m_rareData)
        visitor.append(&thisObject->m_rareData);
}

CallType JSFunction::getCallData(JSCell* cell, CallData& callData)
{
    JSFunction* thisObject = jsCast<JSFunction*>(cell);
    if (thisObject->isHostFunction()) {
        callData.native.function = thisObject->nativeFunction();
        return CallType::Host;
    }
    callData.js.functionExecutable = thisObject->jsExecutable();
    callData.js.scope = thisObject->scope();
    return CallType::JS;
}

class RetrieveArgumentsFunctor {
public:
    RetrieveArgumentsFunctor(JSFunction* functionObj)
        : m_targetCallee(jsDynamicCast<JSObject*>(functionObj))
        , m_result(jsNull())
    {
    }

    JSValue result() const { return m_result; }

    StackVisitor::Status operator()(StackVisitor& visitor) const
    {
        JSObject* callee = visitor->callee();
        if (callee != m_targetCallee)
            return StackVisitor::Continue;

        m_result = JSValue(visitor->createArguments());
        return StackVisitor::Done;
    }

private:
    JSObject* m_targetCallee;
    mutable JSValue m_result;
};

static JSValue retrieveArguments(ExecState* exec, JSFunction* functionObj)
{
    RetrieveArgumentsFunctor functor(functionObj);
    exec->iterate(functor);
    return functor.result();
}

EncodedJSValue JSFunction::argumentsGetter(ExecState* exec, EncodedJSValue thisValue, PropertyName)
{
    JSFunction* thisObj = jsCast<JSFunction*>(JSValue::decode(thisValue));
    ASSERT(!thisObj->isHostFunction());

    return JSValue::encode(retrieveArguments(exec, thisObj));
}

class RetrieveCallerFunctionFunctor {
public:
    RetrieveCallerFunctionFunctor(JSFunction* functionObj)
        : m_targetCallee(jsDynamicCast<JSObject*>(functionObj))
        , m_hasFoundFrame(false)
        , m_hasSkippedToCallerFrame(false)
        , m_result(jsNull())
    {
    }

    JSValue result() const { return m_result; }

    StackVisitor::Status operator()(StackVisitor& visitor) const
    {
        JSObject* callee = visitor->callee();

        if (callee && callee->inherits(JSBoundFunction::info()))
            return StackVisitor::Continue;

        if (!m_hasFoundFrame && (callee != m_targetCallee))
            return StackVisitor::Continue;

        m_hasFoundFrame = true;
        if (!m_hasSkippedToCallerFrame) {
            m_hasSkippedToCallerFrame = true;
            return StackVisitor::Continue;
        }

        if (callee)
            m_result = callee;
        return StackVisitor::Done;
    }

private:
    JSObject* m_targetCallee;
    mutable bool m_hasFoundFrame;
    mutable bool m_hasSkippedToCallerFrame;
    mutable JSValue m_result;
};

static JSValue retrieveCallerFunction(ExecState* exec, JSFunction* functionObj)
{
    RetrieveCallerFunctionFunctor functor(functionObj);
    exec->iterate(functor);
    return functor.result();
}

EncodedJSValue JSFunction::callerGetter(ExecState* exec, EncodedJSValue thisValue, PropertyName)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSFunction* thisObj = jsCast<JSFunction*>(JSValue::decode(thisValue));
    ASSERT(!thisObj->isHostFunction());
    JSValue caller = retrieveCallerFunction(exec, thisObj);

    // See ES5.1 15.3.5.4 - Function.caller may not be used to retrieve a strict caller.
    if (!caller.isObject() || !asObject(caller)->inherits(JSFunction::info())) {
        // It isn't a JSFunction, but if it is a JSCallee from a program or call eval, return null.
        if (jsDynamicCast<JSCallee*>(caller))
            return JSValue::encode(jsNull());
        return JSValue::encode(caller);
    }
    JSFunction* function = jsCast<JSFunction*>(caller);
    if (function->isHostOrBuiltinFunction() || !function->jsExecutable()->isStrictMode())
        return JSValue::encode(caller);
    return JSValue::encode(throwTypeError(exec, scope, ASCIILiteral("Function.caller used to retrieve strict caller")));
}

bool JSFunction::getOwnPropertySlot(JSObject* object, ExecState* exec, PropertyName propertyName, PropertySlot& slot)
{
    VM& vm = exec->vm();
    JSFunction* thisObject = jsCast<JSFunction*>(object);
    if (thisObject->isHostOrBuiltinFunction()) {
        thisObject->reifyBoundNameIfNeeded(vm, exec, propertyName);
        return Base::getOwnPropertySlot(thisObject, exec, propertyName, slot);
    }

    if (propertyName == vm.propertyNames->prototype && !thisObject->jsExecutable()->isArrowFunction()) {
        unsigned attributes;
        PropertyOffset offset = thisObject->getDirectOffset(vm, propertyName, attributes);
        if (!isValidOffset(offset)) {
            JSObject* prototype = nullptr;
            if (thisObject->jsExecutable()->parseMode() == SourceParseMode::GeneratorWrapperFunctionMode)
                prototype = constructEmptyObject(exec, thisObject->globalObject(vm)->generatorPrototype());
            else
                prototype = constructEmptyObject(exec);

            prototype->putDirect(vm, vm.propertyNames->constructor, thisObject, DontEnum);
            thisObject->putDirect(vm, vm.propertyNames->prototype, prototype, DontDelete | DontEnum);
            offset = thisObject->getDirectOffset(vm, vm.propertyNames->prototype, attributes);
            ASSERT(isValidOffset(offset));
        }

        slot.setValue(thisObject, attributes, thisObject->getDirect(offset), offset);
    }

    if (propertyName == vm.propertyNames->arguments) {
        if (thisObject->jsExecutable()->isStrictMode() || thisObject->jsExecutable()->isClassConstructorFunction()) {
            bool result = Base::getOwnPropertySlot(thisObject, exec, propertyName, slot);
            if (!result) {
                GetterSetter* errorGetterSetter = thisObject->globalObject(vm)->throwTypeErrorArgumentsCalleeAndCallerGetterSetter();
                thisObject->putDirectAccessor(exec, propertyName, errorGetterSetter, DontDelete | DontEnum | Accessor);
                result = Base::getOwnPropertySlot(thisObject, exec, propertyName, slot);
                ASSERT(result);
            }
            return result;
        }
        slot.setCacheableCustom(thisObject, ReadOnly | DontEnum | DontDelete, argumentsGetter);
        return true;
    }

    if (propertyName == vm.propertyNames->caller) {
        if (thisObject->jsExecutable()->isStrictMode() || thisObject->jsExecutable()->isClassConstructorFunction()) {
            bool result = Base::getOwnPropertySlot(thisObject, exec, propertyName, slot);
            if (!result) {
                GetterSetter* errorGetterSetter = thisObject->globalObject(vm)->throwTypeErrorArgumentsCalleeAndCallerGetterSetter();
                thisObject->putDirectAccessor(exec, propertyName, errorGetterSetter, DontDelete | DontEnum | Accessor);
                result = Base::getOwnPropertySlot(thisObject, exec, propertyName, slot);
                ASSERT(result);
            }
            return result;
        }
        slot.setCacheableCustom(thisObject, ReadOnly | DontEnum | DontDelete, callerGetter);
        return true;
    }

    thisObject->reifyLazyPropertyIfNeeded(vm, exec, propertyName);

    return Base::getOwnPropertySlot(thisObject, exec, propertyName, slot);
}

void JSFunction::getOwnNonIndexPropertyNames(JSObject* object, ExecState* exec, PropertyNameArray& propertyNames, EnumerationMode mode)
{
    JSFunction* thisObject = jsCast<JSFunction*>(object);
    if (!thisObject->isHostOrBuiltinFunction() && mode.includeDontEnumProperties()) {
        VM& vm = exec->vm();
        // Make sure prototype has been reified.
        PropertySlot slot(thisObject, PropertySlot::InternalMethodType::VMInquiry);
        thisObject->methodTable(vm)->getOwnPropertySlot(thisObject, exec, vm.propertyNames->prototype, slot);

        propertyNames.add(vm.propertyNames->arguments);
        propertyNames.add(vm.propertyNames->caller);
        if (!thisObject->hasReifiedLength())
            propertyNames.add(vm.propertyNames->length);
        if (!thisObject->hasReifiedName())
            propertyNames.add(vm.propertyNames->name);
    } else if (thisObject->isHostOrBuiltinFunction() && mode.includeDontEnumProperties() && thisObject->inherits(JSBoundFunction::info()) && !thisObject->hasReifiedName())
        propertyNames.add(exec->vm().propertyNames->name);
    Base::getOwnNonIndexPropertyNames(thisObject, exec, propertyNames, mode);
}

bool JSFunction::put(JSCell* cell, ExecState* exec, PropertyName propertyName, JSValue value, PutPropertySlot& slot)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSFunction* thisObject = jsCast<JSFunction*>(cell);

    if (UNLIKELY(isThisValueAltered(slot, thisObject)))
        return ordinarySetSlow(exec, thisObject, propertyName, value, slot.thisValue(), slot.isStrictMode());

    if (thisObject->isHostOrBuiltinFunction()) {
        thisObject->reifyBoundNameIfNeeded(vm, exec, propertyName);
        return Base::put(thisObject, exec, propertyName, value, slot);
    }

    if (propertyName == vm.propertyNames->prototype) {
        // Make sure prototype has been reified, such that it can only be overwritten
        // following the rules set out in ECMA-262 8.12.9.
        PropertySlot slot(thisObject, PropertySlot::InternalMethodType::VMInquiry);
        thisObject->methodTable(vm)->getOwnPropertySlot(thisObject, exec, propertyName, slot);
        if (thisObject->m_rareData)
            thisObject->m_rareData->clear("Store to prototype property of a function");
        // Don't allow this to be cached, since a [[Put]] must clear m_rareData.
        PutPropertySlot dontCache(thisObject);
        scope.release();
        return Base::put(thisObject, exec, propertyName, value, dontCache);
    }
    if (thisObject->jsExecutable()->isStrictMode() && (propertyName == vm.propertyNames->arguments || propertyName == vm.propertyNames->caller)) {
        // This will trigger the property to be reified, if this is not already the case!
        bool okay = thisObject->hasProperty(exec, propertyName);
        ASSERT_UNUSED(okay, okay);
        scope.release();
        return Base::put(thisObject, exec, propertyName, value, slot);
    }
    if (propertyName == vm.propertyNames->arguments || propertyName == vm.propertyNames->caller) {
        if (slot.isStrictMode())
            throwTypeError(exec, scope, StrictModeReadonlyPropertyWriteError);
        return false;
    }
    thisObject->reifyLazyPropertyIfNeeded(vm, exec, propertyName);
    scope.release();
    return Base::put(thisObject, exec, propertyName, value, slot);
}

bool JSFunction::deleteProperty(JSCell* cell, ExecState* exec, PropertyName propertyName)
{
    JSFunction* thisObject = jsCast<JSFunction*>(cell);
    if (thisObject->isHostOrBuiltinFunction())
        thisObject->reifyBoundNameIfNeeded(exec->vm(), exec, propertyName);
    else if (exec->vm().deletePropertyMode() != VM::DeletePropertyMode::IgnoreConfigurable) {
        // For non-host functions, don't let these properties by deleted - except by DefineOwnProperty.
        VM& vm = exec->vm();
        FunctionExecutable* executable = thisObject->jsExecutable();
        if (propertyName == vm.propertyNames->arguments
            || (propertyName == vm.propertyNames->prototype && !executable->isArrowFunction())
            || propertyName == vm.propertyNames->caller)
            return false;

        thisObject->reifyLazyPropertyIfNeeded(vm, exec, propertyName);
    }
    
    return Base::deleteProperty(thisObject, exec, propertyName);
}

bool JSFunction::defineOwnProperty(JSObject* object, ExecState* exec, PropertyName propertyName, const PropertyDescriptor& descriptor, bool throwException)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSFunction* thisObject = jsCast<JSFunction*>(object);
    if (thisObject->isHostOrBuiltinFunction()) {
        thisObject->reifyBoundNameIfNeeded(vm, exec, propertyName);
        return Base::defineOwnProperty(object, exec, propertyName, descriptor, throwException);
    }

    if (propertyName == vm.propertyNames->prototype) {
        // Make sure prototype has been reified, such that it can only be overwritten
        // following the rules set out in ECMA-262 8.12.9.
        PropertySlot slot(thisObject, PropertySlot::InternalMethodType::VMInquiry);
        thisObject->methodTable(vm)->getOwnPropertySlot(thisObject, exec, propertyName, slot);
        if (thisObject->m_rareData)
            thisObject->m_rareData->clear("Store to prototype property of a function");
        return Base::defineOwnProperty(object, exec, propertyName, descriptor, throwException);
    }

    bool valueCheck;
    if (propertyName == vm.propertyNames->arguments) {
        if (thisObject->jsExecutable()->isStrictMode()) {
            PropertySlot slot(thisObject, PropertySlot::InternalMethodType::VMInquiry);
            if (!Base::getOwnPropertySlot(thisObject, exec, propertyName, slot))
                thisObject->putDirectAccessor(exec, propertyName, thisObject->globalObject(vm)->throwTypeErrorArgumentsCalleeAndCallerGetterSetter(), DontDelete | DontEnum | Accessor);
            return Base::defineOwnProperty(object, exec, propertyName, descriptor, throwException);
        }
        valueCheck = !descriptor.value() || sameValue(exec, descriptor.value(), retrieveArguments(exec, thisObject));
    } else if (propertyName == vm.propertyNames->caller) {
        if (thisObject->jsExecutable()->isStrictMode()) {
            PropertySlot slot(thisObject, PropertySlot::InternalMethodType::VMInquiry);
            if (!Base::getOwnPropertySlot(thisObject, exec, propertyName, slot))
                thisObject->putDirectAccessor(exec, propertyName, thisObject->globalObject(vm)->throwTypeErrorArgumentsCalleeAndCallerGetterSetter(), DontDelete | DontEnum | Accessor);
            return Base::defineOwnProperty(object, exec, propertyName, descriptor, throwException);
        }
        valueCheck = !descriptor.value() || sameValue(exec, descriptor.value(), retrieveCallerFunction(exec, thisObject));
    } else {
        thisObject->reifyLazyPropertyIfNeeded(vm, exec, propertyName);
        return Base::defineOwnProperty(object, exec, propertyName, descriptor, throwException);
    }
     
    if (descriptor.configurablePresent() && descriptor.configurable()) {
        if (throwException)
            throwTypeError(exec, scope, ASCIILiteral("Attempting to change configurable attribute of unconfigurable property."));
        return false;
    }
    if (descriptor.enumerablePresent() && descriptor.enumerable()) {
        if (throwException)
            throwTypeError(exec, scope, ASCIILiteral("Attempting to change enumerable attribute of unconfigurable property."));
        return false;
    }
    if (descriptor.isAccessorDescriptor()) {
        if (throwException)
            throwTypeError(exec, scope, ASCIILiteral(UnconfigurablePropertyChangeAccessMechanismError));
        return false;
    }
    if (descriptor.writablePresent() && descriptor.writable()) {
        if (throwException)
            throwTypeError(exec, scope, ASCIILiteral("Attempting to change writable attribute of unconfigurable property."));
        return false;
    }
    if (!valueCheck) {
        if (throwException)
            throwTypeError(exec, scope, ASCIILiteral("Attempting to change value of a readonly property."));
        return false;
    }
    return true;
}

// ECMA 13.2.2 [[Construct]]
ConstructType JSFunction::getConstructData(JSCell* cell, ConstructData& constructData)
{
    JSFunction* thisObject = jsCast<JSFunction*>(cell);

    if (thisObject->isHostFunction()) {
        constructData.native.function = thisObject->nativeConstructor();
        return ConstructType::Host;
    }

    FunctionExecutable* functionExecutable = thisObject->jsExecutable();
    if (functionExecutable->constructAbility() == ConstructAbility::CannotConstruct)
        return ConstructType::None;

    constructData.js.functionExecutable = functionExecutable;
    constructData.js.scope = thisObject->scope();
    return ConstructType::JS;
}

String getCalculatedDisplayName(VM& vm, JSObject* object)
{
    if (JSFunction* function = jsDynamicCast<JSFunction*>(object))
        return function->calculatedDisplayName(vm);
    if (InternalFunction* function = jsDynamicCast<InternalFunction*>(object))
        return function->calculatedDisplayName(vm);
    return emptyString();
}

void JSFunction::setFunctionName(ExecState* exec, JSValue value)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // The "name" property may have been already been defined as part of a property list in an
    // object literal (and therefore reified).
    if (hasReifiedName())
        return;

    ASSERT(!isHostFunction());
    ASSERT(jsExecutable()->ecmaName().isNull());
    String name;
    if (value.isSymbol()) {
        SymbolImpl& uid = asSymbol(value)->privateName().uid();
        if (uid.isNullSymbol())
            name = emptyString();
        else
            name = makeString('[', String(&uid), ']');
    } else {
        JSString* jsStr = value.toString(exec);
        if (UNLIKELY(scope.exception()))
            return;
        name = jsStr->value(exec);
        if (UNLIKELY(scope.exception()))
            return;
    }
    reifyName(vm, exec, name);
}

void JSFunction::reifyLength(VM& vm)
{
    FunctionRareData* rareData = this->rareData(vm);

    ASSERT(!hasReifiedLength());
    ASSERT(!isHostFunction());
    JSValue initialValue = jsNumber(jsExecutable()->parameterCount());
    unsigned initialAttributes = DontEnum | ReadOnly;
    const Identifier& identifier = vm.propertyNames->length;
    putDirect(vm, identifier, initialValue, initialAttributes);

    rareData->setHasReifiedLength();
}

void JSFunction::reifyName(VM& vm, ExecState* exec)
{
    const Identifier& ecmaName = jsExecutable()->ecmaName();
    String name;
    // https://tc39.github.io/ecma262/#sec-exports-runtime-semantics-evaluation
    // When the ident is "*default*", we need to set "default" for the ecma name.
    // This "*default*" name is never shown to users.
    if (ecmaName == exec->propertyNames().builtinNames().starDefaultPrivateName())
        name = exec->propertyNames().defaultKeyword.string();
    else
        name = ecmaName.string();
    reifyName(vm, exec, name);
}

void JSFunction::reifyName(VM& vm, ExecState* exec, String name)
{
    FunctionRareData* rareData = this->rareData(vm);

    ASSERT(!hasReifiedName());
    ASSERT(!isHostFunction());
    unsigned initialAttributes = DontEnum | ReadOnly;
    const Identifier& propID = vm.propertyNames->name;

    if (exec->lexicalGlobalObject()->needsSiteSpecificQuirks()) {
        auto illegalCharMatcher = [] (UChar ch) -> bool {
            return ch == ' ' || ch == '|';
        };
        if (name.find(illegalCharMatcher) != notFound)
            name = String();
    }
    
    if (jsExecutable()->isGetter())
        name = makeString("get ", name);
    else if (jsExecutable()->isSetter())
        name = makeString("set ", name);

    putDirect(vm, propID, jsString(exec, name), initialAttributes);
    rareData->setHasReifiedName();
}

void JSFunction::reifyLazyPropertyIfNeeded(VM& vm, ExecState* exec, PropertyName propertyName)
{
    if (propertyName == vm.propertyNames->length) {
        if (!hasReifiedLength())
            reifyLength(vm);
    } else if (propertyName == vm.propertyNames->name) {
        if (!hasReifiedName())
            reifyName(vm, exec);
    }
}

void JSFunction::reifyBoundNameIfNeeded(VM& vm, ExecState* exec, PropertyName propertyName)
{
    const Identifier& nameIdent = vm.propertyNames->name;
    if (propertyName != nameIdent)
        return;

    if (hasReifiedName())
        return;

    if (this->inherits(JSBoundFunction::info())) {
        FunctionRareData* rareData = this->rareData(vm);
        String name = makeString("bound ", static_cast<NativeExecutable*>(m_executable.get())->name());
        unsigned initialAttributes = DontEnum | ReadOnly;
        putDirect(vm, nameIdent, jsString(exec, name), initialAttributes);
        rareData->setHasReifiedName();
    }
}

} // namespace JSC
