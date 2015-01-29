export top_srcdir

AUTOMAKE_OPTIONS = subdir-objects

if ENABLE_JAVA

module_LTLIBRARIES = libmod-java.la

libmod_java_la_CFLAGS = \
    $(JNI_CFLAGS)  \
    $(INCUBATOR_CFLAGS) \
    -I$(top_srcdir)/modules/java    \
    -I$(top_builddir)/modules/java	\
    -I$(top_srcdir)/modules/java/native \
    -I$(top_builddir)/modules/java/native

libmod_java_la_SOURCES = \
    native/java-grammar.y \
    native/java-plugin.c \
    native/java-destination.c \
    native/java-destination.h \
    proxies/java-destination-proxy.c \
    proxies/java-destination-proxy.h \
    proxies/java-logmsg-proxy.c \
    proxies/java-logmsg-proxy.h \
    proxies/java-template-proxy.c \
    proxies/java-template-proxy.h \
    proxies/internal-message-sender-proxy.c \
    native/java-class-loader.c \
    native/java-class-loader.h \
    native/java-parser.c \
    native/java-parser.h \
    native/java_machine.c \
    native/java_machine.h

libmod_java_la_LIBADD = \
    $(JNI_LIBS) $(INCUBATOR_LIBS)

libmod_java_la_LDFLAGS = \
    -avoid-version -module -no-undefined

libmod_java_la_DEPENDENCIES  = \
        $(MODULE_DEPS_LIBS)

JAVA_PKG_NAME = org.syslog_ng

JAVA_CLASS_DIR = org/syslog_ng

JAVA_CLASS_PATH = $(JAVA_CLASS_DIR)
JAVA_SRC_DIR = .

JAVA_CLASS_FILES = \
    $(JAVA_CLASS_PATH)/InternalMessageSender.class \
    $(JAVA_CLASS_PATH)/SyslogNgClassLoader.class \
    $(JAVA_CLASS_PATH)/LogMessage.class \
    $(JAVA_CLASS_PATH)/LogTemplate.class \
    $(JAVA_CLASS_PATH)/LogPipe.class \
    $(JAVA_CLASS_PATH)/LogDestination.class \
    $(JAVA_CLASS_PATH)/TextLogDestination.class \
    $(JAVA_CLASS_PATH)/StructuredLogDestination.class \
    $(JAVA_CLASS_PATH)/DummyTextDestination.class \
    $(JAVA_CLASS_PATH)/DummyStructuredDestination.class

JAVA_FILES = \
    $(JAVA_SRC_DIR)/InternalMessageSender.java \
    $(JAVA_SRC_DIR)/SyslogNgClassLoader.java \
    $(JAVA_SRC_DIR)/LogMessage.java \
    $(JAVA_SRC_DIR)/LogTemplate.java \
    $(JAVA_SRC_DIR)/LogPipe.java \
    $(JAVA_SRC_DIR)/LogDestination.java \
    $(JAVA_SRC_DIR)/TextLogDestination.java \
    $(JAVA_SRC_DIR)/StructuredLogDestination.java \
    $(JAVA_SRC_DIR)/DummyTextDestination.java \
    $(JAVA_SRC_DIR)/DummyStructuredDestination.java

JAVA_HEADER_FILES = \
   LogDestination.h \
   LogMessage.h \
   LogTemplate.h \
   InternalMessageSender.h

SyslogNg.jar: $(JAVA_CLASS_FILES) $(JAVA_HEADER_FILES)
	$(AM_V_GEN) $(JAR) -cvf $@ -C ./ org

$(JAVA_HEADER_FILES): $(JAVA_CLASS_FILES)
	$(AM_V_GEN) $(JAVAH) -cp ./ -o $@ $(JAVA_PKG_NAME).$(notdir $(basename $@))

$(JAVA_CLASS_PATH)/%.class: $(top_srcdir)/modules/java/%.java $(JAVA_FILES)
	$(AM_V_GEN) $(JAVAC) -d ./ -cp ./ $<

install-exec-local:
	$(mkinstalldirs) $(DESTDIR)$(moduledir)
	cp SyslogNg.jar $(DESTDIR)$(moduledir)/

uninstall-local:
	rm -rf $(DESTDIR)$(moduledir)/SyslogNg.jar

clean-local:
	rm -rf org
endif

include $(top_srcdir)/build/lex-rules.am

BUILT_SOURCES = \
    native/java-grammar.y \
    native/java-grammar.c \
    native/java-grammar.h

if ENABLE_JAVA
BUILT_SOURCES += $(JAVA_HEADER_FILES) \
                 SyslogNg.jar

CLEANFILES = $(JAVA_CLASS_FILES) \
             $(JAVA_HEADER_FILES) \
             SyslogNg.jar
endif

EXTRA_DIST = \
    native/java-grammar.ym \
    $(JAVA_FILES)
