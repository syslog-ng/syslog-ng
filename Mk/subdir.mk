top_srcdir	= $(dir $(shell readlink ${MAKEFILE_LIST}))../
top_builddir	= $(shell ${top_srcdir}/Mk/find-top-builddir.sh)

self		= $(subst ${top_builddir}/,,${CURDIR})
self_sub	= $(subst -,_,$(subst /,_,${self}))

all:
	${AM_v_at}${MAKE} -C ${top_builddir} ${self}/

check:
	${AM_v_at}${MAKE} -C ${top_builddir} local-check check_subdir=${self_sub}

clean dist distcheck:
	${AM_v_at}${MAKE} -C ${top_builddir} $@

%:
	${AM_v_at}${MAKE} -C ${top_builddir} ${self}/$@
