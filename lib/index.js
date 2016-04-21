/**
 * Module dependencies.
 */

var binding = require('bindings')('zmq.node');
var context = require('./context');
var socket = require('./socket');

/**
 * Expose bindings.
 */

exports.binding = binding;

/**
 * Expose zmq version.
 */

exports.version = binding.zmqVersion();

/**
 * Expose zmq_curve_keypair
 */

exports.curveKeypair = binding.zmqCurveKeypair;

/**
 * Context creation
 *
 * @return {Context}
 */

exports.context = exports.createContext = function (options) {
  return context.createContext(options);
};

/**
 * Exposing the default context
 *
 * @return {Context}
 */

exports.getDefaultContext = function () {
  return context.getDefaultContext();
};

/**
 * Socket creation, using a default context
 *
 * @return {Socket}
 */

exports.socket = exports.createSocket = function (type, options) {
  return socket.createSocket(type, options, context.getDefaultContext());
};

/**
 * Tests if a context option exists in this version of ZMQ
 *
 * @return {Boolean}
 */

exports.contextOptionExists = function (option) {
  return context.opts.hasOwnProperty(option);
};

/**
 * Tests if a socket option exists in this version of ZMQ
 *
 * @return {Boolean}
 */

exports.socketOptionExists = function (option) {
  return socket.opts.hasOwnProperty(option);
};

/**
 * Tests if a socket type exists in this version of ZMQ
 *
 * @return {Boolean}
 */

exports.socketTypeExists = function (type) {
  return socket.types.hasOwnProperty(type);
};

/**
 * JS proxy method based on API characteristics of the native zmq_proxy()
 */

function proxy(frontend, backend, capture) {
  switch (frontend.type + '/' + backend.type) {
    case 'push/pull':
    case 'pull/push':
    case 'xpub/xsub':
      if (capture) {
        frontend.on('message', function (msg) {
          backend.send(msg);
        });

        backend.on('message', function (msg) {
          frontend.send(msg);

          //forwarding messages over capture socket
          capture.send(msg);
        });
      } else {
        //no capture socket provided, just forwarding msgs to respective sockets
        frontend.on('message', function (msg) {
          backend.send(msg);
        });

        backend.on('message', function (msg) {
          frontend.send(msg);
        });
      }
      break;
    case 'router/dealer':
    case 'xrep/xreq':
      if (capture){
        // forwarding router/dealer pack signature: id, delimiter, msg
        frontend.on('message', function (id, delimiter, msg){
          backend.send([id, delimiter, msg]);
        });

        backend.on('message', function (id, delimiter, msg){
          frontend.send([id, delimiter, msg]);

          //forwarding message to the capture socket
          capture.send(msg);
        });
      } else {
        //forwarding router/dealer signatures without capture
        frontend.on('message', function (id, delimiter, msg){
          backend.send([id, delimiter, msg]);
        });

        backend.on('message', function (id, delimiter, msg){
          frontend.send([id, delimiter, msg]);
        });
      }
      break;
    default:
      throw new Error('wrong socket order to proxy');
  }
}

exports.proxy = proxy;
