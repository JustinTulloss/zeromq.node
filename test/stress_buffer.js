var vows = require('vows'),
    assert = require('assert'),
    zeromq = require('../zeromq');

var uri = 'inproc://unbalanced';

var sock1 = zeromq.createSocket("xreq");
sock1["identity"] = "Sock1";

var sock2 = zeromq.createSocket("xrep");

function sock1Message(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9)
{
    assert.equal(arg2, "this");
    assert.equal(arg3, "is");
    assert.equal(arg4, "a");
    assert.equal(arg5, "long");
    assert.equal(arg6, "message");
    assert.equal(arg7, "back");
    assert.equal(arg8, "to");
    assert.equal(arg9, "you");

    //console.log("got message on sock1: ", arg1 ? arg1.toString() : "", arg2 ? arg2.toString() : "",
    //            arg3 ? arg3.toString() : "");
    //sock1.send('message back');
}

function sock1Error(err)
{
    console.log("error from sock1: ", err);
    sock1.close();
    sock2.close();
    process.exit(1);
}

function sock2Message(arg1, arg2, arg3)
{
    assert.equal(arg1.toString(), "Sock1");
    assert.equal(arg2.toString(), "message");

    //console.log("got message on sock2: ", arg1 ? arg1.toString() : "", arg2 ? arg2.toString() : "",
    //            arg3 ? arg3.toString() : "");
    sock2.send(arg1, arg2, 'this', 'is', 'a', 'long', 'message', 'back', 'to', 'you');
    //console.log("got message on sock2: ", arg1, arg2, arg3);
    //sock2.send(arg1, 'hi', 'back');
}

function sock2Error(err)
{
    console.log("error from sock2: ", err);
    sock1.close();
    sock2.close();
    process.exit(1);
}

sock1.on('message', sock1Message);
sock1.on('error', sock1Error);
sock2.on('message', sock2Message);
sock2.on('error', sock2Error);

var msg = 0;

var intervalId;

function onTick()
{
    for (var i = 0;  i < 1000;  ++i) {
        sock1.send('message', "" + msg);
        ++msg;
    }

    console.log("done " + msg + " messages");

    if (msg > 1000000) {
        clearInterval(intervalId);
        sock1.close();
        sock2.close();
    }

}

function onBindDone()
{
    sock1.connect(uri);

    //sock1.send('hello', 'world');
    intervalId = setInterval(onTick, 1);
}

var doSync = true;

if (doSync) {
    sock2.bindSync(uri);
    onBindDone();
}
else {
    sock2.bind(uri, onBindDone);
}


var suite = vows.describe('ZeroMQ');
suite.options.error = false;

var tests = {

};

suite.addBatch(tests);

suite.export(module);
