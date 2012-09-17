#include "node-mruby.h"

/**
 * NodeJS::Object class for ruby world.
 */

/// mruby symbol to string
static inline mrb_value mrb_sym_to_s(mrb_state *mrb, mrb_value sym) {
    mrb_sym id = SYM2ID(sym);
    int len;

    const char *p = mrb_sym2name_len(mrb, id, &len);
    return mrb_str_new(mrb, p, len);
}

/// NodeJS::Object#method_missing
static mrb_value node_object_method_missing(mrb_state *mrb, mrb_value self) {
    int alen;
    mrb_value name, *a, b;

    Persistent<Context> context = Context::New();
    Context::Scope context_scope(context);

    mrb_get_args(mrb, "o*&", &name, &a, &alen, &b);
    // std::cerr << "[DEBUG] object#method_missing with arguments: " << alen << " " << (!mrb_nil_p(b) ? "with" : "without") << " block" << std::endl;
    if (!SYMBOL_P(name)) {
        mrb_raise(mrb, E_TYPE_ERROR, "name should be a symbol");
    }

    Handle<Value> jsval = ((NodeMRubyValueContainer*)mrb_get_datatype(mrb, self, &node_mruby_function_data_type))->v_;
    assert(jsval->IsObject());
    Handle<Object> jsobj = jsval->ToObject();
    mrb_value rsName = mrb_sym_to_s(mrb, name);
    Handle<String> hsName = String::New(RSTRING_PTR(rsName), RSTRING_LEN(rsName));
    Handle<Value> elem = jsobj->Get(hsName);
    if (elem->IsFunction()) {
        // call javascript method
        int alen2 = alen + ( mrb_nil_p(b) ? 0 : 1 );
        Handle<Value> *args = new Handle<Value>[alen2];
        for (int i=0; i<alen; i++) {
            args[i] = mrubyobj2js(mrb, a[i]);
        }
        if (!mrb_nil_p(b)) {
            args[alen] = rubyproc2jsfunc(mrb, b);
        }
        {
            TryCatch try_catch;
            Local<Value> retval = Function::Cast(*elem)->Call(jsobj, alen2, args);

            delete []args;
            if (*retval) {
                mrb_value ret =  jsobj2ruby(mrb, retval);
                return ret;
            } else { // got exception
                assert(try_catch.HasCaught());
                Local<Value> exc(try_catch.Exception());
                DisplayExceptionLine(try_catch);
                DBG("JS LEVEL Exception");
                // TODO: better exception
            }
        }
        // You need to raise exception after TryCatch object was released.
        // mrb_raise calls longjmp, it doesn't call the destructor.
        mrb_raise(mrb, E_RUNTIME_ERROR, "Got js exception");
        return mrb_undef_value(); // should not reach here
    } else {
        DBG("Is not a function");
        // mrb_p(mrb, jsobj2ruby(mrb, elem));
        // jsobjdump(jsobj);
        mrb_value ret =  jsobj2ruby(mrb, elem);
        return ret;
    }
}

/// install NodeJS::Object to mruby world
struct RClass * nm_inject_nodejs_object(mrb_state *mrb) {
    struct RClass *obj = mrb_define_class(mrb, "NodeJS::Object", mrb->object_class);
    MRB_SET_INSTANCE_TT(obj, MRB_TT_DATA);
    mrb_define_method(mrb, obj, "method_missing", node_object_method_missing, ARGS_ANY());
    return obj;
}

