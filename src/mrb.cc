#include "nodeutil.h"

#include "mruby.h"
#include "mruby/array.h"
#include "mruby/class.h"
#include "mruby/compile.h"
#include "mruby/hash.h"
#include "mruby/khash.h"
#include "mruby/proc.h"
#include "mruby/string.h"
#include "mruby/variable.h"

using namespace v8;
using namespace node;

#define MRB_ (Unwrap<NodeMRuby>(args.This())->mrb_)
#define CXT_ (Unwrap<NodeMRuby>(args.This())->cxt_)

KHASH_DECLARE(ht, mrb_value, mrb_value, 1);

static Handle<Value> rubyobj2js(mrb_value v) {
    HandleScope scope;
    switch (mrb_type(v)) {
    case MRB_TT_FALSE:
        return scope.Close(Boolean::New(false));
    case MRB_TT_FREE:
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
    case MRB_TT_MAIN:
        // what's this object?
        abort();
    case MRB_TT_OBJECT:
        // what's this object?
        abort();
    case MRB_TT_CLASS:
        // what's this object?
        abort();
    case MRB_TT_MODULE:
        // what's this object?
        abort();
    case MRB_TT_ICLASS:
        // what's this object?
        abort();
    case MRB_TT_SCLASS:
        // what's this object?
        abort();
    case MRB_TT_PROC:
        // what's this object?
        abort();
    case MRB_TT_ARRAY: {
        int len = RARRAY_LEN(v);
        Handle<Array> retval = Array::New();
        mrb_value *ptr = RARRAY_PTR(v);
        for (int i=0; i<len; ++i) {
            retval->Set(i, rubyobj2js(ptr[i]));
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
                    rubyobj2js(kk),
                    rubyobj2js(vv)
                );
            }
        }
        return scope.Close(retval);
    }
    case MRB_TT_STRING: {
        return scope.Close(String::New(RSTRING_PTR(v), RSTRING_LEN(v)));
    }
    case MRB_TT_RANGE:
        // what's this object?
        abort();
    case MRB_TT_REGEX:
        // what's this object?
        abort();
    case MRB_TT_STRUCT:
        // what's this object?
        abort();
    case MRB_TT_EXCEPTION:
        // what's this object?
        abort();
    case MRB_TT_MATCH:
        // what's this object?
        abort();
    case MRB_TT_FILE:
        // what's this object?
        abort();
    case MRB_TT_ENV:
        // what's this object?
        abort();
    case MRB_TT_DATA:
        // what's this object?
        abort();
    }
    return ThrowException(Exception::Error(String::New("[node-mruby] Unknown object type")));
}

class NodeMRuby : ObjectWrap {
public:
    mrb_state* mrb_;
    mrbc_context *cxt_;

    static Persistent<FunctionTemplate> constructor_template;

    static void Init(Handle<Object> target) {
        Local<FunctionTemplate> t = FunctionTemplate::New(NodeMRuby::New);
        constructor_template = Persistent<FunctionTemplate>::New(t);
        constructor_template->SetClassName(String::NewSymbol("NodeMRubyObject"));

        Local<ObjectTemplate> instance_template = constructor_template->InstanceTemplate();
        instance_template->SetInternalFieldCount(1);

        NODE_SET_PROTOTYPE_METHOD(t, "run", NodeMRuby::run);

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
            // TODO throw exception
        }
        cxt_ = mrbc_context_new(mrb_);
        assert(cxt_);
    }
    ~NodeMRuby() {
        mrbc_context_free(mrb_, cxt_);
        mrb_close(mrb_);
    }
    static Handle<Value> run(const Arguments& args) {
        HandleScope scope;

        ARG_STR(0, src);

        struct mrb_parser_state *parser = mrb_parser_new(MRB_);
        parser->s = *src;
        parser->send = *src + strlen(*src);
        parser->lineno = 1;
        mrb_parser_parse(parser, CXT_);

        if (0 < parser->nerr) {
            printf("line %d: %s\n", parser->error_buffer[0].lineno, parser->error_buffer[0].message);
            mrb_parser_free(parser);
            return ThrowException(Exception::Error(String::New("Syntax error")));
        }

        int n = mrb_generate_code(MRB_, parser);
        mrb_value result = mrb_run(MRB_,
                        /* pass a proc for evaulation */
                        mrb_proc_new(MRB_, MRB_->irep[n]),
                        mrb_top_self(MRB_));
        Handle<Value> retval;
        if (MRB_->exc) {
            mrb_p(MRB_, mrb_obj_value(MRB_->exc));
            return scope.Close(Undefined());
        } else {
            mrb_p(MRB_, result);

            Handle<Value> retval = rubyobj2js(result);
            mrb_parser_free(parser);
            return scope.Close(retval);
        }
    }
};

#undef MRB_

Persistent<FunctionTemplate> NodeMRuby::constructor_template;

extern "C" void init(Handle<Object> target) {
    NodeMRuby::Init(target);
}
