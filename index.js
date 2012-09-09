var mruby = require('./build/Release/mruby.node');
mruby.init(require, eval, console.log);
module.exports = mruby.mRuby;
