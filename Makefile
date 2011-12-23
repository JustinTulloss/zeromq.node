
DOX = ./node_modules/.bin/dox
JADE = ./node_modules/.bin/jade

binding.node: build binding.cc
	node-waf build

build:
	node-waf configure

test:
	@./test/run

clean:
	node-waf clean

docs:
	$(DOX) < lib/index.js > docs/index.json
	$(JADE) < docs/template.jade -o "{comments:$$(cat docs/index.json)}" > docs/index.html

docclean:
	rm -fr docs/index.{json,html}

distclean:
	node-waf distclean

.PHONY: clean distclean test docs docclean