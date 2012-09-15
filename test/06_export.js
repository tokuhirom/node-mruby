#!/usr/bin/env node
"use strict";

var mRuby = require('../index.js'),
    test = require('tap').test,
    heredoc = require('here').here;

test('class method', function (t) {
    var mruby = new mRuby();
    var klass = mruby.loadString(heredoc(/*
        class Foo
            def Foo.yay
                return 5963
            end
        end

        Foo
    */));
    t.equivalent(klass.yay(), 5963);
    t.end();
});

test('instance method', function (t) {
    var mruby = new mRuby();
    var klass = mruby.loadString(heredoc(/*
        class Foo
            def hey
                return 4649
            end
            def square(n)
                n*n
            end
        end

        Foo.new()
    */));
    t.equivalent(klass.hey(), 4649);
    t.equivalent(klass.square(8), 64);
    t.end();
});

