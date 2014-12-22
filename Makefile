
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

.PHONY: clean distclean test docs docclean
