DIST_SUBDIRS				+= modules/afamqp/rabbitmq-c

CLEAN_SUBDIRS 				+= @LIBRABBITMQ_SUBDIRS@

if ENABLE_AMQP
module_LTLIBRARIES			+= modules/afamqp/libafamqp.la

modules_afamqp_libafamqp_la_CFLAGS	= 	\
	$(LIBRABBITMQ_CFLAGS) 			\
	-I$(top_srcdir)/modules/afamqp 		\
	-I$(top_builddir)/modules/afamqp
modules_afamqp_libafamqp_la_SOURCES	= 	\
	modules/afamqp/dummy.c			\
	modules/afamqp/afamqp-grammar.y		\
	modules/afamqp/afamqp.c			\
	modules/afamqp/afamqp.h			\
	modules/afamqp/afamqp-parser.c		\
	modules/afamqp/afamqp-parser.h
modules_afamqp_libafamqp_la_LIBADD	= 	\
	$(MODULE_DEPS_LIBS) $(LIBRABBITMQ_LIBS)
modules_afamqp_libafamqp_la_LDFLAGS	=	\
	$(MODULE_LDFLAGS)
modules_afamqp_libafamqp_la_DEPENDENCIES=	\
	$(MODULE_DEPS_LIBS)			\
	modules/afamqp/rabbitmq-c/librabbitmq/librabbitmq.la

modules/afamqp/dummy.c: modules/afamqp/rabbitmq-c/librabbitmq/librabbitmq.la
	$(AM_V_GEN) touch $@
modules/afamqp/rabbitmq-c/librabbitmq/librabbitmq.la:
	${MAKE} -C modules/afamqp/rabbitmq-c
modules/afamqp modules/afamqp/ mod-afamqp mod-amqp: \
	modules/afamqp/libafamqp.la
else
modules/afamqp modules/afamqp/ mod-afamqp mod-amqp:
endif

BUILT_SOURCES				+=	\
		modules/afamqp/dummy.c		\
		modules/afamqp/afamqp-grammar.y \
		modules/afamqp/afamqp-grammar.c \
		modules/afamqp/afamqp-grammar.h
EXTRA_DIST				+=	\
		modules/afamqp/afamqp-grammar.ym \
		modules/afamqp/rabbitmq-c/configure.gnu

.PHONY: modules/afamqp/ mod-afamqp mod-amqp
