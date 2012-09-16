#include "node-mruby.h"

using namespace v8;
using namespace node;

Handle<Value> NodeMRubyMethod::Call(const Arguments& args) {
    HandleScope scope;

    NodeMRubyMethod* myself = Unwrap<NodeMRubyMethod>(args.This());
    mrb_value * argv = new mrb_value[args.Length()];
    mrb_state* mrb = myself->mrb();
    for (int i=0; i<args.Length(); i++) {
        int ai = mrb_gc_arena_save(mrb);

        argv[i] = jsobj2ruby(mrb, args[i]);

        mrb_gc_arena_restore(mrb, ai);
    }
    String::Utf8Value u8meth(myself->method_name_);
    mrb_sym mid = mrb_intern(mrb, *u8meth);
    mrb_value* self = Unwrap<NodeMRubyObject>(myself->self_)->value();
    mrb_value rbretval = mrb_funcall_argv(mrb, *self, mid, args.Length(), argv);
    delete []argv;

    Handle<Value> jsretval = mrubyobj2js(mrb, rbretval);
    return scope.Close(jsretval);
}

mrb_state* NodeMRubyMethod::mrb() {
    return Unwrap<NodeMRubyObject>(this->self_)->mrb();
}

