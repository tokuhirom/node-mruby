var test  = require('tap').test,
    here  = require('here').here,
    mRuby = require('../index.js');

var mruby = new mRuby();
mruby.loadString(here(/*
    tap = NodeJS.require('tap');
    mimi = NodeJS.require('./test/lib/mimi.js')
    tap.test {|t|
        mimi.r(mimi.obj, t)
        t.end()
    }
*/));

