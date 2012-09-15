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

#define TRACE_DESTRUCTOR(str) std::cerr << "[CLOSE] " << str << std::endl
#define DBG(str) std::cerr << str << std::endl

// TODO: mrb_define_global_const(mrb, "ARGV", ARGV);

KHASH_DECLARE(ht, mrb_value, mrb_value, 1);

static Handle<Value> mrubyobj2js(Handle<Object> nmrb, const mrb_value &v);
static Handle<Value> mrubyobj2js(mrb_state*mrb, const mrb_value &v);
static Handle<Value> rubyproc2jsfunc(mrb_state*mrb, const mrb_value &v);
static Handle<Object> mrb2nmrb(mrb_state* mrb);
static mrb_value jsobj2ruby(mrb_state* mrb, Handle<Value> val);

struct NodeMRubyValueContainer {
    Persistent<Value> v_;
    int type_;
    NodeMRubyValueContainer(Handle<Value> v, int t) : type_(t) {
        v_ = Persistent<Value>::New(v);
    }
    ~NodeMRubyValueContainer() {
        TRACE_DESTRUCTOR("NodeMRubyValueContainer");
        std::cerr << "container type : " << type_ <<std::endl;
        // v_.MakeWeak(this, NULL);
        v_.Dispose();
        v_.Clear();
    }
};

static void node_mruby_value_container_free(mrb_state * mrb, void* data) {
    TRACE_DESTRUCTOR("Ruby#NodeMRubyValueContainer");
    struct NodeMRubyValueContainer* d = static_cast<struct NodeMRubyValueContainer*>(data);
    delete d;
}

static const struct mrb_data_type node_mruby_function_data_type = {
    // "mruby_function", NULL
    "mruby_function", node_mruby_value_container_free
};

/**
 * This is a container object to save a NodeMRuby object.
 * This struct must be store in mrb->ud
 */
struct NodeMRubyUDContext {
    Persistent<Object> nmrb_;

    NodeMRubyUDContext(Handle<Object> nmrb) {
        nmrb_ = Persistent<Object>::New(nmrb);
        nmrb_.MakeWeak(this, WeakCallback);
        nmrb_.MarkIndependent();
    }
    ~NodeMRubyUDContext() {
        TRACE_DESTRUCTOR("NodeMRubyUDContext");
        nmrb_.Dispose();
        nmrb_.Clear();
    }
    struct RClass *mruby_node_function_class();
    struct RClass *mruby_node_object_class();
    Handle<Object> nmrb() {
        return nmrb_;
    }
private:
    static void WeakCallback (v8::Persistent<v8::Value> value, void *data) {
        // NodeMRubyUDContext *obj = static_cast<NodeMRubyUDContext*>(data);
        // assert(value == obj->nmrb_);
        // assert(!obj->refs_);
        assert(value.IsNearDeath());
        // delete obj;
    }
};


/**
 * mruby methods.
 */
static mrb_value node_require(mrb_state *mrb, mrb_value self);
static mrb_value node_eval(mrb_state *mrb, mrb_value self);
static mrb_value node_log(mrb_state *mrb, mrb_value self);
static mrb_value node_object_method_missing(mrb_state *mrb, mrb_value self);
static mrb_value node_function_call(mrb_state *mrb, mrb_value self);

/**
 * Container class for mRuby level callback function.
 */
class NodeMRubyFunctionInner : ObjectWrap {
public:
    static Persistent<FunctionTemplate> constructor_template;

    struct Callback {
        mrb_state *mrb;
        mrb_value callback;
        Callback(mrb_state* m, const mrb_value &c) : mrb(m), callback(c) { }
    };

    Callback * cb_;

    static void Init(Handle<Object> target) {
        DBG("NodeMRubyFunctionInner::Init");
        Local<FunctionTemplate> t = FunctionTemplate::New(NodeMRubyFunctionInner::New);
        constructor_template = Persistent<FunctionTemplate>::New(t);
        constructor_template->SetClassName(String::NewSymbol("mRubyFunctionInner"));

        Local<ObjectTemplate> instance_template = constructor_template->InstanceTemplate();
        instance_template->SetInternalFieldCount(1);
    }

    static Handle<Value> New(const Arguments& args) {
        HandleScope scope;

        if (!args.IsConstructCall())
            return args.Callee()->NewInstance();

        ARG_EXT(0, jscb);
        Callback *cb = static_cast<Callback*>(jscb->Value());
        (new NodeMRubyFunctionInner(cb))->Wrap(args.Holder());
        return scope.Close(args.Holder());
    }
    NodeMRubyFunctionInner(Callback * cb) : cb_(cb) { }
    ~NodeMRubyFunctionInner() {
        TRACE_DESTRUCTOR("NodeMRubyFunctionInner");
        delete cb_;
    } // TODO: is it works?
};

class NodeMRubyFunction : ObjectWrap {
public:
    static Persistent<FunctionTemplate> constructor_template;

    struct Callback {
        mrb_value callback;
    };

    static Handle<Value> Call(const Arguments& args) {
        HandleScope scope;

        // DBG("NodeMRubyFunction::New");
        Local<Value> jsinner = args.Callee()->Get(0);
        assert(jsinner->IsObject());
        NodeMRubyFunctionInner::Callback * cbstruct  = Unwrap<NodeMRubyFunctionInner>(jsinner->ToObject())->cb_;

        // TODO: optimize for few arguments.
        mrb_value * argv = new mrb_value[args.Length()];
        mrb_state* mrb = cbstruct->mrb;
        for (int i=0; i<args.Length(); i++) {
            int ai = mrb_gc_arena_save(mrb);

            argv[i] = jsobj2ruby(cbstruct->mrb, args[i]);

            mrb_gc_arena_restore(mrb, ai);
        }
        mrb_sym mid = mrb_intern(cbstruct->mrb, "call");
        mrb_value retval = mrb_funcall_argv(cbstruct->mrb, cbstruct->callback, mid, args.Length(), argv);
        delete []argv;

        return scope.Close(mrubyobj2js(mrb2nmrb(cbstruct->mrb), retval));
    }
};

#define VALUE_ (Unwrap<NodeMRubyObject>(args.This())->value_)
#define MRB_   (Unwrap<NodeMRubyObject>(args.This())->mrb_)

class NodeMRubyObject : ObjectWrap {
public:
    mrb_state* mrb_;
    mrb_value* value_;
    Persistent<Object> nmrb_;

    static Persistent<FunctionTemplate> constructor_template;

    static void Init(Handle<Object> target) {
        Local<FunctionTemplate> t = FunctionTemplate::New(NodeMRubyObject::New);
        constructor_template = Persistent<FunctionTemplate>::New(t);
        constructor_template->SetClassName(String::NewSymbol("mRubyObject"));

        Local<ObjectTemplate> instance_template = constructor_template->InstanceTemplate();
        instance_template->SetInternalFieldCount(1);

        NODE_SET_PROTOTYPE_METHOD(t, "inspect", NodeMRubyObject::inspect);
    }

    static Handle<Value> New(const Arguments& args) {
        HandleScope scope;

        if (!args.IsConstructCall())
            return args.Callee()->NewInstance();

        ARG_EXT(0, jsmrb);
        ARG_EXT(1, jsval);
        ARG_OBJ(2, nmrb);
        mrb_state *m = static_cast<mrb_state*>(jsmrb->Value());
        mrb_value *v = static_cast<mrb_value*>(jsval->Value());
        (new NodeMRubyObject(m, v, nmrb))->Wrap(args.Holder());

        return scope.Close(args.Holder());
    }

    NodeMRubyObject(mrb_state* m, mrb_value* v, Handle<Object> nmrb) : mrb_(m), value_(v) {
        nmrb_ = Persistent<Object>::New(nmrb);
    }
    ~NodeMRubyObject() {
        TRACE_DESTRUCTOR("NodeMRubyObject");
        if (*nmrb_) {
            nmrb_.Dispose();
            nmrb_.Clear();
        }
        free(value_);
    }

    static Handle<Value> inspect(const Arguments& args) {
        HandleScope scope;
        mrb_value* val = VALUE_;
        mrb_value inspected = mrb_obj_inspect(MRB_, *val);
        Handle<Value> retval = mrubyobj2js(MRB_, inspected);
        return scope.Close(retval);
    }
};

#undef MRB_
#undef VALUE_

class NodeMRubyLoadTest : ObjectWrap {
public:
    static void Init(Handle<Object> target) {
        NODE_SET_METHOD(target, "loadTestOpenClose", NodeMRubyLoadTest::openClose);
    }
    static Handle<Value> openClose(const Arguments& args) {
        HandleScope scope;
        while (1) {
            mrb_state *mrb = mrb_open();
            mrb_close(mrb);
        }
        return scope.Close(Undefined());
    }
};

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
    static Persistent<Function> log;
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

        (new NodeMRuby(args.Holder()))->Wrap(args.Holder());
        return scope.Close(args.Holder());
    }

    NodeMRuby(Handle<Object> nmrb) {
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
        mrb_define_class_method(mrb_, s, "log",     node_log,          ARGS_REQ(1));

        // define NodeJS::Object class.
        this->mruby_node_object_class_ = mrb_define_class(mrb_, "NodeJS::Object", mrb_->object_class);
        MRB_SET_INSTANCE_TT(this->mruby_node_object_class_, MRB_TT_DATA);
        mrb_define_method(mrb_, this->mruby_node_object_class_, "method_missing", node_object_method_missing, ARGS_ANY());

        // define NodeJS::Function class.
        this->mruby_node_function_class_ = mrb_define_class(mrb_, "NodeJS::Function", this->mruby_node_object_class_);
        mrb_define_method(mrb_, this->mruby_node_object_class_, "call", node_function_call, ARGS_ANY());

        mrb_->ud = new NodeMRubyUDContext(nmrb);
    }
    ~NodeMRuby() {
        TRACE_DESTRUCTOR("NodeMRuby");
        NodeMRubyUDContext* udc = reinterpret_cast<NodeMRubyUDContext*>(mrb_->ud);
        delete udc;
        mrbc_context_free(mrb_, cxt_);
        mrb_close(mrb_);
    }
    static Handle<Value> init(const Arguments& args) {
        HandleScope scope;
        ARG_FUNC(0, require);
        ARG_FUNC(1, eval);
        ARG_FUNC(2, log);
        NodeMRuby::require = Persistent<Function>::New(require);
        NodeMRuby::eval    = Persistent<Function>::New(eval);
        NodeMRuby::log     = Persistent<Function>::New(log);
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
            return ThrowException(mrubyobj2js(args.This(), val));
        } else {
            return scope.Close(mrubyobj2js(args.This(), result));
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
            DBG("--- Got mruby level exception in loadFile");
            mrb_value val = mrb_obj_value(MRB_->exc);
            mrb_p(MRB_, val);
            // return ThrowException(mrubyobj2js(args.This(), val));
            // TODO: better exception
            return ThrowException(Exception::Error(String::New("Caused ruby level exception")));
        } else {
            return scope.Close(mrubyobj2js(args.This(), result));
        }
    }

    /**
     * Unwrap and get a data from outside
     */
    static struct RClass* GetFunctionClass(Handle<Object> self) {
        return (Unwrap<NodeMRuby>(self)->mruby_node_function_class_);
    }
    static struct RClass* GetObjectClass(Handle<Object> self) {
        return (Unwrap<NodeMRuby>(self)->mruby_node_object_class_);
    }
    static mrb_state* GetMRBState(Handle<Object> self) {
        return (Unwrap<NodeMRuby>(self)->mrb_);
    }
};

#undef MRB_

static Handle<Value> mrubyobj2js(Handle<Object> nmrb, const mrb_value &v) {
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
static Handle<Value> mrubyobj2js(mrb_state *mrb, const mrb_value &v) {
    return mrubyobj2js(mrb2nmrb(mrb), v);
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
        fprintf(stderr, "UD: %X\n", mrb->ud);
        std::cerr << "HOGHEOGE" << std::endl;
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

inline mrb_value mrb_sym_to_s(mrb_state *mrb, mrb_value sym) {
    mrb_sym id = SYM2ID(sym);
    int len;

    const char *p = mrb_sym2name_len(mrb, id, &len);
    return mrb_str_new(mrb, p, len);
}

// NodeJS::Object#method_missing
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
        int alen2 = alen + ( mrb_nil_p(b) ? 0 : 1 );
        Handle<Value> *args = new Handle<Value>[alen2];
        for (int i=0; i<alen; i++) {
            args[i] = mrubyobj2js(mrb, a[i]);
        }
        if (!mrb_nil_p(b)) {
            args[alen] = rubyproc2jsfunc(mrb, b);
        }
        Local<Value> retval = Function::Cast(*elem)->Call(jsobj, alen2, args);

        delete []args;
        if (*retval) {
            mrb_value ret =  jsobj2ruby(mrb, retval);
            return ret;
        } else { // got exception
            DBG("JS LEVEL Exception");
            // TODO: better exception
            mrb_raise(mrb, E_RUNTIME_ERROR, "Got js exception");
            return mrb_undef_value(); // should not reach here
        }
    } else {
        DBG("Is not a function");
        // mrb_p(mrb, jsobj2ruby(mrb, elem));
        // jsobjdump(jsobj);
        mrb_value ret =  jsobj2ruby(mrb, elem);
        return ret;
    }
}

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

static Handle<Value> rubyproc2jsfunc(mrb_state*mrb, const mrb_value &v) {
    HandleScope scope;

    DBG("rubyproc2jsfunc");

    // get a instance of NodeMRubyFunctionInner
    Local<Value> arg0 = External::New(new NodeMRubyFunctionInner::Callback(mrb, v));
    Local<Value> args[] = {arg0};
    Local<Object> inner = NodeMRubyFunctionInner::constructor_template->GetFunction()->NewInstance(1, args);

    // create function object
    Local<FunctionTemplate> t = FunctionTemplate::New(NodeMRubyFunction::Call);
    Local<ObjectTemplate> instance_template = t->InstanceTemplate();
    instance_template->SetInternalFieldCount(1);
    Local<Function> func = t->GetFunction();
    func->Set(0, inner);
    func->Set(1, mrb2nmrb(mrb));
    return scope.Close(func);
}

/**
 * Implementation of NodeMRubyUDContext
 */
struct RClass *NodeMRubyUDContext::mruby_node_function_class() {
    return NodeMRuby::GetFunctionClass(nmrb_);
}
struct RClass *NodeMRubyUDContext::mruby_node_object_class() {
    return NodeMRuby::GetObjectClass(nmrb_);
}

static Handle<Object> mrb2nmrb(mrb_state* mrb) {
    return reinterpret_cast<NodeMRubyUDContext*>(mrb->ud)->nmrb();
}

/**
 * Static global variables.
 */
Persistent<FunctionTemplate> NodeMRuby::constructor_template;
Persistent<FunctionTemplate> NodeMRubyObject::constructor_template;
// Persistent<FunctionTemplate> NodeMRubyFunction::constructor_template;
Persistent<FunctionTemplate> NodeMRubyFunctionInner::constructor_template;
Persistent<Function> NodeMRuby::require;
Persistent<Function> NodeMRuby::eval;
Persistent<Function> NodeMRuby::log;

extern "C" void init(Handle<Object> target) {
    DBG("Init");
    NodeMRuby::Init(target);
    NodeMRubyObject::Init(target);
    NodeMRubyFunctionInner::Init(target);
    NodeMRubyLoadTest::Init(target);
    // NodeMRubyFunction::Init(target);
}

