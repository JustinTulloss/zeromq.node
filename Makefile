
binding.node: build binding.cc
	node-waf build

build:
	node-waf configure

test:
	@./test/run

clean:
	node-waf clean

distclean:
	node-waf distclean

.PHONY: clean distclean test