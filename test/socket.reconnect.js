var zmq = require('..')
  , router = zmq.socket('router')
  , req = zmq.socket('req')
  , should = require('should')
  , tcp = 'tcp://127.0.0.1:3000';

describe('socket reconnect', function() {

    it('should reconnect', function(done) {
        var reqs = {a: 0, b: 0, c: 0, d: 0, e: 0};

        router.on('message', function(source, _, envelope, data) {
            // increments reqs flag on req
            reqs[data]++;
            router.send([source, '', 'ACK', data]);

            if (data == 'c') {
                router.unbind(tcp);
                setTimeout(router.bindSync.bind(router, tcp), 15);
            }
        });

        router.bindSync(tcp);

        req
            .connect(tcp)
            .monitor(1)
            .on('message', function(topic, data) {
                // increments reqs flag on router res
                reqs[data]++;
            });

        var delay = 0;
        Object.keys(reqs).forEach(function(key) {
            setTimeout(function() {
                req.send(['REQ', key]);
            }, delay += 5);
        });

        var assert = function() {
            // delay the test until every req get a response
            if(reqs.e !== 2)
                return setTimeout(assert, delay += 5);

            reqs.should.eql({a: 2, b: 2, c: 2, d: 2, e: 2});
            done();
        };
        setTimeout(assert, delay += 5);
    });
});
