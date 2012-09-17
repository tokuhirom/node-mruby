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

Handle<Value> mrubyobj2js(Handle<Object> nmrb, const mrb_value &v);
Handle<Value> mrubyobj2js(mrb_state*mrb, const mrb_value &v);
Handle<Value> rubyproc2jsfunc(mrb_state*mrb, const mrb_value &v);
Handle<Object> mrb2nmrb(mrb_state* mrb);
mrb_value jsobj2ruby(mrb_state* mrb, Handle<Value> val);

/**
 * export NodeJS object to ruby world.
 */
void nm_inject_nodejs_api_to_ruby_world(mrb_state* mrb);
/**
 * export NodeJS object to ruby world.
 */
struct RClass * nm_inject_nodejs_object(mrb_state *mrb);
/**
 * export NodeJS function to ruby world.
 */
struct RClass * nm_inject_nodejs_function(mrb_state *mrb, struct RClass * node_object_class);

class NodeMRuby;

/**
 * mruby level value container.
 */
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

extern const struct mrb_data_type node_mruby_function_data_type;

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

/**
 * object wraps mruby's object.
 */
class NodeMRubyObject : ObjectWrap {
protected:
    mrb_state* mrb_;
    mrb_value* value_;
    Persistent<Object> nmrb_;
public:
    static Persistent<FunctionTemplate> constructor_template;

    static void Init(Handle<Object> target) {
        Local<FunctionTemplate> t = FunctionTemplate::New(NodeMRubyObject::New);
        constructor_template = Persistent<FunctionTemplate>::New(t);
        constructor_template->SetClassName(String::NewSymbol("mRubyObject"));

        Local<ObjectTemplate> instance_template = constructor_template->InstanceTemplate();
        instance_template->SetInternalFieldCount(1);
        instance_template->SetNamedPropertyHandler(NodeMRubyObject::GetNamedProperty);

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

    static Handle<Value> GetNamedProperty(Local<String> name,
            const AccessorInfo &info);

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
    mrb_state* mrb() {
        return mrb_;
    }
    mrb_value* value() {
        return value_;
    }
};

#undef MRB_
#undef VALUE_

#define MRB_   (Unwrap<NodeMRuby>(args.This())->mrb_)
#define CXT_   (Unwrap<NodeMRuby>(args.This())->cxt_)
class NodeMRuby : ObjectWrap {
protected:
    mrb_state* mrb_;
    mrbc_context *cxt_;
    struct RClass *mruby_node_object_class_;
    struct RClass *mruby_node_function_class_;

public:
    static Persistent<Function> require;
    static Persistent<Function> eval;
    static Persistent<Function> log;
    static Persistent<Function> method_func_generator;
    static Persistent<Array>    argv;
    static Persistent<FunctionTemplate> constructor_template;

    static void Init(Handle<Object> target) {
        Local<FunctionTemplate> t = FunctionTemplate::New(NodeMRuby::New);
        constructor_template = Persistent<FunctionTemplate>::New(t);
        constructor_template->SetClassName(String::NewSymbol("mRuby"));

        Local<ObjectTemplate> instance_template = constructor_template->InstanceTemplate();
        instance_template->SetInternalFieldCount(1);

        NODE_SET_PROTOTYPE_METHOD(t, "loadString", NodeMRuby::loadString);
        NODE_SET_PROTOTYPE_METHOD(t, "loadFile",   NodeMRuby::loadFile);
        NODE_SET_PROTOTYPE_METHOD(t, "callMethod", NodeMRuby::callMethod);
        NODE_SET_PROTOTYPE_METHOD(t, "defineGlobalConst", NodeMRuby::defineGlobalConst);
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
        nm_inject_nodejs_api_to_ruby_world(mrb_);
        this->mruby_node_object_class_   = nm_inject_nodejs_object(mrb_);
        this->mruby_node_function_class_ = nm_inject_nodejs_function(mrb_, this->mruby_node_object_class_);

        mrb_define_global_const(mrb_, "ARGV", jsobj2ruby(mrb_, NodeMRuby::argv));

        mrb_->ud = new NodeMRubyUDContext(nmrb);
    }
    ~NodeMRuby() {
        TRACE_DESTRUCTOR("NodeMRuby");
        NodeMRubyUDContext* udc = reinterpret_cast<NodeMRubyUDContext*>(mrb_->ud);
        delete udc;
        mrbc_context_free(mrb_, cxt_);
        mrb_close(mrb_);
    }
    /**
     * initialize node-mruby library.
     */
    static Handle<Value> init(const Arguments& args) {
        HandleScope scope;
        ARG_FUNC(0, require);
        ARG_FUNC(1, eval);
        ARG_FUNC(2, log);
        ARG_FUNC(3, methfun);
        ARG_ARRAY(4, argv);
        NodeMRuby::require = Persistent<Function>::New(require);
        NodeMRuby::eval    = Persistent<Function>::New(eval);
        NodeMRuby::log     = Persistent<Function>::New(log);
        NodeMRuby::method_func_generator = Persistent<Function>::New(methfun);
        NodeMRuby::argv    = Persistent<Array>::New(argv);
        return scope.Close(Undefined());
    }
    /**
     * compile and evaluate string ruby code.
     */
    static Handle<Value> loadString(const Arguments& args) {
        HandleScope scope;

        ARG_STR(0, src);

        mrbc_filename(MRB_, CXT_, "-e");

        mrb_value result = mrb_load_string_cxt(MRB_, *src, CXT_);

        if (MRB_->exc) {
            mrb_value val = mrb_obj_value(MRB_->exc);
            mrb_p(MRB_, val);
            // TODO: better diag
            return ThrowException(Exception::Error(String::New("Got a ruby level exception")));
            // return ThrowException(mrubyobj2js(args.This(), val));
        } else {
            return scope.Close(mrubyobj2js(args.This(), result));
        }
    }

    /**
     * load ruby code from file, and evaluate it.
     */
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
     * Call a method.
     */
    static Handle<Value> callMethod(const Arguments& args) {
        HandleScope scope;

        mrb_state* mrb = GetMRBState(args.This());

        ARG_OBJ(0,   obj);
        ARG_STR(1,   meth_name);
        ARG_ARRAY(2, arguments);

        mrb_value * argv = new mrb_value[arguments->Length()];
        for (size_t i=0; i<arguments->Length(); i++) {
            int ai = mrb_gc_arena_save(mrb);

            argv[i] = jsobj2ruby(mrb, arguments->Get(i));

            mrb_gc_arena_restore(mrb, ai);
        }
        mrb_sym mid = mrb_intern(mrb, *meth_name);
        mrb_value* self = Unwrap<NodeMRubyObject>(obj)->value();
        mrb_value rbretval = mrb_funcall_argv(mrb, *self, mid, arguments->Length(), argv);
        delete []argv;

        Handle<Value> jsretval = mrubyobj2js(mrb, rbretval);
        return scope.Close(jsretval);
    }
    /**
     * call mrb_define_global_const
     */
    static Handle<Value> defineGlobalConst(const Arguments& args) {
        HandleScope scope;
        mrb_state* mrb = GetMRBState(args.This());
        ARG_STR(0, name);
        ARG_VAL(1, val);
        mrb_define_global_const(mrb, *name, jsobj2ruby(mrb, val));
        return scope.Close(Undefined());
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
