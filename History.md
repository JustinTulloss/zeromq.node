2.15.1 / 2016-5-8
=================

  * Node.js 6 compatibility (NAN 2.3) [kkoopa]

2.15.0 / 2016-4-27
==================

  * Dropped support for Node 0.8 [reqshark]
  * Added unref/ref APIs to detach/attach sockets from/to the event loop [Joongi Kim]
  * Improved message throughput 3-fold on ZMQ 4 [ronkorving]
  * When bind or unbind failed, you could never try again (fixed) [ronkorving]
  * Various travis configuration improvements [reqshark]
  * Bumped NAN to 2.2.x [JanStevens]

2.14.0 / 2015-11-20
===================

  * A socket.read() method was added to retrieve messages while paused [sshutovskyi]
  * socket.send() now takes a callback as 3rd argument which is called once the message is sent [ronkorving]
  * Now tested on Node.js 0.8, 0.10, 0.12, 4 and 5 [ronkorving]

2.13.0 / 2015-08-26
===================

  * io.js 3.x compatible [kkoopa]
  * corrections to type casting operations [kkoopa]
  * "make clean" now also removes node_modules [reqshark]

2.12.0 / 2015-07-10
===================

  * Massive improvements to monitoring code, with new documentation and tests [ValYouW]
  * Improved documentation [reqshark]
  * Updated bindings from ~1.1.1 to ~1.2.1 [reqshark]
  * Test suite improvements [reqshark]
  * Updated the Windows bundle to ZeroMQ 4.0.4 [kkoopa]
  * License attribute added to package.json [pdehaan]

2.11.1 / 2015-05-21
===================

  * io.js 2.x compatible [transcranial]
  * replaced asserts with proper exceptions [reqshark]

2.11.0 / 2015-03-31
===================

  * Added pause() and resume() APIs on sockets to allow backpressure [philip1986]
  * Elegant handling of EINTR return codes [hurricaneLTG]
  * Small performance improvements in send() and internal flush methods [ronkorving]
  * Updated test suite to cover io.js and Node 0.12 (removed 0.11) [ronkorving]
  * Added "make perf" for easy benchmarking [ronkorving]

2.10.0 / 2015-01-22
===================

  * Added ZMQ_STREAM socket type [reqshark]
  * Update NAN to io.js compatible 1.5.0 [kkoopa]
  * Hitting open file descriptor limit now throws an error during zmq.socket() [briansorahan]
  * More reliable benchmarking [maxired]

2.9.0 / 2015-01-05
==================

  * More unit tests [bluebery and reqshark]
  * More reliable testing [f34rdotcom and kkoopa]
  * Improved ReadMe [dminkovsky and skibz]
  * Support for zmq_proxy sockets [reqshark]
  * Removed "docs" and related deps in favor of ReadMe [reqshark]

2.8.0 / 2014-08-27
==================

  * Fixed: monitor API would keep CPU busy at 100% [f34rdotcom]
  * Fixed: an exception during flush could render a socket unusable [ronkorving]
  * Fixed: Travis changed behavior and broke our tests [ronkorving]
  * Code cleanup [kkoopa and ronkorving]
  * Removed legacy nextTick event emission during flush [utvara and ronkorving]
  * Context API added: setMaxThreads, getMaxThreads, setMaxSockets, getMaxSockets [yoneal]
  * Changed unit test suite to Mocha [skeggse and yoneal]
  * NAN updated to ~1.3.0 [kkoopa]

2.7.0 / 2014-04-24
==================

  * Fixed memory leak when closing socket [rasky]
  * Fixed high water mark [soplwang, kkoopa]
  * Added socket opts for zeromq 4.x security mechanisms [msealand]
  * Use MakeCallback [kkoopa]
  * Remove useless setImmediate [kkoopa]
  * Use `zmq_msg_send` for ZMQ >= 4.0 [kkoopa]
  * Expose the Socket class as zmq.Socket [tcr]

2.6.0 / 2014-01-23
==================

  * Monitor support [f34rdotcom, dr-fozzy]
  * Unbind support [kkoopa]
  * Node 0.11.9 compatibility [kkoopa]
  * Support for ZMQ 4 [atrniv]
  * Fixed memory leak [utvara]
  * OSX Homebrew support [jwalton]
  * Fix unit tests [ryanlelek]

2.5.1 / 2013-08-28
==================

  * Regression fix for IPC socket bind failure [christopherobin]

2.5.0 / 2013-08-20
==================

  * Added testing against Node.js v0.11 [AlexeyKupershtokh]
  * Add support for Joyent SmartMachines [JonGretar]
  * Use pkg-config on OS X too [blalor]
  * Patch for Node 0.11.3 [kkoopa]
  * Fix for bind / connect / send problem [kkoopa]
  * Fixed multiple bugs in perf tests and changed them to push/pull [ronkorving]
  * Add definitions for building on openbsd & freebsd [Minjung]

2.4.0 / 2013-04-09
==================

  * added: Windows support [mscdex]
  * added: support for all options ever [AlexeyKupershtokh]
  * fixed: prevent zeromq sockets from being destroyed by GC [AlexeyKupershtokh]

2.3.0 / 2013-03-15
==================

  * added: xpub/xsub socket types [xla]
  * added: support for zmq_disconnect [matehat]
  * added: LAST_ENDPOINT socket option [ronkorving]
  * added: local/remote_lat local/remote_thr perf test [wavded]
  * fixed: tests improved [qubyte, jeremybarnes, ronkorving]
  * fixed: Node v0.9.4+ compatibility [mscdex]
  * fixed: SNDHWM and RCVHWM options were given the wrong type [freehaha]
  * removed: waf support [mscdex]

2.2.0 / 2012-10-17
==================

  * add support for pkg-config
  * add libzmq 3.x support [aaudis]
  * fix: prevent GC happening too soon for connect/bindSync

2.1.0 / 2012-06-29
==================

  * fix require() for 0.8.0
  * change: use uv_poll in place of IOWatcher
  * remove stupid engines field

2.0.3 / 2012-03-14
==================

  * Removed -Wall (libuv unused vars caused the build to fail...)

2.0.2 / 2012-02-16
==================

  * Added back `.createSocket()` for BC. Closes #86

2.0.1 / 2012-01-26
==================

  * Added `.zmqVersion` [patricklucas]
  * Fixed multipart support [joshrtay]
