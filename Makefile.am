ACLOCAL_AMFLAGS = -I m4
SUBDIRS = src

EXTRA_DIST = utils Doxyfile.in README.md

if HAVE_DOXYGEN
doxygen: Doxyfile
	$(DOXYGEN) Doxyfile

clean-local:
	rm -rf doc
endif
