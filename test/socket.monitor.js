var zmq = require('..')
  , should = require('should')
  , semver = require('semver');

describe('socket.monitor', function(){
  it('should be able to monitor the socket', function(done){
    /* no test if monitor is not available */
    if (!zmq.ZMQ_CAN_MONITOR) {
      console.log("monitoring not enabled skipping test");
      done();
      return;
    }

    var rep = zmq.socket('rep')
      , req = zmq.socket('req')
      , events = [];

    rep.on('message', function(msg){
      msg.should.be.an.instanceof(Buffer);
      msg.toString().should.equal('hello');
      rep.send('world');
    });

    rep.on('listen', function(event_value, event_endpoint_addr){
      console.log("listen %s,%d",event_endpoint_addr,event_value);
      event_endpoint_addr.toString().should.equal('tcp://127.0.0.1:5423');
      events.push('listen');
    });

    rep.on('accept', function(event_value, event_endpoint_addr){
      console.log("accept %s,%d",event_endpoint_addr,event_value);
      event_endpoint_addr.toString().should.equal('tcp://127.0.0.1:5423');
      events.push('accept');
    });

    rep.on('disconnect', function(event_value, event_endpoint_addr){
      console.log("disconnect %s,%d",event_endpoint_addr, event_value);
      event_endpoint_addr.toString().should.equal('tcp://127.0.0.1:5423');
      events.push('disconnect');
    });

    rep.on('close', function(event_value, event_endpoint_addr){
      console.log("closed %s,%d",event_endpoint_addr, event_value);
      event_endpoint_addr.toString().should.equal('tcp://127.0.0.1:5423');
      events.push('close');
    });

    /* enable monitoring for this socket */
    rep.monitor();

    rep.bind('tcp://127.0.0.1:5423');

    rep.on('bind', function(){
      req.connect('tcp://127.0.0.1:5423');
      req.send('hello');
      req.on('message', function(msg){
        msg.should.be.an.instanceof(Buffer);
        msg.toString().should.equal('world');
        req.close();

        // wait a few for the "disconnect" to reach us (can't close rep yet)
        setTimeout((function() {
          rep.close();
          // wait a few for the "close" to reach us then call unmonitor to release the handle
          setTimeout(function() {
            rep.unmonitor();
            events.length.should.equal(4);
            done();
	      }, 500);
        }), 500);
      });
    });
  });
});
