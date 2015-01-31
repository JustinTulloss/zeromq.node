
TESTS = $(wildcard test/test.*.js)
MOCHA = ./node_modules/.bin/mocha

build/Release/binding.node: binding.cc binding.gyp
	npm install

test:
	$(MOCHA) --expose-gc --slow 2000 --timeout 600000

clean:
	rm -fr build

distclean:
	node-gyp clean

perf:
	node perf/local_lat.js tcp://127.0.0.1:5555 1 100000& node perf/remote_lat.js tcp://127.0.0.1:5555 1 100000
	node perf/local_thr.js tcp://127.0.0.1:5556 1 100000& node perf/remote_thr.js tcp://127.0.0.1:5556 1 100000

.PHONY: test clean distclean perf
