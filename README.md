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
  * node.js 0.8.9 or later

BUGS
----

### path for require

path for require is not based on .rb file.

mRuby LIMITATIONS
-----------------

### 'x'+3(mRuby bug?)

returns nil on mruby. Should it be exception?

### heredocs

mruby doesn't work with heredocs?

