var zeromq = require('zeromq'),
    util = require('util');

var netstringRe = /([0-9]+):(.*)/;
var parseRe = /(\S+) ([0-9]+) (\S+) (.*)/;

function parseNetstring(ns) {
    var parts = ns.match(netstringRe);
    var len = parseInt(parts[1], 10);
    var data = parts[2];

    return {
        data: data.slice(0, len),
        rest: data.slice(len + 1)
    };
}

function parse(msg) {
    var parts = msg.match(parseRe);
    var uuid = parts[1];
    var connId = parts[2];
    var path = parts[3];

    parts = parseNetstring(parts[4]);
    var headers = JSON.parse(parts.data);
    var body = parseNetstring(parts.rest).data;

    return {
        uuid: uuid,
        connId: connId,
        path: path,
        headers: headers,
        body: body
    };
}

function Response() {
    this.socket = zeromq.createSocket('pub');
    this.socket.connect('tcp://127.0.0.1:34560');
    this.socket.identity = "supernodery";
}

Response.prototype = {
    send: function(uuid, connIds, msg) {
        var idString = connIds.join(' ');
        var header = [
            uuid, " ",
            idString.length, ':', idString, ', '].join('');
        var httpHeader = "HTTP/1.1 200 OK\r\n\r\n\r\n"
        var out = new Buffer(header + httpHeader + msg, 'ascii');
        this.socket.send(out);
    }
};

function handler() {
    var socket = zeromq.createSocket('pull');
    socket.connect('tcp://127.0.0.1:45670');
    socket.on('message', function(message) {
        message = parse(message.toString()); //messages come back as buffers
        var resp = new Response();
        resp.send(message.uuid,
            [message.connId],
            'Got a new message!\n' + util.inspect(message));
    });
    util.puts('Ready to serve.');
}

handler();
