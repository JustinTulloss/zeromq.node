var util = require('util');
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

  var s = new binding.Socket(ctx, typecode);

  if (typeof(options) === 'object') {
    for (var key in options)
      s[key] = options[key];
  }

  return s;
};

var sockProp = function(name, option) {
  Socket.prototype.__defineGetter__(name, function() {
    return this.getsockopt(option);
  });
  Socket.prototype.__defineSetter__(name, function(value) {
    return this.setsockopt(option, value);
  });
};
sockProp('highwaterMark',     Socket.ZMQ_HWM);
sockProp('diskOffloadSize',   Socket.ZMQ_SWAP);
sockProp('identity',          Socket.ZMQ_IDENTITY);
sockProp('multicastDataRate', Socket.ZMQ_RATE);
sockProp('recoveryIVL',       Socket.ZMQ_RECOVERY_IVL);
sockProp('multicastLoop',     Socket.ZMQ_MCAST_LOOP);
sockProp('sendBufferSize',    Socket.ZMQ_SNDBUF);
sockProp('receiveBufferSize', Socket.ZMQ_RCVBUF);
sockProp('receiveMoreParts',  Socket.ZMQ_RCVMORE);
sockProp('currentEvents',     Socket.ZMQ_EVENTS);
