
binding.node: build binding.cc
	node-waf build

build:
	node-waf configure

clean:
	node-waf clean

distclean:
	node-waf distclean

.PHONY: clean distclean