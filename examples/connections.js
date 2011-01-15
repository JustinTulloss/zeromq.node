zmq = require('zeromq');
util = require('util');

//this will test the number of concurrent connections
msg = "when will I break"

trys = 500

for(i=0;i<trys;i++){

	createSock = "s_" + i + " = zmq.createSocket('req');"
	eval (createSock);
	console.log(createSock)
	
	connectStr = "s_" + i + ".connect('tcp://127.0.0.1:5554')";
	eval (connectStr);
	console.log(connectStr);
	
	sendStr = "s_" + i + ".send(msg);";
	eval (sendStr);
	console.log(sendStr);
}

for(i=0;i<trys;i++){
	str = "s_" + i + ".close();"
	eval (str);
	console.log(str)
}
