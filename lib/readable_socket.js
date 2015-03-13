var util = require('util')
  , Socket = require('./index').Socket;

var ReadableSocket = function (type) {
  var self = this;

  if (type !== 'sub' && type != 'xsub' && type != 'pull') {
    throw new Error('Unsupported readable socket type');
  }

  Socket.call(this, type);

  this._zmq.onReady = function() {
    self.emit('data');
  };
};

util.inherits(ReadableSocket, Socket);

ReadableSocket.prototype.send = function() {
  throw new Error('Can not send using ReadableSocket.');
};

ReadableSocket.prototype._flush = function() {
  return;
};

exports.ReadableSocket = ReadableSocket;

exports.createReadableSocket = function(type, options) {
  var sock = new ReadableSocket(type);
  for (var key in options) sock[key] = options[key];
  return sock;
};