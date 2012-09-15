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
  * node.js 0.8.8 or later

TODO
----

  * write a node.js library in ruby, and export classes/functions as npm library.
    * export mRuby's function/class to the node.js world?
  * better diag for syntax error in node.js level
  * inject object from node.js world to ruby world

BUGS
----

### SEGV

    mruby.loadString('p NodeJS.eval(%q!function x() { }; return x;!)');

### NodeMRubyObject::constructor\_template->GetFunction() does not works correctly after mrb\_load\_string\_cxt

Following code makes assertion error. I don't know why it caused.

        mrb_value result = mrb_load_string_cxt(MRB_, *src, CXT_);
        Local<Function> func = NodeMRubyObject::constructor_template->GetFunction();
        assert(*func);

It's not reproduce under node.js 0.8.8

### path for require

path for require is not based on .rb file.

### 'x'+3(mRuby bug?)

returns nil on mruby. Should it be exception?
