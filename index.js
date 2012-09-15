var mruby = require('./build/Debug/mruby.node');
mruby.init(require, eval, console.log);
module.exports = mruby.mRuby;
