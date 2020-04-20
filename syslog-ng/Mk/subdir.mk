top_srcdir	= $(dir $(shell readlink ${MAKEFILE_LIST}))../
top_builddir	= $(shell ${top_srcdir}/Mk/find-top-builddir.sh)

self		= $(subst ${top_builddir}/,,${CURDIR})
self_sub	= $(subst -,_,$(subst /,_,${self}))
basedir		= $(firstword $(subst /, ,${self}))

levelup		=

ifeq (${self_sub},lib_filter)
levelup		= 1
else ifeq (${self_sub},lib_logproto)
levelup		= 1
else ifeq (${self_sub},lib_parser)
levelup		= 1
else ifeq (${self_sub},lib_rewrite)
levelup		= 1
endif

ifeq (${levelup},1)
self		= $(subst /,,$(dir $(subst ${top_builddir}/,,${CURDIR})))
endif

all:
	${AM_v_at}${MAKE} -C ${top_builddir} ${self}/

check:
	${AM_v_at}${MAKE} -C ${top_builddir} local-check check_subdir=${self_sub}

INSTALL_ARGS	= bin_SCRIPTS= \
		bin_PROGRAMS= \
		sbin_PROGRAMS= \
		libexec_PROGRAMS= \
		man_MANS= \
		tools_DATA= \
		xsd_DATA= \
		INSTALL_EXEC_HOOKS=

ifeq (${basedir},lib)
INSTALL_ARGS	+= module_LTLIBRARIES=
else
 ifeq (${basedir},modules)
INSTALL_ARGS	+= lib_LTLIBRARIES= pkginclude_HEADERS= pkgconfig_DATA=
  ifneq (${self_sub},modules)
   ifeq (${self_sub},modules_afsocket)
INSTALL_ARGS	+= module_LTLIBRARIES="modules/afsocket/libafsocket.la"
INSTALL_ARGS	+= INSTALL_EXEC_HOOKS=afsocket-install-exec-hook
   else ifeq (${self_sub},modules_json)
INSTALL_ARGS	+= module_LTLIBRARIES="modules/json/libjson-plugin.la"
   else ifeq (${self_sub},modules_python)
INSTALL_ARGS	+= module_LTLIBRARIES="modules/python/libmod-python.la"
   else
INSTALL_ARGS	+= module_LTLIBRARIES=${self}/lib$(word 2,$(subst /, ,${self})).la
   endif
  endif
 else
INSTALL_ARGS	=
 endif
endif

install:
ifeq (${INSTALL_ARGS},)
	@echo "Installing from this directory is not supported."
else
	${AM_v_at}${MAKE} -C ${top_builddir} install ${INSTALL_ARGS}
endif

clean dist distcheck:
	${AM_v_at}${MAKE} -C ${top_builddir} $@

%:
	${AM_v_at}${MAKE} -C ${top_builddir} ${self}/$@
