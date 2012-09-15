#pragma once

#define BUILDING_NODE_EXTENSION
#include <node.h>
#include <node_buffer.h>
#include <v8.h>
#include <iostream>

/* ******************************************************
 * exception utilities
 */
#define THROW_TYPE_ERROR(str) \
    ThrowException(Exception::TypeError(String::New(str)))

/* ******************************************************
 * Argument utilities.
 */
#define ARG_EXT(I, VAR) \
    if (args.Length() <= (I) || !args[I]->IsExternal()) \
        return ThrowException(Exception::TypeError( \
            String::New("Argument " #I " must be an external"))); \
    Local<External> VAR = Local<External>::Cast(args[I]);

/**
 * ARG_STR(0, src);
 *
 * see http://blog.64p.org/entry/2012/09/02/101609
 */
#define ARG_STR(I, VAR) \
    if (args.Length() <= (I)) \
        return ThrowException(Exception::TypeError( \
            String::New("Argument " #I " must be a string"))); \
    String::Utf8Value VAR(args[I]->ToString());

#define ARG_STRR(I, VAR) \
    if (args.Length() <= (I)) \
        return ThrowException(Exception::TypeError( \
            String::New("Argument " #I " must be a string"))); \
    Local<String> VAR = args[I]->ToString();

#define ARG_OBJ(I, VAR) \
    if (args.Length() <= (I) || !args[I]->IsObject()) \
        return ThrowException(Exception::TypeError( \
            String::New("Argument " #I " must be a object"))); \
    Local<Object> VAR = Local<Object>::Cast(args[I]);

#define ARG_INT(I, VAR) \
    if (args.Length() <= (I) || !args[I]->IsInt32()) \
        return ThrowException(Exception::TypeError( \
            String::New("Argument " #I " must be an integer"))); \
    int32_t VAR = args[I]->Int32Value();

#define ARG_BUF(I, VAR) \
    if (args.Length() <= (I) || !Buffer::HasInstance(args[I])) \
        return ThrowException(Exception::TypeError( \
            String::New("Argument " #I " must be an Buffer"))); \
    void * VAR = Buffer::Data(args[I]->ToObject());

#define ARG_FUNC(I, VAR) \
    if (args.Length() <= (I) || !args[I]->IsFunction()) \
        return ThrowException(Exception::TypeError( \
              String::New("Argument " #I " must be a function"))); \
    Local<Function> VAR = Local<Function>::Cast(args[I]);

/* ******************************************************
 * Class construction utilities
 */
#define SET_ENUM_VALUE(target, _value) \
        target->Set(String::NewSymbol(#_value), \
                Integer::New(_value), \
                static_cast<PropertyAttribute>(ReadOnly|DontDelete))

#ifndef DEBUGGING
inline static void jsobjdump(v8::Handle<v8::Value> val, int n=0) {
    for (int i=0; i<n; i++) {
        std::cerr << " ";
    }
    if (n>32) {
        std::cerr << "too deep" << std::endl;
        return;
    }
    if (val->IsTrue()) {
        std::cerr << "true" << std::endl;
    } else if (val->IsFalse()) {
        std::cerr << "false" << std::endl;
    } else if (val->IsNull()) {
        std::cerr << "null" << std::endl;
    } else if (val->IsUndefined()) {
        std::cerr << "undefined" << std::endl;
    } else if (val->IsString()) {
        v8::String::Utf8Value u8val(val);
        std::cerr << *u8val << std::endl;
    } else if (val->IsArray()) {
        v8::Handle<v8::Array> jsav = v8::Handle<v8::Array>::Cast(val);
        for (size_t i=0; i<jsav->Length(); ++i) {
            jsobjdump(jsav->Get(i), n+1);
        }
    } else if (val->IsFunction()) {
        std::cerr << "[Function]" << std::endl;
    } else if (val->IsObject()) {
        v8::Handle<v8::Object> jsobj = v8::Handle<v8::Object>::Cast(val);
        v8::Handle<v8::Array> keys = jsobj->GetPropertyNames();
        std::cerr << "[Object" << std::endl;
        for (size_t i=0; i<keys->Length(); ++i) {
            jsobjdump(keys->Get(i), n+1);
            jsobjdump(jsobj->Get(keys->Get(i)), n+1);
        }
        std::cerr << "]" << std::endl;
    } else if (val->IsInt32()) {
        std::cerr << "[Int32 " << val->Int32Value() << "]" << std::endl;
    } else if (val->IsUint32()) {
        std::cerr << "[UInt32 " << val->Uint32Value() << "]" << std::endl;
    } else if (val->IsNumber()) {
        std::cerr << "[Number " << val->NumberValue() << "]" << std::endl;
    } else {
        std::cerr << "[Unknown]" << std::endl;
    }
}
#else
inline static void jsobjdump(v8::Handle<v8::Value> val) { }
#endif
