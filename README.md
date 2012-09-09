node-mruby
==========

node-mruby is a extension library for node.js. It embeds mRuby into node.js.

node-mruby is...
----------------

  * You can call a mruby as a library

preqreuirements
---------------

  * bison?
  * make?

TODO
----

  * write a node.js library in ruby, and export classes/functions as npm library.
  * use callback function

BUGS
----

### SEGV

    mruby.loadString('p NodeJS.eval(%q!function x() { }; return x;!)');

### NodeMRubyObject::constructor\_template->GetFunction() does not works correctly after mrb\_load\_string\_cxt

Following code makes assertion error. I don't know why it caused.

        mrb_value result = mrb_load_string_cxt(MRB_, *src, CXT_);
        Local<Function> func = NodeMRubyObject::constructor_template->GetFunction();
        assert(*func);

