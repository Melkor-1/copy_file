# Dummy Makefile to indicate that GNU Make should be used.

.POSIX:
all trip:
	@echo "You need GNU Make to build tests." 1>&2; false
