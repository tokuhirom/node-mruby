function Server() {
}
Server.prototype.on = function (func) {
  console.log(func);
};
module.exports = {
  obj: new Server(),
  r: function (svr) {
    svr.on(function () { });
  }
};
