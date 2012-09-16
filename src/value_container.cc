#include "node-mruby.h"

static void node_mruby_value_container_free(mrb_state * mrb, void* data) {
    TRACE_DESTRUCTOR("Ruby#NodeMRubyValueContainer");
    struct NodeMRubyValueContainer* d = static_cast<struct NodeMRubyValueContainer*>(data);
    delete d;
}

const struct mrb_data_type node_mruby_function_data_type = {
    "mruby_function", node_mruby_value_container_free
};
