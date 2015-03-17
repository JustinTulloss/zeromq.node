var util = require('util')
  , Readable = require('stream').Readable;

var ReadableStream = function (socket, options) {
  var self = this;

  if (socket.type !== 'sub' && socket.type != 'xsub' && socket.type != 'pull') {
    throw new Error('Unsupported readable socket type.');
  }

  if (socket.autoFlush !== false) {
    throw new Error('AutoFlush socket can not be used.');
  }

  options = options || {};

  this.socket = socket;

  options.objectMode = true;
  Readable.call(this, options);

  this._readMore = false;
  this.socket.on('ready', function () {
    // we don't want to read more right now
    if (!self._readMore) return;

    self._pushMessages();
  });
};

util.inherits(ReadableStream, Readable);

ReadableStream.prototype._pushMessages = function() {
  var message;

  // Read as long as you can
  while (message = this.socket.read()) {
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