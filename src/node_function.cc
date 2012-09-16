#include "node-mruby.h"

/*
 * This file provides NodeJS::Function extension.
 */

// function#call
static mrb_value node_function_call(mrb_state *mrb, mrb_value self) {
    HandleScope scope;

    int alen;
    mrb_value *a;

    DBG("FUNCTION CALL");
    mrb_get_args(mrb, "*", &a, &alen);
    Handle<Object> jsobj = ((NodeMRubyValueContainer*)mrb_get_datatype(mrb, self, &node_mruby_function_data_type))->v_->ToObject();

    Handle<Value> *args = new Handle<Value>[alen];
    for (int i=0; i<alen; i++) {
        args[i] = mrubyobj2js(mrb, a[i]);
    }
    Local<Value> retval = Function::Cast(*jsobj)->Call(jsobj, alen, args);
    delete []args;

    scope.Close(Undefined());

    return jsobj2ruby(mrb, retval);
}

struct RClass * nm_inject_nodejs_function(mrb_state *mrb, struct RClass * node_object_class) {
      struct RClass *func = mrb_define_class(mrb, "NodeJS::Function", node_object_class);
      mrb_define_method(mrb, func, "call", node_function_call, ARGS_ANY());
      return func;
}

