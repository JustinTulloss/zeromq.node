// This is mainly for testing that the security mechanisms themselves are working
// not the ZAP protocol itself.  As long as the request is valid, this will 
// authenticate it.

var zmq = require('../');

module.exports.start = function(count) {
  var zap = zmq.socket('router');
  zap.on('message', function() {
    var data = Array.prototype.slice.call(arguments);
  
    if (!data || !data.length) throw new Error("Invalid ZAP request");
  
    var returnPath = [],
      frame = data.shift();
    while (frame && (frame.length != 0)) {
      returnPath.push(frame);
      frame = data.shift();
    }
    returnPath.push(frame);

    if (data.length < 6) throw new Error("Invalid ZAP request");

    var zapReq = {
      version: data.shift(),
      requestId: data.shift(),
      domain: new Buffer(data.shift()).toString('utf8'),
      address: new Buffer(data.shift()).toString('utf8'),
      identity: new Buffer(data.shift()).toString('utf8'),
      mechanism: new Buffer(data.shift()).toString('utf8'),
      credentials: data.slice(0)
    };

    zap.send(returnPath.concat([
      zapReq.version,
      zapReq.requestId,
      new Buffer("200", "utf8"),
      new Buffer("OK", "utf8"),
      new Buffer(0),
      new Buffer(0)
    ]));
  });
  
  zap.bindSync("inproc://zeromq.zap.01."+count);
  return zap;
}
