#!/usr/bin/env node

var mRuby = require('../index.js'),
    test = require('tap').test;

test(function (t) {
    var mruby = new mRuby();
    t.equivalent(mruby.loadFile(__dirname + "/rb/01_true.rb"), true);
    t.equivalent(mruby.loadFile(__dirname + "/rb/02_5963.rb"), 5963);
    t.end();
});


