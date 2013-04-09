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
