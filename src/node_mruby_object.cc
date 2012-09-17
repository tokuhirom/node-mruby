#include "node-mruby.h"

Handle<Value> NodeMRubyObject::GetNamedProperty(Local<String> name,
        const AccessorInfo &info) {
    HandleScope scope;

    if (info.This()->InternalFieldCount() < 1 || info.Data().IsEmpty()) {
        return THROW_TYPE_ERROR("SetNamedProperty intercepted "
            "by non-Proxy object");
    }

    Handle<Value> arg0 = (Unwrap<NodeMRubyObject>(info.This())->nmrb_);
    Handle<Value> arg1(info.This());
    Handle<Value> arg2 = name;
    Handle<Value> args[] = {arg0, arg1, arg2};

    Handle<Value> method = NodeMRuby::method_func_generator->Call(info.This(), 3, args);
    return scope.Close(method);
}
