export top_srcdir

if ENABLE_JAVA

module_LTLIBRARIES += libmod-java.la

libmod_java_la_CFLAGS = \
    $(JNI_CFLAGS)  \
    $(INCUBATOR_CFLAGS) \
    -I$(top_srcdir)/modules/java    \
    -I$(top_builddir)/modules/java	\
    -I$(top_srcdir)/modules/java/native \
    -I$(top_builddir)/modules/java/native

libmod_java_la_SOURCES = \
    modules/java/native/java-grammar.y \
    modules/java/native/java-plugin.c \
    modules/java/native/java-destination.c \
    modules/java/native/java-destination.h \
    modules/java/proxies/java-destination-proxy.c \
    modules/java/proxies/java-destination-proxy.h \
    modules/java/proxies/java-logmsg-proxy.c \
    modules/java/proxies/java-logmsg-proxy.h \
    modules/java/proxies/java-template-proxy.c \
    modules/java/proxies/java-template-proxy.h \
    modules/java/proxies/internal-message-sender-proxy.c \
    modules/java/native/java-class-loader.c \
    modules/java/native/java-class-loader.h \
    modules/java/native/java-parser.c \
    modules/java/native/java-parser.h \
    modules/java/native/java_machine.c \
    modules/java/native/java_machine.h

libmod_java_la_LIBADD = \
    $(JNI_LIBS) $(INCUBATOR_LIBS)

libmod_java_la_LDFLAGS = \
    -avoid-version -module -no-undefined

libmod_java_la_DEPENDENCIES  = \
        $(MODULE_DEPS_LIBS)

JAVA_PKG_NAME = org.syslog_ng

JAVA_CLASS_DIR = modules/java/org/syslog_ng

JAVA_CLASS_PATH = $(JAVA_CLASS_DIR)
JAVA_SRC_DIR = modules/java

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
   modules/java/LogDestination.h \
   modules/java/LogMessage.h \
   modules/java/LogTemplate.h \
   modules/java/InternalMessageSender.h

modules/java/SyslogNg.jar: $(JAVA_CLASS_FILES) $(JAVA_HEADER_FILES)
	$(AM_V_GEN) $(JAR) -cvf $@ -C ./ org

$(JAVA_HEADER_FILES): $(JAVA_CLASS_FILES)
	$(AM_V_GEN) $(JAVAH) -cp ./ -o $@ $(JAVA_PKG_NAME).$(notdir $(basename $@))

$(JAVA_CLASS_PATH)/%.class: $(top_srcdir)/modules/java/%.java $(JAVA_FILES)
	$(AM_V_GEN) $(JAVAC) -d ./ -cp ./ $<

java-install-exec-hook:
	$(mkinstalldirs) $(DESTDIR)$(moduledir)
	cp modules/java/SyslogNg.jar $(DESTDIR)$(moduledir)/

java-uninstall-exec-hook:
	rm -rf $(DESTDIR)$(moduledir)/SyslogNg.jar

java-clean-hook:
	rm -rf org

INSTALL_EXEC_HOOKS += java-install-exec-hook
UNINSTALL_HOOKS += java-uninstall-exec-hook
CLEAN_HOOKS += java-clean-hook

endif

BUILT_SOURCES += \
    modules/java/native/java-grammar.y \
    modules/java/native/java-grammar.c \
    modules/java/native/java-grammar.h

if ENABLE_JAVA
BUILT_SOURCES += $(JAVA_HEADER_FILES) \
                 modules/java/SyslogNg.jar

CLEANFILES += $(JAVA_CLASS_FILES) \
             $(JAVA_HEADER_FILES) \
             modules/java/SyslogNg.jar
endif

EXTRA_DIST += \
    modules/java/native/java-grammar.ym \
    $(JAVA_FILES)
