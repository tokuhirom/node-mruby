#!/usr/bin/env node

var mRuby = require('../index.js'),
    test = require('tap').test;

test(function (t) {
    var mruby = new mRuby();
    t.ok(mruby.loadString('NodeJS.require("fs").readFileSync("' + __dirname + '/../index.js", "utf-8")').match(/node/));
    t.end();
});

