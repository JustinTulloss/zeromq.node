/**
 * Module dependencies.
 */

var EventEmitter = require('events').EventEmitter
  , zmq = require('bindings')('zmq.node')
  , util = require('util');

/**
 * Expose bindings as the module.
 */

exports = module.exports = zmq;

/**
 * Expose zmq version.
 */

exports.version = zmq.zmqVersion();

/**
 * Expose zmq_curve_keypair
 */

exports.curveKeypair = zmq.zmqCurveKeypair;

/**
 * Map of socket types.
 */

var types = exports.types = {
    pub: zmq.ZMQ_PUB
  , xpub: zmq.ZMQ_XPUB
  , sub: zmq.ZMQ_SUB
  , xsub: zmq.ZMQ_XSUB
  , req: zmq.ZMQ_REQ
  , xreq: zmq.ZMQ_XREQ
  , rep: zmq.ZMQ_REP
  , xrep: zmq.ZMQ_XREP
  , push: zmq.ZMQ_PUSH
  , pull: zmq.ZMQ_PULL
  , dealer: zmq.ZMQ_DEALER
  , router: zmq.ZMQ_ROUTER
  , pair: zmq.ZMQ_PAIR
  , stream: zmq.ZMQ_STREAM
};

var longOptions = {
    ZMQ_HWM: 1
  , ZMQ_SWAP: 3
  , ZMQ_AFFINITY: 4
  , ZMQ_IDENTITY: 5
  , ZMQ_SUBSCRIBE: 6
  , ZMQ_UNSUBSCRIBE: 7
  , ZMQ_RATE: 8
  , ZMQ_RECOVERY_IVL: 9
  , ZMQ_MCAST_LOOP: 10
  , ZMQ_SNDBUF: 11
  , ZMQ_RCVBUF: 12
  , ZMQ_RCVMORE: 13
  , ZMQ_FD: 14
  , ZMQ_EVENTS: 15
  , ZMQ_TYPE: 16
  , ZMQ_LINGER: 17
  , ZMQ_RECONNECT_IVL: 18
  , ZMQ_BACKLOG: 19
  , ZMQ_RECOVERY_IVL_MSEC: 20
  , ZMQ_RECONNECT_IVL_MAX: 21
  , ZMQ_MAXMSGSIZE: 22
  , ZMQ_SNDHWM: 23
  , ZMQ_RCVHWM: 24
  , ZMQ_MULTICAST_HOPS: 25
  , ZMQ_RCVTIMEO: 27
  , ZMQ_SNDTIMEO: 28
  , ZMQ_IPV4ONLY: 31
  , ZMQ_LAST_ENDPOINT: 32
  , ZMQ_ROUTER_MANDATORY: 33
  , ZMQ_TCP_KEEPALIVE: 34
  , ZMQ_TCP_KEEPALIVE_CNT: 35
  , ZMQ_TCP_KEEPALIVE_IDLE: 36
  , ZMQ_TCP_KEEPALIVE_INTVL: 37
  , ZMQ_TCP_ACCEPT_FILTER: 38
  , ZMQ_DELAY_ATTACH_ON_CONNECT: 39
  , ZMQ_XPUB_VERBOSE: 40
  , ZMQ_ROUTER_RAW: 41
  , ZMQ_IPV6: 42
  , ZMQ_MECHANISM: 43
  , ZMQ_PLAIN_SERVER: 44
  , ZMQ_PLAIN_USERNAME: 45
  , ZMQ_PLAIN_PASSWORD: 46
  , ZMQ_CURVE_SERVER: 47
  , ZMQ_CURVE_PUBLICKEY: 48
  , ZMQ_CURVE_SECRETKEY: 49
  , ZMQ_CURVE_SERVERKEY: 50
  , ZMQ_ZAP_DOMAIN: 55
  , ZMQ_IO_THREADS: 1
  , ZMQ_MAX_SOCKETS: 2
};

Object.keys(longOptions).forEach(function(name){
  Object.defineProperty(zmq, name, {
    enumerable: true,
    configurable: false,
    writable: false,
    value: longOptions[name]
  });
});

/**
 * Map of socket options.
 */

var opts = exports.options = {
    _fd: zmq.ZMQ_FD
  , _ioevents: zmq.ZMQ_EVENTS
  , _receiveMore: zmq.ZMQ_RCVMORE
  , _subscribe: zmq.ZMQ_SUBSCRIBE
  , _unsubscribe: zmq.ZMQ_UNSUBSCRIBE
  , affinity: zmq.ZMQ_AFFINITY
  , backlog: zmq.ZMQ_BACKLOG
  , hwm: zmq.ZMQ_HWM
  , identity: zmq.ZMQ_IDENTITY
  , linger: zmq.ZMQ_LINGER
  , mcast_loop: zmq.ZMQ_MCAST_LOOP
  , rate: zmq.ZMQ_RATE
  , rcvbuf: zmq.ZMQ_RCVBUF
  , last_endpoint: zmq.ZMQ_LAST_ENDPOINT
  , reconnect_ivl: zmq.ZMQ_RECONNECT_IVL
  , recovery_ivl: zmq.ZMQ_RECOVERY_IVL
  , sndbuf: zmq.ZMQ_SNDBUF
  , swap: zmq.ZMQ_SWAP
  , mechanism: zmq.ZMQ_MECHANISM
  , plain_server: zmq.ZMQ_PLAIN_SERVER
  , plain_username: zmq.ZMQ_PLAIN_USERNAME
  , plain_password: zmq.ZMQ_PLAIN_PASSWORD
  , curve_server: zmq.ZMQ_CURVE_SERVER
  , curve_publickey: zmq.ZMQ_CURVE_PUBLICKEY
  , curve_secretkey: zmq.ZMQ_CURVE_SECRETKEY
  , curve_serverkey: zmq.ZMQ_CURVE_SERVERKEY
  , zap_domain: zmq.ZMQ_ZAP_DOMAIN
};

/**
 *  Monitor events
 */
var events = exports.events = {
    1:   "connect"       // zmq.ZMQ_EVENT_CONNECTED
  , 2:   "connect_delay" // zmq.ZMQ_EVENT_CONNECT_DELAYED
  , 4:   "connect_retry" // zmq.ZMQ_EVENT_CONNECT_RETRIED
  , 8:   "listen"        // zmq.ZMQ_EVENT_LISTENING
  , 16:  "bind_error"    // zmq.ZMQ_EVENT_BIND_FAILED
  , 32:  "accept"        // zmq.ZMQ_EVENT_ACCEPTED
  , 64:  "accept_error"  // zmq.ZMQ_EVENT_ACCEPT_FAILED
  , 128: "close"         // zmq.ZMQ_EVENT_CLOSED
  , 256: "close_error"   // zmq.ZMQ_EVENT_CLOSE_FAILED
  , 512: "disconnect"    // zmq.ZMQ_EVENT_DISCONNECTED
}

// Context management happens here. We lazily initialize a default context,
// and use that everywhere. Also cleans up on exit.
var ctx;
function defaultContext() {
  if (ctx) return ctx;

  var io_threads = 1;
  if (process.env.ZMQ_IO_THREADS) {
    io_threads = parseInt(process.env.ZMQ_IO_THREADS, 10);
    if (!io_threads || io_threads < 1) {
      console.warn('Invalid number in ZMQ_IO_THREADS, using 1 IO thread.');
      io_threads = 1;
    }
  }

  ctx = new zmq.Context(io_threads);
  process.on('exit', function(){
    // ctx.close();
    ctx = null;
  });

  return ctx;
};

/**
 * A batch consists of 1 or more message parts with their flags that need to be sent as one unit
 */

function OutBatch() {
  this.content = [];      // buf, flags, buf, flags, ...
  this.cbs = [];          // callbacks
  this.isClosed = false;  // true if the last message does not have SNDMORE in its flags, false otherwise
  this.next = null;       // next batch (for linked list of batches)
}

OutBatch.prototype.append = function (buf, flags, cb) {
  if (!Buffer.isBuffer(buf)) {
    buf = new Buffer(String(buf), 'utf8');
  }

  this.content.push(buf, flags);

  if (cb) {
    this.cbs.push(cb);
  }

  if ((flags & zmq.ZMQ_SNDMORE) === 0) {
    this.isClosed = true;
  }
};

OutBatch.prototype.invokeError = function (socket, error) {
  var returned = false;
  for (var i = 0; i < this.cbs.length; i += 1) {
    this.cbs[i].call(socket, error);
    returned = true;
  }

  if (!returned) {
    throw error;
  }
};

OutBatch.prototype.invokeSent = function (socket) {
  for (var i = 0; i < this.cbs.length; i += 1) {
    this.cbs[i].call(socket);
  }
};


function BatchList() {
  this.firstBatch = null;
  this.lastBatch = null;
  this.length = 0;
}

BatchList.prototype.canSend = function () {
  return this.firstBatch ? this.firstBatch.isClosed : false;
};

BatchList.prototype.append = function (buf, flags, cb) {
  var batch = this.lastBatch;

  if (!batch || batch.isClosed) {
    batch = new OutBatch();

    if (this.lastBatch) {
      this.lastBatch.next = batch;
    }

    this.lastBatch = batch;

    if (!this.firstBatch) {
      this.firstBatch = batch;
    }

    this.length += 1;
  }

  batch.append(buf, flags, cb);
};

BatchList.prototype.fetch = function () {
  var batch = this.firstBatch;
  if (batch && batch.isClosed) {
    this.firstBatch = batch.next;
    this.length -= 1;
    return batch;
  }
  return undefined;
};

BatchList.prototype.restore = function (batch) {
  this.firstBatch = batch;
  this.length += 1;
};


/**
 * Create a new socket of the given `type`.
 *
 * @constructor
 * @param {String|Number} type
 * @api public
 */

var Socket =
exports.Socket = function (type) {
  var self = this;
  EventEmitter.call(this);
  this.type = type;
  this._zmq = new zmq.SocketBinding(defaultContext(), types[type]);
  this._paused = false;
  this._isFlushingReads = false;
  this._isFlushingWrites = false;
  this._outgoing = new BatchList();

  this._zmq.onReady = function(readable, writable) {
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
  this._zmq.ref();
}

Socket.prototype.unref = function() {
  this._zmq.unref();
}

Socket.prototype.read = function() {
  var message = [], flags;

  if (this._zmq.state !== zmq.STATE_READY) {
    return null;
  }

  flags = this._zmq.getsockopt(zmq.ZMQ_EVENTS);

  if (flags & zmq.ZMQ_POLLIN) {
    do {
      message.push(this._zmq.recv());
    } while (this._zmq.getsockopt(zmq.ZMQ_RCVMORE));

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
  this._zmq.setsockopt(opts[opt] || opt, val);
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
  return this._zmq.getsockopt(opts[opt] || opt);
};

/**
 * Socket opt accessors allowing `sock.backlog = val`
 * instead of `sock.setsockopt('backlog', val)`.
 */

Object.keys(opts).forEach(function(name){
  Socket.prototype.__defineGetter__(name, function() {
    return this._zmq.getsockopt(opts[name]);
  });

  Socket.prototype.__defineSetter__(name, function(val) {
    if ('string' == typeof val) val = new Buffer(val, 'utf8');
    return this._zmq.setsockopt(opts[name], val);
  });
});

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
  this._zmq.bind(addr, function(err) {
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
  this._zmq.bindSync(addr);

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
    this._zmq.unbind(addr, function(err) {
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
    this._zmq.unbindSync(addr);
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
  this._zmq.connect(addr);
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
    this._zmq.disconnect(addr);
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
  if (zmq.ZMQ_CAN_MONITOR) {
    var self = this;

    self._zmq.onMonitorEvent = function(event_id, event_value, event_endpoint_addr) {
      self.emit(events[event_id], event_value, event_endpoint_addr);
    }

    self._zmq.onMonitorError = function(error) {
      self.emit('monitor_error', error);
    }

    this._zmq.monitor(interval, numOfEvents);
  } else {
    throw new Error('Monitoring support disabled check zmq version is > 3.2.1 and recompile this addon');
  }
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
    this._zmq.unmonitor();
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
  this._subscribe = filter;
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
  this._unsubscribe = filter;
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

Socket.prototype.send = function(msg, flags, cb) {
  flags = flags | 0;

  if (Array.isArray(msg)) {
    for (var i = 0, len = msg.length; i < len; i++) {
      var isLast = i === len - 1;
      var msgFlags = isLast ? flags : flags | zmq.ZMQ_SNDMORE;
      var callback = isLast ? cb : undefined;

      this._outgoing.append(msg[i], msgFlags, callback);
    }
  } else {
    this._outgoing.append(msg, flags, cb);
  }

  if (this._outgoing.canSend()) {
    this._zmq.pending = true;
    this._flushWrites();
  } else {
    this._zmq.pending = false;
  }

  return this;
};


Socket.prototype._flushRead = function () {
  var message = this._zmq.readv(); // can throw
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
    this._zmq.pending = false;
    return false;
  }

  try {
    if (this._zmq.sendv(batch.content)) {
      this._zmq.pending = this._outgoing.canSend();
      batch.invokeSent(this);
      return true;
    }

    this._outgoing.restore(batch);
    return false;
  } catch (sendError) {
    this._zmq.pending = this._outgoing.canSend();
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
  this._zmq.close();
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

exports.socket =
exports.createSocket = function(type, options) {
  var sock = new Socket(type);
  for (var key in options) sock[key] = options[key];
  return sock;
};

exports.Context.setMaxThreads = function(value) {
  if (!zmq.ZMQ_CAN_SET_CTX) {
    throw new Error('Setting of context options disabled, check zmq version is >= 3.2.1 and recompile this addon');
  }
  var defaultCtx = defaultContext();
  defaultCtx.setOpt(zmq.ZMQ_IO_THREADS, value);
};

exports.Context.getMaxThreads = function() {
  if (!zmq.ZMQ_CAN_SET_CTX) {
    throw new Error('Getting of context options disabled, check zmq version is >= 3.2.1 and recompile this addon');
  }
  var defaultCtx = defaultContext();
  return defaultCtx.getOpt(zmq.ZMQ_IO_THREADS);
};

exports.Context.setMaxSockets = function(value) {
  if (!zmq.ZMQ_CAN_SET_CTX) {
    throw new Error('Setting of context options disabled, check zmq version is >= 3.2.1 and recompile this addon');
  }
  var defaultCtx = defaultContext();
  defaultCtx.setOpt(zmq.ZMQ_MAX_SOCKETS, value);
};

exports.Context.getMaxSockets = function() {
  if (!zmq.ZMQ_CAN_SET_CTX) {
    throw new Error('Getting of context options disabled, check zmq version is >= 3.2.1 and recompile this addon');
  }
  var defaultCtx = defaultContext();
  return defaultCtx.getOpt(zmq.ZMQ_MAX_SOCKETS);
};

/**
 * JS based on API characteristics of the native zmq_proxy()
 */

function proxy (frontend, backend, capture){
  switch(frontend.type+'/'+backend.type){
    case 'push/pull':
    case 'pull/push':
    case 'xpub/xsub':
      if(capture){

        frontend.on('message',function (msg){
          backend.send(msg);
        });

        backend.on('message',function (msg){
          frontend.send(msg);

          //forwarding messages over capture socket
          capture.send(msg);
        });

      } else {

        //no capture socket provided, just forwarding msgs to respective sockets
        frontend.on('message',function (msg){
          backend.send(msg);
        });

        backend.on('message',function (msg){
          frontend.send(msg);
        });

      }
      break;
    case 'router/dealer':
    case 'xrep/xreq':
      if(capture){

        //forwarding router/dealer pack signature: id, delimiter, msg
        frontend.on('message',function (id,delimiter,msg){
          backend.send([id,delimiter,msg]);
        });

        backend.on('message',function (id,delimiter,msg){
          frontend.send([id,delimiter,msg]);

          //forwarding message to the capture socket
          capture.send(msg);
        });

      } else {

        //forwarding router/dealer signatures without capture
        frontend.on('message',function (id,delimiter,msg){
          backend.send([id,delimiter,msg]);
        });

        backend.on('message',function (id,delimiter,msg){
          frontend.send([id,delimiter,msg]);
        });

      }
      break;
    default:
      throw new Error('wrong socket order to proxy');
  }
}

exports.proxy = proxy;
