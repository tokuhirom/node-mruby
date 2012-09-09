#!/usr/bin/env node

var mRuby = require('../index.js'),
    test = require('tap').test;

test(function (t) {
    try {
        var mruby = new mRuby();
        t.ok(mruby.loadString('NodeJS.require("fs").readFileSync("hoge.js", "utf-8")').match(/node/));
    } catch (e) { console.log(e); t.fail(); }
    t.end();
});

