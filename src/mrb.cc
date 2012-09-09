#include "nodeutil.h"

#include <iostream>

#include "mruby.h"
#include "mruby/array.h"
#include "mruby/class.h"
#include "mruby/compile.h"
#include "mruby/hash.h"
#include "mruby/khash.h"
#include "mruby/proc.h"
#include "mruby/data.h"
#include "mruby/string.h"
#include "mruby/variable.h"

using namespace v8;
using namespace node;

#define DBG(str) std::cerr << str << std::endl

// TODO: mrb_define_global_const(mrb, "ARGV", ARGV);

KHASH_DECLARE(ht, mrb_value, mrb_value, 1);

static Handle<Value> rubyobj2js(mrb_state*mrb, const mrb_value &v);
static Handle<Value> rubyobj2js(mrb_state*mrb, const mrb_value &v, Handle<Function> object_constructor);
static mrb_value jsobj2ruby(mrb_state* mrb, Handle<Value> val);

struct NodeMRubyValueContainer {
    Handle<Value> v_;
    NodeMRubyValueContainer(Handle<Value> v) : v_(v) { }
};

static void node_mruby_value_container_free(mrb_state * mrb, void* data) {
    struct NodeMRubyValueContainer* d = static_cast<struct NodeMRubyValueContainer*>(data);
    delete d;
}

static const struct mrb_data_type node_mruby_function_data_type = {
    "mruby_function", node_mruby_value_container_free
};


/**
 * mruby methods.
 */
static mrb_value node_require(mrb_state *mrb, mrb_value self);
static mrb_value node_eval(mrb_state *mrb, mrb_value self);
static mrb_value node_object_method_missing(mrb_state *mrb, mrb_value self);
static mrb_value node_function_call(mrb_state *mrb, mrb_value self);

#define VALUE_ (Unwrap<NodeMRubyObject>(args.This())->value_)
#define MRB_   (Unwrap<NodeMRubyObject>(args.This())->mrb_)

class NodeMRubyObject : ObjectWrap {
public:
    mrb_state* mrb_;
    mrb_value* value_;

    static Persistent<FunctionTemplate> constructor_template;

    static void Init(Handle<Object> target) {
        Local<FunctionTemplate> t = FunctionTemplate::New(NodeMRubyObject::New);
        constructor_template = Persistent<FunctionTemplate>::New(t);
        constructor_template->SetClassName(String::NewSymbol("mRubyObject"));

        Local<ObjectTemplate> instance_template = constructor_template->InstanceTemplate();
        instance_template->SetInternalFieldCount(1);

        NODE_SET_PROTOTYPE_METHOD(t, "inspect", NodeMRubyObject::inspect);

        // target->Set(String::NewSymbol("mRubyObject"), constructor_template->GetFunction());
    }

    static Handle<Value> New(const Arguments& args) {
        HandleScope scope;

        if (!args.IsConstructCall())
            return args.Callee()->NewInstance();

        ARG_EXT(0, jsmrb);
        ARG_EXT(1, jsval);

        mrb_state *m = static_cast<mrb_state*>(jsmrb->Value());
        mrb_value *v = static_cast<mrb_value*>(jsval->Value());
        (new NodeMRubyObject(m, v))->Wrap(args.Holder());
        return scope.Close(args.Holder());
    }

    NodeMRubyObject(mrb_state* m, mrb_value* v) : mrb_(m), value_(v) {
    }
    ~NodeMRubyObject() {
        free(value_);
    }

    static Handle<Value> inspect(const Arguments& args) {
        HandleScope scope;
        mrb_value* val = VALUE_;
        mrb_value inspected = mrb_obj_inspect(MRB_, *val);
        Handle<Value> retval = rubyobj2js(MRB_, inspected);
        return scope.Close(retval);
    }
};

#undef MRB_
#undef VALUE_

#define MRB_   (Unwrap<NodeMRuby>(args.This())->mrb_)
#define CXT_   (Unwrap<NodeMRuby>(args.This())->cxt_)
class NodeMRuby : ObjectWrap {
public:
    mrb_state* mrb_;
    mrbc_context *cxt_;
    struct RClass *mruby_node_object_class_;
    struct RClass *mruby_node_function_class_;

    static Persistent<Function> require;
    static Persistent<Function> eval;
    static Persistent<FunctionTemplate> constructor_template;

    static void Init(Handle<Object> target) {
        Local<FunctionTemplate> t = FunctionTemplate::New(NodeMRuby::New);
        constructor_template = Persistent<FunctionTemplate>::New(t);
        constructor_template->SetClassName(String::NewSymbol("mRuby"));

        Local<ObjectTemplate> instance_template = constructor_template->InstanceTemplate();
        instance_template->SetInternalFieldCount(1);

        NODE_SET_PROTOTYPE_METHOD(t, "loadString", NodeMRuby::loadString);
        NODE_SET_PROTOTYPE_METHOD(t, "loadFile",   NodeMRuby::loadFile);
        NODE_SET_METHOD(target, "init", NodeMRuby::init);

        target->Set(String::NewSymbol("mRuby"), constructor_template->GetFunction());
    }

    static Handle<Value> New(const Arguments& args) {
        HandleScope scope;

        if (!args.IsConstructCall())
            return args.Callee()->NewInstance();

        (new NodeMRuby())->Wrap(args.Holder());
        return scope.Close(args.Holder());
    }

    NodeMRuby() {
        mrb_ = mrb_open();
        if (!mrb_) {
            ThrowException(Exception::Error(String::New("Cannot create mrb_state")));
        }
        cxt_ = mrbc_context_new(mrb_);
        assert(cxt_);

        // define NodeJS class.
        RClass * s = mrb_define_class(mrb_, "NodeJS", mrb_->object_class);
        mrb_define_class_method(mrb_, s, "require", node_require,      ARGS_REQ(1));
        mrb_define_class_method(mrb_, s, "eval",    node_eval,         ARGS_REQ(1));

        // define NodeJS::Object class.
        this->mruby_node_object_class_ = mrb_define_class(mrb_, "NodeJS::Object", mrb_->object_class);
        MRB_SET_INSTANCE_TT(this->mruby_node_object_class_, MRB_TT_DATA);
        mrb_define_method(mrb_, this->mruby_node_object_class_, "method_missing", node_object_method_missing, ARGS_ANY());

        // define NodeJS::Function class.
        this->mruby_node_function_class_ = mrb_define_class(mrb_, "NodeJS::Function", this->mruby_node_object_class_);
        mrb_define_method(mrb_, this->mruby_node_object_class_, "call", node_function_call, ARGS_ANY());

        mrb_->ud = this;
    }
    ~NodeMRuby() {
        mrbc_context_free(mrb_, cxt_);
        mrb_close(mrb_);
    }
    static Handle<Value> init(const Arguments& args) {
        HandleScope scope;
        ARG_FUNC(0, require);
        ARG_FUNC(1, eval);
        NodeMRuby::require = Persistent<Function>::New(require);
        NodeMRuby::eval    = Persistent<Function>::New(eval);
        return scope.Close(Undefined());
    }
    static Handle<Value> loadString(const Arguments& args) {
        HandleScope scope;

        ARG_STR(0, src);

        // after calling mrb_load_string_cxt, NodeMRubyObject::constructor_template->GetFunction() returns NULL value.
        // This is a workaround for it.
        // If you can fix this issue, make it this code as simpler.
        Local<Function> object_constructor = NodeMRubyObject::constructor_template->GetFunction();

        mrbc_filename(MRB_, CXT_, "-e");

        mrb_value result = mrb_load_string_cxt(MRB_, *src, CXT_);

        if (MRB_->exc) {
            mrb_value val = mrb_obj_value(MRB_->exc);
            mrb_p(MRB_, val);
            return ThrowException(rubyobj2js(MRB_, val, object_constructor));
        } else {
            return scope.Close(rubyobj2js(MRB_, result));
        }
    }

    static Handle<Value> loadFile(const Arguments& args) {
        HandleScope scope;

        // after calling mrb_load_string_cxt, NodeMRubyObject::constructor_template->GetFunction() returns NULL value.
        // This is a workaround for it.
        // If you can fix this issue, make it this code as simpler.
        Local<Function> object_constructor = NodeMRubyObject::constructor_template->GetFunction();

        ARG_STR(0, fname);

        mrbc_filename(MRB_, CXT_, *fname);
        FILE * fp = fopen(*fname, "rb");
        if (!fp) {
            return ThrowException(Exception::Error(String::New("Cannot open file")));
        }

        mrb_value result = mrb_load_file_cxt(MRB_, fp, CXT_);
        if (MRB_->exc) {
            mrb_value val = mrb_obj_value(MRB_->exc);
            return ThrowException(rubyobj2js(MRB_, val, object_constructor));
        } else {
            return scope.Close(rubyobj2js(MRB_, result));
        }
    }
};

#undef MRB_

static Handle<Value> rubyobj2js(mrb_state *mrb, const mrb_value &v) {
    Local<Function> func = NodeMRubyObject::constructor_template->GetFunction();
    return rubyobj2js(mrb, v, func);
}
static Handle<Value> rubyobj2js(mrb_state *mrb, const mrb_value &v, Handle<Function> object_constructor) {
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
            retval->Set(i, rubyobj2js(mrb, (ptr[i])));
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
                    rubyobj2js(mrb, kk),
                    rubyobj2js(mrb, vv)
                );
            }
        }
        return scope.Close(retval);
    }
    case MRB_TT_STRING: {
        return scope.Close(String::New(RSTRING_PTR(v), RSTRING_LEN(v)));
    }
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
    case MRB_TT_PROC:
    case MRB_TT_FREE:
    case MRB_TT_RANGE: {
        mrb_value * vvv = (mrb_value*)malloc(sizeof(mrb_value));
        *vvv = v;

        assert(mrb);
        assert(vvv);
        Local<Value> arg0 = External::New(mrb);
        Local<Value> arg1 = External::New(vvv);
        Local<Value> args[] = {arg0, arg1};
        Local<Value> ret = object_constructor->NewInstance(2, args);
        return scope.Close(ret);
    }
    }
    return ThrowException(Exception::Error(String::New("[node-mruby] Unknown object type")));
}

inline static mrb_value jsobj2ruby(mrb_state* mrb, Handle<Value> val) {
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
        struct RClass *c = reinterpret_cast<NodeMRuby*>(mrb->ud)->mruby_node_function_class_;
        NodeMRubyValueContainer * vc = new NodeMRubyValueContainer(val);
        return mrb_obj_value(Data_Wrap_Struct(mrb, c, &node_mruby_function_data_type, vc));
    } else if (val->IsObject()) {
        struct RClass *c = reinterpret_cast<NodeMRuby*>(mrb->ud)->mruby_node_object_class_;
        NodeMRubyValueContainer * vc = new NodeMRubyValueContainer(val);
        return mrb_obj_value(Data_Wrap_Struct(mrb, c, &node_mruby_function_data_type, vc));
    } else if (val->IsInt32()) {
        return mrb_fixnum_value(val->Int32Value());
    } else if (val->IsUint32()) {
        return mrb_fixnum_value(val->Uint32Value());
    } else if (val->IsNumber()) {
        return mrb_float_value(val->NumberValue());
    } else {
        // TODO: better exception
        std::cerr << "OOOOOPS!" << std::endl;
        ThrowException(Exception::Error(String::New("Unknown type")));
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
        return mrb_undef_value();
    }
}

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
        return mrb_undef_value();
    }
}

inline mrb_value mrb_sym_to_s(mrb_state *mrb, mrb_value sym) {
    mrb_sym id = SYM2ID(sym);
    int len;

    const char *p = mrb_sym2name_len(mrb, id, &len);
    return mrb_str_new(mrb, p, len);
}

static mrb_value node_object_method_missing(mrb_state *mrb, mrb_value self) {
    int alen;
    mrb_value name, *a;

    mrb_get_args(mrb, "o*", &name, &a, &alen);
    if (!SYMBOL_P(name)) {
        mrb_raise(mrb, E_TYPE_ERROR, "name should be a symbol");
    }

    Handle<Object> jsobj = ((NodeMRubyValueContainer*)mrb_get_datatype(mrb, self, &node_mruby_function_data_type))->v_->ToObject();
    mrb_value rsName = mrb_sym_to_s(mrb, name);
    Handle<String> hsName = String::New(RSTRING_PTR(rsName), RSTRING_LEN(rsName));
    Handle<Value> elem = jsobj->Get(hsName);
    if (elem->IsFunction()) {
        Handle<Value> *args = new Handle<Value>[alen];
        for (int i=0; i<alen; i++) {
            args[i] = rubyobj2js(mrb, a[i]);
        }
        Local<Value> retval = Function::Cast(*elem)->Call(jsobj, alen, args);
        delete []args;
        if (*retval) {
            return jsobj2ruby(mrb, retval);
        } else { // got exception
            // TODO: better exception
            mrb_raise(mrb, E_RUNTIME_ERROR, "Got js exception");
            return mrb_undef_value(); // should not reach here
        }
    } else {
        return jsobj2ruby(mrb, elem);
    }
}

static mrb_value node_function_call(mrb_state *mrb, mrb_value self) {
    int alen;
    mrb_value name, *a;

    mrb_get_args(mrb, "*", &a, &alen);
    Handle<Object> jsobj = ((NodeMRubyValueContainer*)mrb_get_datatype(mrb, self, &node_mruby_function_data_type))->v_->ToObject();

    Handle<Value> *args = new Handle<Value>[alen];
    for (int i=0; i<alen; i++) {
        args[i] = rubyobj2js(mrb, a[i]);
    }
    Local<Value> retval = Function::Cast(*jsobj)->Call(jsobj, alen, args);
    delete []args;
    return jsobj2ruby(mrb, retval);
}

Persistent<FunctionTemplate> NodeMRuby::constructor_template;
Persistent<FunctionTemplate> NodeMRubyObject::constructor_template;
Persistent<Function> NodeMRuby::require;
Persistent<Function> NodeMRuby::eval;

extern "C" void init(Handle<Object> target) {
    NodeMRuby::Init(target);
    NodeMRubyObject::Init(target);
}

