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

        target->Set(String::NewSymbol("NodeMRubyObject"), constructor_template->GetFunction());
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
        mrb_close(mrb_);
    }
    static Handle<Value> parseString(const Arguments& args) {
        HandleScope scope;
        ARG_STR(0, src);
        struct mrb_parser_state* st = mrb_parse_string(MRB_, *src, CXT_);
        return scope.Close(Undefined());
    }
};

#undef MRB_

extern "C" void init(Handle<Object> target) {
    NodeMRuby::Init(target);
}
