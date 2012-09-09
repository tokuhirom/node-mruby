#!/usr/bin/env node

var mRuby = require('../index.js'),
    heredoc = require('here').here;

var mruby = new mRuby();
mruby.loadString(heredoc(/*
    tap = NodeJS.require('tap')
    fs = NodeJS.require('fs')
    tap.test {|t|
        fs.readFile('FNAME', 'utf-8') {|err, content|
            t.ok(content.index('node') > 10)
            t.end()
        }
    }
*/).replace(/FNAME/, __dirname + "/05_proc.js"));

