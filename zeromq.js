var util = require('util');
var IOWatcher = process.binding('io_watcher').IOWatcher;
var binding = exports.capi = require('./binding');
var Socket = binding.Socket;

var namemap = (function() {
  var m = {};
  m.pub  = m.publish   = m.publisher  = Socket.ZMQ_PUB;
  m.sub  = m.subscribe = m.subscriber = Socket.ZMQ_SUB;
  m.req  = m.request   = m.requester  = Socket.ZMQ_REQ;
  m.xreq = m.xrequest  = m.xrequester = Socket.ZMQ_XREQ;
  m.rep  = m.reply     = m.replier    = Socket.ZMQ_REP;
  m.xrep = m.xreply    = m.xreplier   = Socket.ZMQ_XREP;
  m.push = m.pusher    = Socket.ZMQ_PUSH;
  m.pull = m.puller    = Socket.ZMQ_PULL;
  m.pair = Socket.ZMQ_PAIR;
  return m;
})();

var context_ = null;
var defaultContext = function() {
  if (context_ !== null)
    return context_;

  var io_threads = 1;
  if (process.env.ZMQ_IO_THREADS) {
    io_threads = parseInt(process.env.ZMQ_IO_THREADS);
    if (!io_threads || io_threads < 1) {
      util.error('Invalid number in ZMQ_IO_THREADS, using 1 IO thread.');
      io_threads = 1;
    }
  }

  context_ = new binding.Context(io_threads);
  process.on('exit', function() {
    context_.close();
    context_ = null;
  });

  return context_;
};

exports.createSocket = function(typename, options) {
  var ctx = defaultContext();

  var typecode = typename;
  if (typeof(typecode) !== 'number') {
    typecode = namemap[typename];
    if (!namemap.hasOwnProperty(typename) || typecode === undefined)
      throw new TypeError("Unknown socket type: " + typename);
  }

  var sock = new binding.Socket(ctx, typecode);
  sock.type = typename;
  sock._outgoing = [];
  sock.watcher = new IOWatcher();
  sock.watcher.callback = function() { sock._flush(); };
  sock.watcher.set(sock._fd, true, false);
  sock.watcher.start();

  if (typeof(options) === 'object') {
    for (var key in options)
      sock[key] = options[key];
  }

  return sock;
};

var sockProp = function(name, option) {
  Socket.prototype.__defineGetter__(name, function() {
    return this.getsockopt(option);
  });
  Socket.prototype.__defineSetter__(name, function(value) {
    return this.setsockopt(option, value);
  });
};
sockProp('_fd',               Socket.ZMQ_FD);
sockProp('_ioevents',         Socket.ZMQ_EVENTS);
sockProp('_receiveMore',      Socket.ZMQ_RCVMORE);
sockProp('_subscribe',        Socket.ZMQ_SUBSCRIBE);
sockProp('_unsubscribe',      Socket.ZMQ_UNSUBSCRIBE);
sockProp('ioThreadAffinity',  Socket.ZMQ_AFFINITY);
sockProp('backlog',           Socket.ZMQ_BACKLOG);
sockProp('highWaterMark',     Socket.ZMQ_HWM);
sockProp('identity',          Socket.ZMQ_IDENTITY);
sockProp('lingerPeriod',      Socket.ZMQ_LINGER);
sockProp('multicastLoop',     Socket.ZMQ_MCAST_LOOP);
sockProp('multicastDataRate', Socket.ZMQ_RATE);
sockProp('receiveBufferSize', Socket.ZMQ_RCVBUF);
sockProp('reconnectInterval', Socket.ZMQ_RECONNECT_IVL);
sockProp('multicastRecovery', Socket.ZMQ_RECOVERY_IVL);
sockProp('sendBufferSize',    Socket.ZMQ_SNDBUF);
sockProp('diskOffloadSize',   Socket.ZMQ_SWAP);

Socket.prototype.subscribe = function(filter) {
  this._subscribe = filter;
};

Socket.prototype.unsubscribe = function(filter) {
  this._unsubscribe = filter;
};

Socket.prototype.send = function() {
  var i, length = arguments.length,
      parts = new Array(length);
  for (i = 0; i < length; i++) {
    var part = arguments[i];
    if (typeof(part) === 'string')
      part = new Buffer(part, 'utf-8');
    var flags = 0;
    if (i !== length-1)
      flags |= Socket.ZMQ_SNDMORE;
    parts[i] = [part, flags];
  }
  this._outgoing = this._outgoing.concat(parts);
  this._flush();
};

Socket.prototype._flush = function() {
  try {
    while (this._ioevents & Socket.ZMQ_POLLIN) {
      var emitArgs = ['message'];
      do {
        emitArgs.push(this._recv());
      } while (this._receiveMore);
      this.emit.apply(this, emitArgs);
    }

    while (this._outgoing.length && (this._ioevents & Socket.ZMQ_POLLOUT)) {
      var sendArgs = this._outgoing.shift();
      this._send.apply(this, sendArgs);
    }

  }
  catch (e) {
    this.emit('error', e);
  }
};

Socket.prototype.close = function() {
  this.watcher.stop();
  this.watcher = undefined;
  this._close();
};
