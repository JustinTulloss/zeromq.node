
binding.node: build binding.cc
	node-waf build

build:
	node-waf configure

test:
	vows test/test.js

clean:
	node-waf clean

distclean:
	node-waf distclean

.PHONY: clean distclean test