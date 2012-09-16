#include "node-mruby.h"

KHASH_DECLARE(ht, mrb_value, mrb_value, 1);

Handle<Value> mrubyobj2js(Handle<Object> nmrb, const mrb_value &v) {
    mrb_state* mrb = NodeMRuby::GetMRBState(nmrb);
    HandleScope scope;
    switch (mrb_type(v)) {
    case MRB_TT_FALSE:
        if (mrb_nil_p(v)) {
            return scope.Close(v8::Null());
        } else {
            return scope.Close(Boolean::New(false));
        }
    case MRB_TT_TRUE:
        return scope.Close(Boolean::New(true));
    case MRB_TT_FIXNUM:
        return scope.Close(Integer::New(mrb_fixnum(v)));
    case MRB_TT_SYMBOL:
        return scope.Close(Integer::New(mrb_symbol(v)));
    case MRB_TT_UNDEF:
        return scope.Close(Undefined());
    case MRB_TT_FLOAT:
        return scope.Close(Number::New(mrb_float(v)));
    case MRB_TT_ARRAY: {
        int len = RARRAY_LEN(v);
        Handle<Array> retval = Array::New();
        mrb_value *ptr = RARRAY_PTR(v);
        for (int i=0; i<len; ++i) {
            retval->Set(i, mrubyobj2js(nmrb, (ptr[i])));
        }
        return scope.Close(retval);
    }
    case MRB_TT_HASH: {
        khash_t(ht) * h = RHASH_TBL(v);
        khiter_t k;
        v8::Local<v8::Object> retval = v8::Object::New();

        if (!h) { abort(); }
        for (k = kh_begin(h); k != kh_end(h); k++) {
            if (kh_exist(h, k)) {
                mrb_value kk = kh_key(h,k);
                mrb_value vv = kh_value(h,k);

                retval->Set(
                    mrubyobj2js(nmrb, kk),
                    mrubyobj2js(nmrb, vv)
                );
            }
        }
        return scope.Close(retval);
    }
    case MRB_TT_STRING: {
        return scope.Close(String::New(RSTRING_PTR(v), RSTRING_LEN(v)));
    }
    case MRB_TT_PROC: // TODO: wrap proc?
    case MRB_TT_REGEX:
    case MRB_TT_STRUCT:
    case MRB_TT_EXCEPTION:
    case MRB_TT_MATCH:
    case MRB_TT_FILE:
    case MRB_TT_ENV:
    case MRB_TT_DATA:
    case MRB_TT_MAIN:
    case MRB_TT_OBJECT:
    case MRB_TT_CLASS:
    case MRB_TT_MODULE:
    case MRB_TT_ICLASS:
    case MRB_TT_SCLASS:
    case MRB_TT_FREE:
    case MRB_TT_RANGE: {
        mrb_value * vvv = (mrb_value*)malloc(sizeof(mrb_value));
        *vvv = v;
        mrb_gc_mark_value(mrb, v);

        assert(mrb);
        assert(vvv);
        Local<Value> arg0 = External::New(mrb);
        Local<Value> arg1 = External::New(vvv);
        Local<Value> arg2 = Local<Value>::New(nmrb);
        Local<Value> args[] = {arg0, arg1, arg2};
        Local<Function> object_constructor = NodeMRubyObject::constructor_template->GetFunction();
        Local<Value> ret = object_constructor->NewInstance(3, args);
        return scope.Close(ret);
    }
    }
    return ThrowException(Exception::Error(String::New("[node-mruby] Unknown object type")));
}
Handle<Value> mrubyobj2js(mrb_state *mrb, const mrb_value &v) {
    return mrubyobj2js(mrb2nmrb(mrb), v);
}

mrb_value jsobj2ruby(mrb_state* mrb, Handle<Value> val) {
    if (val->IsTrue()) {
        return mrb_true_value();
    } else if (val->IsFalse()) {
        return mrb_false_value();
    } else if (val->IsNull()) {
        return mrb_nil_value();
    } else if (val->IsUndefined()) {
        return mrb_nil_value(); // TODO: I want to return undef here, but it causes segv. why?
    } else if (val->IsString()) {
        v8::String::Utf8Value u8val(val);
        return mrb_str_new(mrb, *u8val, u8val.length());
    } else if (val->IsArray()) {
        Handle<Array> jsav = Handle<Array>::Cast(val);
        mrb_value retval = mrb_ary_new(mrb);
        for (size_t i=0; i<jsav->Length(); ++i) {
            mrb_ary_push(mrb, retval, jsobj2ruby(mrb, jsav->Get(i)));
        }
        return retval;
    } else if (val->IsFunction()) {
        struct RClass *c = reinterpret_cast<NodeMRubyUDContext*>(mrb->ud)->mruby_node_function_class();
        NodeMRubyValueContainer * vc = new NodeMRubyValueContainer(val, 1);
        return mrb_obj_value(Data_Wrap_Struct(mrb, c, &node_mruby_function_data_type, vc));
    } else if (val->IsObject()) {
        struct RClass *c = reinterpret_cast<NodeMRubyUDContext*>(mrb->ud)->mruby_node_object_class();
        NodeMRubyValueContainer * vc = new NodeMRubyValueContainer(val, 2);
        RData * p = Data_Wrap_Struct(mrb, c, &node_mruby_function_data_type, vc);
        return mrb_obj_value(p);
    } else if (val->IsInt32()) {
        return mrb_fixnum_value(val->Int32Value());
    } else if (val->IsUint32()) {
        DBG("UI32");
        return mrb_fixnum_value(val->Uint32Value());
    } else if (val->IsNumber()) {
        return mrb_float_value(val->NumberValue());
    } else {
        // TODO: better exception
        std::cerr << "OOOOOPS!" << std::endl;
        ThrowException(Exception::Error(String::New("Unknown type")));
        return mrb_undef_value(); // should not reach here
    }
}
