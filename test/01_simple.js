var mRuby = require('../index.js'),
    test = require('tap').test;

test(function (t) {
  var mruby = new mRuby();
  t.ok(mruby);
  t.equivalent(mruby.run('false'), false);
  t.equivalent(mruby.run('true'), true);
  t.equivalent(mruby.run('3'), 3);
  t.equivalent(typeof(mruby.run(':foo')), 'number');
  t.equivalent(mruby.run('3.14'), 3.14);
  t.equivalent(mruby.run('[3.14,5963]'), [3.14,5963]);
  t.equivalent(mruby.run('{}'), {});
  t.equivalent(mruby.run('"hoge"'), 'hoge');
  t.equivalent(typeof(mruby.run('1..10')), 'object');
  t.ok(mruby.run('1..10').inspect().match(/^#<Range:/));
  t.end();
});

