var mruby = require('./build/Release/mruby.node');
mruby.init(require, eval);
module.exports = mruby.mRuby;
