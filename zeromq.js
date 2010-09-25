var sys = require('sys');
var binding = exports.capi = require('./binding');

var context_ = null;
var defaultContext = function() {
  if (context_ !== null)
    return context_;

  var io_threads = 1;
  if (process.env.ZMQ_IO_THREADS) {
    io_threads = parseInt(process.env.ZMQ_IO_THREADS);
    if (!io_threads || io_threads < 1) {
      sys.error('Invalid number in ZMQ_IO_THREADS, using 1 IO thread.');
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

exports.createSocket = function(type) {
  var ctx = defaultContext();
  return new binding.Socket(ctx, type);
};
