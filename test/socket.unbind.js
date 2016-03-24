var zmq = require('..')
  , should = require('should')
  , semver = require('semver');

describe('socket.unbind', function(){

  it('should be able to unbind', function(done){
    if (!zmq.ZMQ_CAN_UNBIND) {
      done();
      return;
    }
    var a = zmq.socket('dealer')
      , b = zmq.socket('dealer')
      , c = zmq.socket('dealer');

    var message_count = 0;
    a.bind('tcp://127.0.0.1:5420', function (error) {
      if (error) throw error;
      a.bind('tcp://127.0.0.1:5421', function (error) {
        if (error) throw error;
        b.connect('tcp://127.0.0.1:5420');
        b.send('Hello from b.');
        c.connect('tcp://127.0.0.1:5421');
        c.send('Hello from c.');
      });
    });

    a.on('unbind', function(addr) {
      if (addr === 'tcp://127.0.0.1:5420') {
        b.send('Error from b.');
        c.send('Messsage from c.');
        setTimeout(function () {
          c.send('Final message from c.');
        }, 100);
      }
    });

    a.on('message', function(msg) {
      message_count++;
      if (msg.toString() === 'Hello from b.') {
        a.unbind('tcp://127.0.0.1:5420', function (error) {
          if (error) throw error;
        });
      } else if (msg.toString() === 'Final message from c.') {
        message_count.should.equal(4);
        a.close();
        b.close();
        c.close();
        done();
      } else if (msg.toString() === 'Error from b.') {
        throw Error('b should have been unbound');
      }
    });
  });
});
