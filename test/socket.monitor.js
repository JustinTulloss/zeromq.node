var zmq = require('..')
  , should = require('should')
  , semver = require('semver');

describe('socket.monitor', function() {
  if (!zmq.ZMQ_CAN_MONITOR) {
    console.log("monitoring not enabled skipping test");
    return;
  }

  it('should be able to monitor the socket', function(done) {
    var rep = zmq.socket('rep')
      , req = zmq.socket('req')
      , events = [];

    rep.on('message', function(msg){
      msg.should.be.an.instanceof(Buffer);
      msg.toString().should.equal('hello');
      rep.send('world');
    });

    var testedEvents = ['listen', 'accept', 'disconnect', 'close'];
    testedEvents.forEach(function(e) {
      rep.on(e, function(event_value, event_endpoint_addr) {
        // Test the endpoint addr arg
        event_endpoint_addr.toString().should.equal('tcp://127.0.0.1:5423');

        // If this is a disconnect event we can now close the rep socket
        if (e === 'disconnect') {
          rep.close();
        }

        testedEvents.pop();
        if (testedEvents.length === 0) {
          rep.unmonitor();
          done();
        }
      });
    });

    // enable monitoring for this socket
    rep.monitor();

    rep.bind('tcp://127.0.0.1:5423', function (error) {
      if (error) throw error;
    });

    rep.on('bind', function(){
      req.connect('tcp://127.0.0.1:5423');
      req.send('hello');
      req.on('message', function(msg){
        msg.should.be.an.instanceof(Buffer);
        msg.toString().should.equal('world');
        req.close();
      });
    });
  });

  it('should use default interval and numOfEvents', function(done) {
    var req = zmq.socket('req');
    req.setsockopt(zmq.ZMQ_RECONNECT_IVL, 5); // We want a quick connect retry from zmq

    // We will try to connect to a non-existing server, zmq will issue events: "connect_retry", "close", "connect_retry"
    // The connect_retry will be issued immediately after the close event, so we will measure the time between the close
    // event and connect_retry event, those should >= 10 (this will tell us that we are reading 1 event at a time from
    // the monitor socket).

    var closeTime;
    req.on('close', function() {
	    closeTime = Date.now();
    });

    req.on('connect_retry', function() {
      var diff = Date.now() - closeTime;
      req.unmonitor();
      req.close();
      diff.should.be.within(10, 20);
      done();
    });

    req.monitor();
    req.connect('tcp://127.0.0.1:5423');
  });

  it('should read multiple events on monitor interval', function(done) {
    var req = zmq.socket('req');
    req.setsockopt(zmq.ZMQ_RECONNECT_IVL, 5);
    var closeTime;
    req.on('close', function() {
      closeTime = Date.now();
    });

    req.on('connect_retry', function() {
      var diff = Date.now() - closeTime;
      req.unmonitor();
      req.close();
      diff.should.be.within(0, 5);
      done();
    });

    // This should read all available messages from the queue, and we expect that "close" and "connect_retry" will be
    // read on the same interval (for further details see the comment in the previous test)
    req.monitor(10, 0);
    req.connect('tcp://127.0.0.1:5423');
  });
});
