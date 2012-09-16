#include "node-mruby.h"

using namespace v8;
using namespace node;

static mrb_value node_eval(mrb_state *mrb, mrb_value self) {
    mrb_value str;
    mrb_get_args(mrb, "o", &str);
    mrb_value rstr = mrb_string_value(mrb, &str);
    Local<Value> arg0 = String::New(RSTRING_PTR(rstr), RSTRING_LEN(rstr));
    Handle<Value> args[] = {arg0};
    Local<v8::Object> jsself = Object::New();
    Handle<Value> retval = NodeMRuby::eval->Call(jsself, 1, args);
    if (*retval) {
        return jsobj2ruby(mrb, retval);
    } else {
        return mrb_nil_value();
    }
}

static mrb_value node_log(mrb_state *mrb, mrb_value self) {
    mrb_value str;
    mrb_get_args(mrb, "o", &str);
    mrb_value rstr = mrb_string_value(mrb, &str);
    Local<Value> arg0 = String::New(RSTRING_PTR(rstr), RSTRING_LEN(rstr));
    Handle<Value> args[] = {arg0};
    Local<v8::Object> jsself = Object::New();
    Handle<Value> retval = NodeMRuby::log->Call(jsself, 1, args);
    if (*retval) {
        return jsobj2ruby(mrb, retval);
    } else {
        return mrb_nil_value();
    }
}

static mrb_value node_require(mrb_state *mrb, mrb_value self) {
    mrb_value str;
    mrb_get_args(mrb, "o", &str);
    mrb_value rstr = mrb_string_value(mrb, &str);
    Local<Value> arg0 = String::New(RSTRING_PTR(rstr), RSTRING_LEN(rstr));
    Handle<Value> args[] = {arg0};
    Handle<v8::Object> jsself = Object::New();
    Handle<Value> retval = NodeMRuby::require->Call(jsself, 1, args);
    if (*retval) {
        mrb_value ret = jsobj2ruby(mrb, retval);
        mrb_p(mrb, ret);
        return ret;
    } else {
        mrb_raise(mrb, E_RUNTIME_ERROR, "Cannot load a library: %s", RSTRING_PTR(rstr));
        return mrb_nil_value();
    }
}

void nm_inject_nodejs_api_to_ruby_world(mrb_state* mrb) {
    RClass * s = mrb_define_class(mrb, "NodeJS", mrb->object_class);
    mrb_define_class_method(mrb, s, "require", node_require,      ARGS_REQ(1));
    mrb_define_class_method(mrb, s, "eval",    node_eval,         ARGS_REQ(1));
    mrb_define_class_method(mrb, s, "log",     node_log,          ARGS_REQ(1));
}

