#!/usr/bin/env node

var mRuby = require('../index.js'),
    test = require('tap').test;

test(function (t) {
    var mruby = new mRuby();
    t.ok(mruby, 'mruby');
    t.equivalent(mruby.loadString('false'), false);
    t.equivalent(mruby.loadString('true'), true);
    t.equivalent(mruby.loadString('3'), 3);
    t.equivalent(typeof(mruby.loadString(':foo')), 'number');
    t.equivalent(mruby.loadString('3.14'), 3.14);
    t.equivalent(mruby.loadString('[3.14,5963]'), [3.14,5963]);
    t.equivalent(mruby.loadString('{}'), {});
    t.equivalent(mruby.loadString('"hoge"'), 'hoge');
    t.equivalent(typeof(mruby.loadString('1..10')), 'object');
    t.ok(mruby.loadString('1..50').inspect().match(/^#<Range:/), 'range');
    t.end();
});

