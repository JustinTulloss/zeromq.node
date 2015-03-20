var util = require('util')
  , Writable = require('stream').Writable;

var WritableStream = function (socket, options) {
  var self = this;

  if (socket.type !== 'pub' && socket.type != 'xpub' && socket.type != 'push') {
    throw new Error('Unsupported writable socket type.');
  }

  if (socket.autoFlush !== false) {
    throw new Error('AutoFlush socket can not be used.');
  }

  options = options || {};

  this.socket = socket;

  options.objectMode = true;
  Writable.call(this, options);
};

util.inherits(WritableStream, Writable);

WritableStream.prototype._write = function(parts, encoding, callback) {
  this.socket.write(parts, callback);
};

exports.WritableStream = WritableStream;

exports.createWritableStream = function(socket, options) {
  var stream = new WritableStream(socket, options);
  return stream;
};