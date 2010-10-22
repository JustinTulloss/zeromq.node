var util = require('util');
var binding = exports.capi = require('./binding');

var namemap = (function() {
  var s = binding.Socket;
  var m = {};
  m.pub  = m.publish   = m.publisher  = s.ZMQ_PUB;
  m.sub  = m.subscribe = m.subscriber = s.ZMQ_SUB;
  m.req  = m.request   = m.requester  = s.ZMQ_REQ;
  m.xreq = m.xrequest  = m.xrequester = s.ZMQ_XREQ;
  m.rep  = m.reply     = m.replier    = s.ZMQ_REP;
  m.xrep = m.xreply    = m.xreplier   = s.ZMQ_XREP;
  m.push = s.ZMQ_PUSH;
  m.pull = s.ZMQ_PULL;
  m.pair = s.ZMQ_PAIR;
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
