var util = require('util')
  , Readable = require('stream').Readable
  , ReadableSocket = require('./readable_socket').ReadableSocket;

var ReadableStream = function (socket, options) {
  var self = this;

  if (!(socket instanceof ReadableSocket)) {
    throw new Error('Socket must be a ReadableSocket.');
  }
  options = options || {};

  this.socket = socket;

  options.objectMode = true;
  Readable.call(this, options);

  this._readMore = false;
  this.socket.on('data', function () {
    // we don't want to read more right now
    if (!self._readMore) return;

    self._pushMessages();
  });
};

util.inherits(ReadableStream, Readable);

ReadableStream.prototype._pushMessages = function() {
  var message;

  // Read as long as you can
  while (message = this.socket._read()) {
    // Stop reading if stream is paused
    if (!this.push(message)) {
      this._readMore = false;
      return;
    }
  }

  // We would like to read more, but there isn't any.
  this._readMore = true;
};

ReadableStream.prototype._read = function() {
  this._pushMessages();
};

exports.ReadableStream = ReadableStream;

exports.createReadableStream = function(socket, options) {
  var stream = new ReadableStream(socket, options);
  return stream;
};