UPSTREAM_VERSION	= $(shell debian/build-tree/syslog-ng/syslog-ng --version | head -n 1)

MANS			= debian/man/syslog-ng.8 debian/man/loggen.1

all: ${MANS}
clean:
	rm -f ${MANS}

debian/man/syslog-ng.8: debian/man/syslog-ng.8.inc
	help2man -N -s 8 -m "syslog-ng manual" -n "syslog-ng system logger application" \
		 --version-string "${UPSTREAM_VERSION}" -h "--help-all" \
		 -i $^ debian/build-tree/syslog-ng/syslog-ng >$@.tmp
	sed -e 's,^lt..syslog..ng \[OPTION...\] \(.*\),\1 [OPTIONS...],' \
	    -e 's#, default=affile.*#.#' <$@.tmp >$@
	rm -f $@.tmp

debian/man/loggen.1: debian/man/loggen.1.inc
	help2man -N -s 1 -m "syslog-ng manual" \
		 --version-string "${UPSTREAM_VERSION}" -h "--help-all" \
		 -i $^ debian/build-tree/tests/loggen/loggen >$@.tmp
	sed -e "s,xlt-,," <$@.tmp >$@
	rm -f $@.tmp

.PHONY: all clean
