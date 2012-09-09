#!/usr/bin/env node

var mRuby = require('../index.js'),
    test = require('tap').test;

test(function (t) {
    try {
        var mruby = new mRuby();
        mruby.loadString('NodeJS.require("fs"); 1');
        mruby.loadString('NodeJS.require("fs").readFileSync');
    } catch (e) { console.log(e); }
    t.end();
});

