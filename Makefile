
build/Release/binding.node: binding.cc binding.gyp
	npm install

test:
	npm test

clean:
	rm -fr build node_modules ldlocal
	rm -rf zeromq-4.1.3 zeromq-4.1.3.tar.gz libsodium-1.0.5 libsodium-1.0.5.tar.gz
	mkdir ldlocal

distclean:
	node-gyp clean

perf:
	node perf/local_lat.js tcp://127.0.0.1:5555 1 100000& node perf/remote_lat.js tcp://127.0.0.1:5555 1 100000
	node perf/local_thr.js tcp://127.0.0.1:5556 1 100000& node perf/remote_thr.js tcp://127.0.0.1:5556 1 100000

.PHONY: test clean distclean perf
