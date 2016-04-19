var EventEmitter = require('events').EventEmitter;
var util = require('util');
var zmq = require('bindings')('zmq.node');
var BatchList = require('./BatchList');
var context = require('./context');

/**
 * Map of socket options in both "ZMQ_LAST_ENDPOINT" and "last_endpoint" style.
 */

var opts = {};
Object.keys(zmq.options).forEach(function (name) {
  var shortName = name.replace(/^ZMQ_/, '').toLowerCase();

  opts[name] = opts[shortName] = zmq.options[name];
});

/**
 * Map of socket types in both "ZMQ_ROUTER" and "router" style
 */

var types = {};
Object.keys(zmq.types).forEach(function (name) {
  var shortName = name.replace(/^ZMQ_/, '').toLowerCase();

  types[name] = types[shortName] = zmq.types[name];
});

/**
 * Map of send flags in both "ZMQ_SNDMORE" and "sndmore" style
 */

var sendFlags = {};
Object.keys(zmq.sendFlags).forEach(function (name) {
  var shortName = name.replace(/^ZMQ_/, '').toLowerCase();

  sendFlags[name] = sendFlags[shortName] = zmq.sendFlags[name];
});

function flagsFromArray(flags) {
  var result = 0;
  for (var i = 0; i < flags.length; i += 1) {
    var flag = flags[i];
    if (!sendFlags.hasOwnProperty(flag)) {
      throw new Error('Unknown flag name: ' + flag);
    }

    result |= sendFlags[flag];
  }
  return result;
}

/**
 * Monitor events
 */

var events = exports.events = {
  1:  'connect',       // ZMQ_EVENT_CONNECTED
  2:  'connect_delay', // ZMQ_EVENT_CONNECT_DELAYED
  4:  'connect_retry', // ZMQ_EVENT_CONNECT_RETRIED
  8:  'listen',        // ZMQ_EVENT_LISTENING
  16: 'bind_error',    // ZMQ_EVENT_BIND_FAILED
  32: 'accept',        // ZMQ_EVENT_ACCEPTED
  64: 'accept_error',  // ZMQ_EVENT_ACCEPT_FAILED
  128: 'close',        // ZMQ_EVENT_CLOSED
  256: 'close_error',  // ZMQ_EVENT_CLOSE_FAILED
  512: 'disconnect'    // ZMQ_EVENT_DISCONNECTED
};

/**
 * Create a new socket of the given `type`.
 *
 * @constructor
 * @param {String|Number} type
 * @api public
 */

function Socket(type, ctx) {
  if (typeof type !== 'string') {
    throw new TypeError('Type argument must be a string');
  }

  if (!types.hasOwnProperty(type)) {
    throw new Error('Socket type "' + type + '" not supported by ZMQ');
  }

  if (!(ctx instanceof context.Context)) {
    throw new TypeError('Context is invalid: ' + ctx);
  }

  var self = this;
  EventEmitter.call(this);
  this.type = type;
  this._sock = new zmq.SocketBinding(ctx._ctx, types[type]);
  this._paused = false;
  this._isFlushingReads = false;
  this._isFlushingWrites = false;
  this._outgoing = new BatchList();

  this._sock.onReady = function(readable, writable) {
    if (readable) self._flushReads();
    if (writable) self._flushWrites();
  };
};

/**
 * Inherit from `EventEmitter.prototype`.
 */

util.inherits(Socket, EventEmitter);

/**
 * Set socket to pause mode
 * no data will be emit until resume() is called
 * all send() calls will be queued
 *
 * @api public
 */

Socket.prototype.pause = function() {
  this._paused = true;
}

/**
 * Set a socket back to normal work mode
 *
 * @api public
 */

Socket.prototype.resume = function() {
  this._paused = false;
  this._flushReads();
  this._flushWrites();
}

Socket.prototype.ref = function() {
  this._sock.ref();
}

Socket.prototype.unref = function() {
  this._sock.unref();
}

Socket.prototype.read = function() {
  var message = [], flags;

  if (this._sock.state !== zmq.STATE_READY) {
    return null;
  }

  flags = this._sock.getsockopt(opts.ZMQ_EVENTS);

  if (flags & zmq.ZMQ_POLLIN) {
    do {
      message.push(this._sock.recv());
    } while (this._sock.getsockopt(opts.ZMQ_RCVMORE));

    return message;
  }

  return null;
}


/**
 * Set `opt` to `val`.
 *
 * @param {String|Number} opt
 * @param {Mixed} val
 * @return {Socket} for chaining
 * @api public
 */

Socket.prototype.setsockopt = function(opt, val){
  if (typeof val === 'string') {
    val = new Buffer(val, 'utf8');
  }

  this._sock.setsockopt(opts[opt] || opt, val);
  return this;
};

/**
 * Get socket `opt`.
 *
 * @param {String|Number} opt
 * @return {Mixed}
 * @api public
 */

Socket.prototype.getsockopt = function(opt){
  return this._sock.getsockopt(opts[opt] || opt);
};

/**
 * Async bind.
 *
 * Emits the "bind" event.
 *
 * @param {String} addr
 * @param {Function} cb
 * @return {Socket} for chaining
 * @api public
 */

Socket.prototype.bind = function(addr, cb) {
  var self = this;
  this._sock.bind(addr, function(err) {
    if (err) {
      return cb && cb(err);
    }

    self._flushReads();
    self._flushWrites();

    self.emit('bind', addr);
    cb && cb();
  });
  return this;
};

/**
 * Sync bind.
 *
 * @param {String} addr
 * @return {Socket} for chaining
 * @api public
 */

Socket.prototype.bindSync = function(addr) {
  this._sock.bindSync(addr);

  return this;
};

/**
 * Async unbind.
 *
 * Emits the "unbind" event.
 *
 * @param {String} addr
 * @param {Function} cb
 * @return {Socket} for chaining
 * @api public
 */

Socket.prototype.unbind = function(addr, cb) {
  if (zmq.ZMQ_CAN_UNBIND) {
    var self = this;
    this._sock.unbind(addr, function(err) {
      if (err) {
        return cb && cb(err);
      }
      self.emit('unbind', addr);

      self._flushReads();
      self._flushWrites();
      cb && cb();
    });
  } else {
    cb && cb();
  }
  return this;
};

/**
 * Sync unbind.
 *
 * @param {String} addr
 * @return {Socket} for chaining
 * @api public
 */

Socket.prototype.unbindSync = function(addr) {
  if (zmq.ZMQ_CAN_UNBIND) {
    this._sock.unbindSync(addr);
  }
  return this;
}

/**
 * Connect to `addr`.
 *
 * @param {String} addr
 * @return {Socket} for chaining
 * @api public
 */

Socket.prototype.connect = function(addr) {
  this._sock.connect(addr);
  return this;
};

/**
 * Disconnect from `addr`.
 *
 * @param {String} addr
 * @return {Socket} for chaining
 * @api public
 */

Socket.prototype.disconnect = function(addr) {
  if (zmq.ZMQ_CAN_DISCONNECT) {
    this._sock.disconnect(addr);
  }
  return this;
};

/**
 * Enable monitoring of a Socket
 *
 * @param {Number} timer interval in ms > 0 or Undefined for default
 * @param {Number} The maximum number of events to read on each interval, default is 1, use 0 for reading all events
 * @return {Socket} for chaining
 * @api public
 */

Socket.prototype.monitor = function(interval, numOfEvents) {
  if (!zmq.ZMQ_CAN_MONITOR) {
    throw new Error('Monitoring support disabled check zmq version is > 3.2.1 and recompile this addon');
  }

  var self = this;

  self._sock.onMonitorEvent = function (event_id, event_value, event_endpoint_addr) {
    self.emit(events[event_id], event_value, event_endpoint_addr);
  }

  self._sock.onMonitorError = function (error) {
    self.emit('monitor_error', error);
  }

  this._sock.monitor(interval, numOfEvents);
  return this;
};

/**
 * Disable monitoring of a Socket release idle handler
 * and close the socket
 *
 * @return {Socket} for chaining
 * @api public
 */

Socket.prototype.unmonitor = function() {
  if (zmq.ZMQ_CAN_MONITOR) {
    this._sock.unmonitor();
  }
  return this;
};


/**
 * Subscribe with the given `filter`.
 *
 * @param {String} filter
 * @return {Socket} for chaining
 * @api public
 */

Socket.prototype.subscribe = function(filter) {
  this.setsockopt(opts.ZMQ_SUBSCRIBE, filter);
  return this;
};

/**
 * Unsubscribe with the given `filter`.
 *
 * @param {String} filter
 * @return {Socket} for chaining
 * @api public
 */

Socket.prototype.unsubscribe = function(filter) {
  this.setsockopt(opts.ZMQ_UNSUBSCRIBE, filter);
  return this;
};

/**
 * Send the given `msg`.
 *
 * @param {String|Buffer|Array} msg
 * @param {Number} [flags]
 * @param {Function} [cb]
 * @return {Socket} for chaining
 * @api public
 */

Socket.prototype.send = function (msg, flags, cb) {
  if (typeof flags === 'string') {
    flags = sendFlags[flags] | 0;
  } else if (Array.isArray(flags)) {
    flags = flagsFromArray(flags);
  } else {
    flags = flags | 0;
  }

  if (Array.isArray(msg)) {
    for (var i = 0, len = msg.length; i < len; i++) {
      var isLast = i === len - 1;
      var msgFlags = isLast ? flags : flags | sendFlags.ZMQ_SNDMORE;
      var callback = isLast ? cb : undefined;

      this._outgoing.append(msg[i], msgFlags, callback);
    }
  } else {
    this._outgoing.append(msg, flags, cb);
  }

  if (this._outgoing.canSend()) {
    this._sock.pending = true;
    this._flushWrites();
  } else {
    this._sock.pending = false;
  }

  return this;
};


Socket.prototype._flushRead = function () {
  var message = this._sock.readv(); // can throw
  if (!message) {
    return false;
  }

  // Handle received message immediately to prevent memory leak in driver
  if (message.length === 1) {
    // hot path
    this.emit('message', message[0]);
  } else {
    this.emit.apply(this, ['message'].concat(message));
  }
  return true;
};

Socket.prototype._flushWrite = function () {
  var batch = this._outgoing.fetch();
  if (!batch) {
    this._sock.pending = false;
    return false;
  }

  try {
    if (this._sock.sendv(batch.content)) {
      this._sock.pending = this._outgoing.canSend();
      batch.invokeSent(this);
      return true;
    }

    this._outgoing.restore(batch);
    return false;
  } catch (sendError) {
    this._sock.pending = this._outgoing.canSend();
    batch.invokeError(this, sendError); // can throw
    return false;
  }
};


Socket.prototype._flushReads = function() {
  if (this._paused || this._isFlushingReads) return;

  this._isFlushingReads = true;

  var received;

  do {
    try {
      received = this._flushRead();
    } catch (error) {
      this._isFlushingReads = false;
      this.emit('error', error); // can throw
      return;
    }
  } while (received);

  this._isFlushingReads = false;
};

Socket.prototype._flushWrites = function() {
  if (this._paused || this._isFlushingWrites) return;

  this._isFlushingWrites = true;

  var sent;

  do {
    try {
      sent = this._flushWrite();
    } catch (error) {
      this._isFlushingWrites = false;
      this.emit('error', error); // can throw
      return;
    }
  } while (sent);

  this._isFlushingWrites = false;
};

/**
 * Close the socket.
 *
 * @return {Socket} for chaining
 * @api public
 */

Socket.prototype.close = function() {
  this._sock.close();
  return this;
};

/**
 * Create a `type` socket with the given `options`.
 *
 * @param {String} type
 * @param {Object} options
 * @return {Socket}
 * @api public
 */

exports.createSocket = function (type, options, ctx) {
  var sock = new Socket(type, ctx);

  if (options) {
    for (var key in options) {
      sock.setsockopt(key, options[key]);
    }
  }

  return sock;
};


/**
 * Expose Socket class and constants
 */

exports.Socket = Socket;
exports.opts = opts;
exports.types = types;
exports.sendFlags = sendFlags;
