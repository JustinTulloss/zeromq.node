var zmq = require('..');
var http = require('http');
var should = require('should');
var semver = require('semver');

describe('socket.stream', function () {
  // socket stream type API after libzmq4+, target > 4.0.0
  // v4.1.3's stream socket is a little buggy, let's exclude it for now

  it('should support a stream socket type', function (done) {
    if (semver.lt(zmq.version, '4.0.0') || zmq.version !== '4.1.3') {
      done();
      return console.warn('Stream socket type requires libzmq v4+ (skipping also on 4.1.3)');
    }

    var stream = zmq.socket('stream');
    stream.on('message', function (id, msg) {
      msg.should.be.an.instanceof(Buffer);

      var raw_header = msg.toString().split('\r\n');
      var method = raw_header[0].split(' ')[0];
      method.should.equal('GET');

      //finding an HTTP GET method, prepare HTTP response for TCP socket
      var httpProtocolString = 'HTTP/1.0 200 OK\r\n' //status code
        + 'Content-Type: text/html\r\n' //headers
        + '\r\n'
        + '<!DOCTYPE html>' //response body
          + '<head>'        //make it xml, json, html or something else
            + '<meta charset="UTF-8">'
          + '</head>'
          + '<body>'
            +'<p>derpin over protocols</p>'
          + '</body>'
        +'</html>'

      //zmq streaming prefixed by envelope's routing identifier
      stream.send([id,httpProtocolString]);
    });

    var addr = '127.0.0.1:5513';
    stream.bind('tcp://' + addr, function (error) {
      if (error) throw error;
      //send non-peer request to zmq, like an http GET method with URI path
      http.get('http://' + addr + '/aRandomRequestPath', function (httpMsg) {

        //msg should now be a node readable stream as the good lord intended
        if (semver.gte(process.versions.node, '0.11.0')) {
          httpMsg.socket._readableState.reading.should.be.false
        } else {
          httpMsg.socket._readableState.reading.should.be.true
        }

        //conventional node streams emit data events to process zmq stream response
        httpMsg.on('data', function (msg) {
          msg.should.be.an.instanceof(Buffer);
          msg.toString().should.equal('<!DOCTYPE html><head><meta charset="UTF-8"></head>'
            +'<body>'
              +'<p>derpin over protocols</p>'
            +'</body>'
          +'</html>');
          done();
        });
      });
    });
  });
});
