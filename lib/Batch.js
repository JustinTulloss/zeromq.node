var ZMQ_SNDMORE = require('bindings')('zmq.node').sendFlags.ZMQ_SNDMORE;


/**
 * A batch consists of 1 or more message parts with their flags that need to be sent as one unit
 */

function Batch() {
  this.content = [];      // buf, flags, buf, flags, ...
  this.cbs = [];          // callbacks
  this.isClosed = false;  // true if the last message does not have SNDMORE in its flags, false otherwise
  this.next = null;       // next batch (for linked list of batches)
}

module.exports = Batch;


Batch.prototype.append = function (buf, flags, cb) {
  if (!Buffer.isBuffer(buf)) {
    buf = new Buffer(String(buf), 'utf8');
  }

  this.content.push(buf, flags);

  if (cb) {
    this.cbs.push(cb);
  }

  if ((flags & ZMQ_SNDMORE) === 0) {
    this.isClosed = true;
  }
};

Batch.prototype.invokeError = function (socket, error) {
  var returned = false;
  for (var i = 0; i < this.cbs.length; i += 1) {
    this.cbs[i].call(socket, error);
    returned = true;
  }

  if (!returned) {
    throw error;
  }
};

Batch.prototype.invokeSent = function (socket) {
  for (var i = 0; i < this.cbs.length; i += 1) {
    this.cbs[i].call(socket);
  }
};
