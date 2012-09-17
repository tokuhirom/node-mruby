var mRuby = require('../index.js'),
    test = require('tap').test
    ;

test(function (t) {
    var server = (new mRuby).loadString('NodeJS.require("http")').createServer;
    server();
    t.equals(typeof(server), 'function');
    t.end();
});

