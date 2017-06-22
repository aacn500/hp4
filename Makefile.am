if HAVE_CHECK
MAYBE_CHECK = test
else
check:
	@echo "libcheck was unavailable; tests could not be run"; false
endif

SUBDIRS = src . $(MAYBE_CHECK)
