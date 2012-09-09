#!/usr/bin/env node

var mRuby = require('../index.js'),
    heredoc = require('here').here;

var mruby = new mRuby();
mruby.loadString(heredoc(/*
    tap = NodeJS.require('tap')
    fs = NodeJS.require('fs')
    tap.test {|t|
        fs.readFile('hoge.js', 'utf-8') {|err, content|
            t.ok(content.index('node') > 10)
            t.end()
        }
    }
*/));

