#!/usr/bin/env node

var mRuby = require('../index.js'),
    test = require('tap').test;

test(function (t) {
    try {
        var mruby = new mRuby();
        t.equivalent(mruby.loadString('NodeJS.eval("false")'), false);
        t.equivalent(mruby.loadString('NodeJS.eval("true")'), true);
        t.equivalent(mruby.loadString('NodeJS.eval("undefined")'), null);
        t.equivalent(mruby.loadString('NodeJS.eval(%q{"xxx"})'), 'xxx');
        t.equivalent(mruby.loadString('NodeJS.eval(%q{[]})'), []);
        t.equivalent(mruby.loadString('NodeJS.eval(%q{[true,false]})'), [true,false]);
        t.equivalent(mruby.loadString('NodeJS.eval("4649")'), 4649);
        t.equivalent(mruby.loadString('NodeJS.eval("3.14")'), 3.14);
        t.equivalent(typeof(mruby.loadString('NodeJS.eval(%q!x={"1":2}!)')), 'object');
        t.equivalent(mruby.loadString('NodeJS.eval(%q!x={"a":[1,2,3]}!).a'), [1,2,3]);
        t.equivalent(mruby.loadString('NodeJS.eval(%q!function x(n) { return n*3; }; x!).call(5)'), 15);
    } catch (e) { console.log(e); }
    t.end();
});

