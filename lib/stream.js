var util = require('util')
  , Readable = require('stream').Readable
  , Writable = require('stream').Writable
  , Duplex = require('stream').Duplex;

var ReadableStream = function ReadableStream (socket, options) {
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

ReadableStream.prototype._pushMessages = function () {
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

ReadableStream.prototype._read = function () {
  this._pushMessages();
};

exports.ReadableStream = ReadableStream;


var WritableStream = function WritableStream (socket, options) {
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


var DuplexStream = function DuplexStream (socket, options) {
  var self = this;

  if (socket.type !== 'req' &&
      socket.type !== 'xreq' &&
      socket.type !== 'rep' &&
      socket.type !== 'xrep' &&
      socket.type !== 'dealer' &&
      socket.type !== 'router' &&
      socket.type !== 'pair') {
    throw new Error('Unsupported duplex socket type.');
  }

  if (socket.autoFlush !== false) {
    throw new Error('AutoFlush socket can not be used.');
  }

  options = options || {};

  this.socket = socket;

  options.readableObjectMode = true;
  options.writableObjectMode = true;
  Duplex.call(this, options);

  this._readMore = false;
  this.socket.on('ready', function () {
    // we don't want to read more right now
    if (!self._readMore) return;

    self._pushMessages();
  });
};

util.inherits(DuplexStream, Duplex);

DuplexStream.prototype._write = WritableStream.prototype._write;
DuplexStream.prototype._pushMessages = ReadableStream.prototype._pushMessages;
DuplexStream.prototype._read = ReadableStream.prototype._read;

exports.DuplexStream = DuplexStream;


exports.createStream = function createStream (socket, options) {
  var stream;

  switch (socket.type) {
    case 'pull':
    case 'sub':
    case 'xsub':
      stream = new ReadableStream(socket, options);
      break;
    case 'push':
    case 'pub':
    case 'xpub':
      stream = new WritableStream(socket, options);
      break;
    case 'req':
    case 'xreq':
    case 'rep':
    case 'xrep':
    case 'dealer':
    case 'router':
    case 'pair':
      stream = new DuplexStream(socket, options);
      break;
    default:
      throw new Error('Unsupported socket type.')
  }
  return stream;
};