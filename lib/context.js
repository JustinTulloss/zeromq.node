var binding = require('bindings')('zmq.node');
var socket = require('./socket');


/**
 * Map of context options in both "ZMQ_IO_THREADS" and "io_threads" style.
 */

var opts = {};
Object.keys(binding.ctxOptions).forEach(function (name) {
  var shortName = name.replace(/^ZMQ_/, '').toLowerCase();

  opts[name] = opts[shortName] = binding.ctxOptions[name];
});

exports.opts = opts;


function Context() {
  this._ctx = new binding.Context();
}

Context.prototype.set = function (option, value) {
  return this._ctx.set(opts[option], value);
};

Context.prototype.get = function (option) {
  return this._ctx.get(opts[option]);
};

Context.prototype.createSocket = function (type, options) {
  return socket.createSocket(type, options, this);
};

Context.prototype.socket = Context.prototype.createSocket;


exports.createContext = function (options) {
  var context = new Context();

  if (options) {
    for (var key in options) {
      context.set(key, options[key]);
    }
  }

  return context;
};


var defaultContext;

exports.getDefaultContext = function () {
  if (!defaultContext) {
    defaultContext = new Context();

    if (process.env.ZMQ_IO_THREADS) {
      defaultContext.set('ZMQ_IO_THREADS', parseInt(process.env.ZMQ_IO_THREADS, 10));
    }
  }

  return defaultContext;
};


/**
 * Expose the Context class
 */

exports.Context = Context;
