if ENABLE_JAVA

JAVA_DST_DIR=$(DESTDIR)/$(moduledir)/java-modules
JAVA_BUILDDIR=$(top_builddir)/modules/java
SYSLOG_NG_CORE_SRC_DIR = $(top_srcdir)/modules/java/src/main/java/org/syslog_ng
SYSLOG_NG_CORE_CLASSES_DIR=$(top_builddir)/modules/java/syslog-ng-core/classes
SYSLOG_NG_CORE_PACKAGE_DIR=$(top_builddir)/modules/java/syslog-ng-core/classes/org/syslog_ng
SYSLOG_NG_CORE_LIBS_DIR=$(top_builddir)/modules/java/syslog-ng-core/libs

LogDestination_CLASS_DEPS = \
	$(SYSLOG_NG_CORE_PACKAGE_DIR)/LogPipe.class

LogPipe_CLASS_DEPS = \
	$(SYSLOG_NG_CORE_PACKAGE_DIR)/InternalMessageSender.class

LogTemplate_CLASS_DEPS = \
	$(SYSLOG_NG_CORE_PACKAGE_DIR)/LogMessage.class

StructuredLogDestination_CLASS_DEPS = \
	$(SYSLOG_NG_CORE_PACKAGE_DIR)/LogDestination.class

SyslogNgClassLoader_CLASS_DEPS = \
	$(SYSLOG_NG_CORE_PACKAGE_DIR)/InternalMessageSender.class

TextLogDestination_CLASS_DEPS = \
	$(SYSLOG_NG_CORE_PACKAGE_DIR)/LogDestination.class

DummyTextDestination_CLASS_DEPS = \
	$(SYSLOG_NG_CORE_PACKAGE_DIR)/TextLogDestination.class \
	$(SYSLOG_NG_CORE_PACKAGE_DIR)/InternalMessageSender.class

DummyStructuredDestination_CLASS_DEPS = \
	$(SYSLOG_NG_CORE_PACKAGE_DIR)/StructuredLogDestination.class \
	$(SYSLOG_NG_CORE_PACKAGE_DIR)/InternalMessageSender.class

java-headers: $(JAVA_HEADER_FILES)

java-binaries: $(SYSLOG_NG_CORE_JAR)

$(SYSLOG_NG_CORE_JAR): $(JAVA_CLASS_FILES) $(SYSLOG_NG_CORE_LIBS_DIR)
	$(AM_V_GEN)$(JAR) cvf $(SYSLOG_NG_CORE_JAR) -C $(SYSLOG_NG_CORE_CLASSES_DIR) .

$(SYSLOG_NG_CORE_LIBS_DIR) $(SYSLOG_NG_CORE_CLASSES_DIR) $(SYSLOG_NG_CORE_PACKAGE_DIR) $(JAVA_BUILDDIR):
	mkdir -p $@

$(JAVA_HEADER_FILES): $(JAVA_BUILDDIR)/org_syslog_ng_%.h: $(SYSLOG_NG_CORE_PACKAGE_DIR)/%.class
	$(AM_V_GEN)$(JAVAH) -classpath  $(SYSLOG_NG_CORE_CLASSES_DIR) -d $(JAVA_BUILDDIR) org.syslog_ng.$*

all-local: java-binaries java-headers

java-install-exec-hook:
	$(mkinstalldirs) $(JAVA_DST_DIR)
	cp $(SYSLOG_NG_CORE_JAR) $(JAVA_DST_DIR)

java-clean-hook:
	rm -f $(SYSLOG_NG_CORE_PACKAGE_DIR)/*.class
	rm -f $(SYSLOG_NG_CORE_JAR)
	rm -f $(JAVA_HEADER_FILES)

java-uninstall-exec-hook:
	rm -f $(JAVA_DST_DIR)/*.jar

INSTALL_EXEC_HOOKS += java-install-exec-hook
UNINSTALL_HOOKS += java-uninstall-exec-hook
CLEAN_HOOKS += java-clean-hook

module_LTLIBRARIES += modules/java/libmod-java.la

modules_java_libmod_java_la_CFLAGS = \
    $(AM_CFLAGS) \
    $(JNI_CFLAGS)  \
    $(INCUBATOR_CFLAGS) \
    -I$(top_srcdir)/modules/java    \
    -I$(top_builddir)/modules/java	\
    -I$(top_srcdir)/modules/java/native \
    -I$(top_builddir)/modules/java/native

modules_java_libmod_java_la_SOURCES = \
    modules/java/native/java-grammar.y \
    modules/java/native/java-plugin.c \
    modules/java/native/java-destination.c \
    modules/java/native/java-class-loader.c \
    modules/java/native/java-class-loader.h \
    modules/java/native/java-parser.c \
    modules/java/native/java-parser.h \
    modules/java/native/java_machine.c \
    modules/java/native/java_machine.h \
    modules/java/native/java-destination.h \
    modules/java/native/java-config.c \
    modules/java/native/java-config.h \
    modules/java/proxies/java-destination-proxy.c \
    modules/java/proxies/java-destination-proxy.h \
    modules/java/proxies/java-logmsg-proxy.c \
    modules/java/proxies/java-logmsg-proxy.h \
    modules/java/proxies/java-template-proxy.c \
    modules/java/proxies/java-template-proxy.h \
    modules/java/proxies/internal-message-sender-proxy.c

modules_java_libmod_java_la_LIBADD =  $(JNI_LIBS) $(MODULE_DEPS_LIBS)

modules_java_libmod_java_la_LDFLAGS = \
    -avoid-version -module -no-undefined $(MODULE_LDFLAGS)

modules_java_libmod_java_la_DEPENDENCIES  = \
        $(MODULE_DEPS_LIBS)

JAVA_FILES = \
    InternalMessageSender.java \
    SyslogNgClassLoader.java \
    LogMessage.java \
    LogTemplate.java \
    LogPipe.java \
    LogDestination.java \
    TextLogDestination.java \
    StructuredLogDestination.java \
    DummyTextDestination.java \
    DummyStructuredDestination.java

JAVA_HEADER_FILES = \
   $(JAVA_BUILDDIR)/org_syslog_ng_LogMessage.h \
   $(JAVA_BUILDDIR)/org_syslog_ng_LogDestination.h \
   $(JAVA_BUILDDIR)/org_syslog_ng_LogTemplate.h \
   $(JAVA_BUILDDIR)/org_syslog_ng_InternalMessageSender.h

JAVA_CLASS_FILES = $(patsubst %.java, $(SYSLOG_NG_CORE_PACKAGE_DIR)/%.class, $(JAVA_FILES))
endif

BUILT_SOURCES += \
    modules/java/native/java-grammar.y \
    modules/java/native/java-grammar.c \
    modules/java/native/java-grammar.h

EXTRA_DIST += \
    modules/java/native/java-grammar.ym \
    modules/java/CMakeLists.txt	\
    modules/java/tools/syslog-ng.conf.example	\
    $(patsubst %, $(SYSLOG_NG_CORE_SRC_DIR)/%, $(JAVA_FILES))

if ENABLE_JAVA

BUILT_SOURCES += $(JAVA_HEADER_FILES)
CLEANFILES += $(JAVA_HEADER_FILES)

endif

.SECONDEXPANSION:

$(JAVA_CLASS_FILES): $(SYSLOG_NG_CORE_PACKAGE_DIR)/%.class: $(SYSLOG_NG_CORE_SRC_DIR)/%.java $(SYSLOG_NG_CORE_CLASSES_DIR) $$($$*_CLASS_DEPS)
	$(AM_V_GEN)$(JAVAC) -classpath  $(SYSLOG_NG_CORE_CLASSES_DIR) -d $(SYSLOG_NG_CORE_CLASSES_DIR) $<

.PHONY: java-binaries java-headers java-install-exec-hook java-uninstall-exec-hook java-clean-hook all-local
