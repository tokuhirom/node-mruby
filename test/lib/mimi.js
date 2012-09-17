function Server() {
}
Server.prototype.on = function (func, t) {
    t.equals(typeof func, 'function');
};
module.exports = {
    obj: new Server(),
    r: function (svr, t) {
        svr.on(function () { }, t);
    }
};
