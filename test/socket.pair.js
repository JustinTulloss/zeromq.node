var zmq = require('..')
  , should = require('should');

describe('socket.pair', function(){

  it('should support pair-pair', function (done){
    var pairB = zmq.socket('pair')
      , pairC = zmq.socket('pair');

    var n = 0;
    pairB.on('message', function (msg) {
      msg.should.be.an.instanceof(Buffer);

      switch (n++) {
        case 0:
          msg.toString().should.equal('foo');
          break;
        case 1:
          msg.toString().should.equal('bar');
          break;
        case 2:
          msg.toString().should.equal('baz');
          pairB.close();
          pairC.close();
          done();
          break;
      }
    });

    pairC.on('message', function (msg) {
      msg.should.be.an.instanceof(Buffer);
      msg.toString().should.equal('barnacle');
    })

    var addr = "inproc://stuffpair";

    pairB.bind(addr, function (error) {
      if (error) throw error;

      pairC.connect(addr);
      pairB.send('barnacle');
      pairC.send('foo');
      pairC.send('bar');
      pairC.send('baz');
    });
  });

});
