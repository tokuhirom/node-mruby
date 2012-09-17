process.argv = ["node", 'test/10_define_global_const.js', '-x', 'y'];

var mRuby = require('../index.js');
var here = require('here').here;

var mruby = new mRuby();
mruby.loadString(here(/*
NodeJS.require('tap').test {|t|
    t.equivalent(ARGV, ['-x', 'y'])
    t.end()
}
*/));

