# Top level makefile, the real shit is at src/Makefile

default: all

.DEFAULT:
	cd src && $(MAKE) $@
demo:
	make -C src debug=debug demo

install:
	cd src && $(MAKE) $@

.PHONY: install
