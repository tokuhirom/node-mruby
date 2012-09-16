#include "node-mruby.h"

// TODO: mrb_define_global_const(mrb, "ARGV", ARGV);

/**
 * Convert ruby procedure to javascript function object.
 */
Handle<Value> rubyproc2jsfunc(mrb_state*mrb, const mrb_value &v) {
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

/**
 * Get a NodeMRuby object instance from mrb_state*.
 */
Handle<Object> mrb2nmrb(mrb_state* mrb) {
    return reinterpret_cast<NodeMRubyUDContext*>(mrb->ud)->nmrb();
}

/*
 * Allocate static global variables.
 */
Persistent<FunctionTemplate> NodeMRuby::constructor_template;
Persistent<FunctionTemplate> NodeMRubyObject::constructor_template;
Persistent<FunctionTemplate> NodeMRubyMethod::constructor_template;
Persistent<FunctionTemplate> NodeMRubyFunctionInner::constructor_template;
Persistent<Function> NodeMRuby::require;
Persistent<Function> NodeMRuby::eval;
Persistent<Function> NodeMRuby::log;

extern "C" void init(Handle<Object> target) {
    DBG("Init");
    NodeMRuby::Init(target);
    NodeMRubyMethod::Init(target);
    NodeMRubyObject::Init(target);
    NodeMRubyFunctionInner::Init(target);
}

