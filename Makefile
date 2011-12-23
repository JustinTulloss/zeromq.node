
binding.node: build binding.cc
	node-waf build

build:
	node-waf configure

test:
	@./test/run

clean:
	node-waf clean

docs:
	dox < lib/index.js > docs/index.json
	jade < docs/template.jade -o "{comments:$$(cat docs/index.json)}" > docs/index.html

docclean:
	rm -fr docs/index.{json,html}

distclean:
	node-waf distclean

.PHONY: clean distclean test docs docclean