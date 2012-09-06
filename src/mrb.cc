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
        if (MRB_->exc) {
            mrb_p(MRB_, mrb_obj_value(MRB_->exc));
            MRB_->exc = 0;
        } else {
            // TODO return value
            mrb_p(MRB_, result);
        }
        mrb_parser_free(parser);

        return scope.Close(Undefined());
    }
};

#undef MRB_

Persistent<FunctionTemplate> NodeMRuby::constructor_template;

extern "C" void init(Handle<Object> target) {
    NodeMRuby::Init(target);
}
