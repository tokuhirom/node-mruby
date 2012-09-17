var mruby = require('bindings')('mruby.node');
mruby.init(require, eval, console.log, function (nmrb, object, method) {
    return function () {
        return nmrb.callMethod(object, method, Array.prototype.slice.call(arguments));
    };
}, process.argv.slice(2));
module.exports = mruby.mRuby;
