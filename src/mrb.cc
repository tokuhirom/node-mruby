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

// TODO: mrb_define_global_const(mrb, "ARGV", ARGV);

KHASH_DECLARE(ht, mrb_value, mrb_value, 1);

static Handle<Value> rubyobj2js(mrb_state*mrb, const mrb_value &v);
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


static mrb_value node_require(mrb_state *mrb, mrb_value self);
static mrb_value node_eval(mrb_state *mrb, mrb_value self);

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

        RClass * s = mrb_define_class(mrb_, "NodeJS", mrb_->object_class);
        mrb_define_class_method(mrb_, s, "require", node_require,      ARGS_REQ(1));
        mrb_define_class_method(mrb_, s, "eval",    node_eval,         ARGS_REQ(1));
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

        mrbc_filename(MRB_, CXT_, "-e");

        mrb_value result = mrb_load_string_cxt(MRB_, *src, CXT_);

        if (MRB_->exc) {
            mrb_value val = mrb_obj_value(MRB_->exc);
            mrb_p(MRB_, val);
            return ThrowException(rubyobj2js(MRB_, val));
        } else {
            return scope.Close(rubyobj2js(MRB_, result));
        }
    }

    static Handle<Value> loadFile(const Arguments& args) {
        HandleScope scope;

        ARG_STR(0, fname);

        mrbc_filename(MRB_, CXT_, *fname);
        FILE * fp = fopen(*fname, "rb");
        if (!fp) {
            return ThrowException(Exception::Error(String::New("Cannot open file")));
        }

        mrb_value result = mrb_load_file_cxt(MRB_, fp, CXT_);
        if (MRB_->exc) {
            mrb_value val = mrb_obj_value(MRB_->exc);
            return ThrowException(rubyobj2js(MRB_, val));
        } else {
            return scope.Close(rubyobj2js(MRB_, result));
        }
    }
};

#undef MRB_

static Handle<Value> rubyobj2js(mrb_state *mrb, const mrb_value &v) {
    HandleScope scope;
    switch (mrb_type(v)) {
    case MRB_TT_FALSE:
        return scope.Close(Boolean::New(false));
        // what's this object?
        abort();
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

        Local<Value> arg0 = External::New(mrb);
        Local<Value> arg1 = External::New(vvv);
        Local<Value> args[] = {arg0, arg1};
        return scope.Close(
            NodeMRubyObject::constructor_template->GetFunction()->NewInstance(2, args)
        );
    }
    }
    return ThrowException(Exception::Error(String::New("[node-mruby] Unknown object type")));
}

static mrb_value jsobj2ruby(mrb_state* mrb, Handle<Value> val) {
    if (val->IsTrue()) {
        return mrb_true_value();
    } else if (val->IsFalse()) {
        return mrb_false_value();
    } else if (val->IsNull()) {
        return mrb_undef_value();
    } else if (val->IsUndefined()) {
        return mrb_undef_value();
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
        struct RClass *c = mrb_define_class(mrb, "MRuby::Function", mrb->object_class);
        NodeMRubyValueContainer * vc = new NodeMRubyValueContainer(val);
        return mrb_obj_value(Data_Wrap_Struct(mrb, c, &node_mruby_function_data_type, vc));
    } else if (val->IsObject()) {
        // if (NodePerlObject::constructor_template->HasInstance(jsobj)) {
        /*
        Handle<Object> jsobj = Handle<Object>::Cast(val);
        Handle<Array> keys = jsobj->GetOwnPropertyNames();
        mrb_value hash = mrb_hash_new(mrb);
        for (size_t i=0; i<keys->Length(); ++i) {
            mrb_value k = jsobj2ruby(mrb, keys->Get(i));
            mrb_value v = jsobj2ruby(mrb, jsobj->Get(keys->Get(i)));
            mrb_hash_set(mrb, hash, k, v);
        }
        return hash;
        */
        struct RClass *c = mrb_define_class(mrb, "MRuby::Object", mrb->object_class);
        NodeMRubyValueContainer * vc = new NodeMRubyValueContainer(val);
        return mrb_obj_value(Data_Wrap_Struct(mrb, c, &node_mruby_function_data_type, vc));
    } else if (val->IsInt32()) {
        return mrb_fixnum_value(val->Int32Value());
    } else if (val->IsUint32()) {
        return mrb_fixnum_value(val->Uint32Value());
    } else if (val->IsNumber()) {
        return mrb_float_value(val->NumberValue());
    } else {
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
    std::cerr << "LOAD" << std::endl;
    if (*retval) {
    std::cerr << "converting" << std::endl;
        mrb_value ret = jsobj2ruby(mrb, retval);
    std::cerr << "converted" << std::endl;
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

Persistent<FunctionTemplate> NodeMRuby::constructor_template;
Persistent<FunctionTemplate> NodeMRubyObject::constructor_template;
Persistent<Function> NodeMRuby::require;
Persistent<Function> NodeMRuby::eval;

extern "C" void init(Handle<Object> target) {
    NodeMRuby::Init(target);
    NodeMRubyObject::Init(target);
}

