
TESTS = $(wildcard test/test.*.js)

binding.node: build binding.cc
	node-waf build

build:
	node-waf configure

test:
	@node test/run $(TESTS)

clean:
	node-waf clean

distclean:
	node-waf distclean

.PHONY: clean distclean test