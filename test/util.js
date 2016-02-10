// cleanup utils for sockets
var sockets = [];

exports.cleanup = function (done) {
  while (sockets.length) {
    sockets.pop().close();
  }
  // give underlying sockets some time to close
  // this shouldn't be necessary on *ix, but it seems to be
  setTimeout(function () {
    done();
  }, 500);
};

exports.push_sockets = function () {
  sockets.push.apply(sockets, arguments);
};

exports.done_countdown = function (done, counter) {
  // add a done-countdown, so multiple async events can be awaited before triggering done
  return function () {
    counter -= 1;
    if (counter === 0) {
      done();
    }
  }
};