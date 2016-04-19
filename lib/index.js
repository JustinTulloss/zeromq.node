/**
 * Module dependencies.
 */

var zmq = require('bindings')('zmq.node');
var context = require('./context');
var socket = require('./socket');

/**
 * Expose bindings.
 */

exports.zmq = zmq;

/**
 * Expose zmq version.
 */

exports.version = zmq.zmqVersion();

/**
 * Expose zmq_curve_keypair
 */

exports.curveKeypair = zmq.zmqCurveKeypair;

/**
 * Expose socket types.
 */

exports.types = socket.types;

/**
 * Context creation
 */

exports.context = exports.createContext = function (options) {
  return context.createContext(options);
};

/**
 * Exposing the default context
 */

exports.getDefaultContext = function () {
  return context.getDefaultContext();
};

/**
 * Socket creation, using a default context
 */

exports.socket = exports.createSocket = function (type, options) {
  return socket.createSocket(type, options, context.getDefaultContext());
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
