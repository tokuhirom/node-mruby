var mruby = require('bindings')('mruby.node');
mruby.init(require, eval, console.log, function (nmrb, object, method) {
    return function () {
        return nmrb.callMethod(object, method, Array.prototype.slice.call(arguments));
    };
});
module.exports = mruby.mRuby;
