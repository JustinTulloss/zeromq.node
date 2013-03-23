
TESTS = $(wildcard test/test.*.js)
DOX = ./node_modules/.bin/dox
JADE = ./node_modules/.bin/jade

build/Release/binding.node: binding.cc binding.gyp
	npm install

test:
	@node test/run $(TESTS)

clean:
	rm -fr build

docs:
	$(DOX) < lib/index.js > docs/index.json
	$(JADE) < docs/template.jade -o "{comments:$$(cat docs/index.json)}" > docs/index.html

docclean:
	rm -fr docs/index.{json,html}

distclean:
	node-gyp clean

.PHONY: clean distclean test docs docclean
