
if ENABLE_JAVA

JAVA_DST_DIR=$(DESTDIR)/$(moduledir)/java-modules
JARS=$(shell find $(abs_top_builddir)/modules/java/syslog-ng-core -name '*.jar')
GRADLE_WORKDIR=$(abs_top_builddir)/modules/java/.gradle

java-binaries:
	$(AM_V_GEN) $(GRADLE) --project-cache-dir $(GRADLE_WORKDIR) -g $(GRADLE_WORKDIR) build -p $(abs_top_srcdir)/modules/java -PsyslogBuildDir=$(abs_top_builddir)/modules/java build 

java-headers:
	$(AM_V_GEN) $(GRADLE) --project-cache-dir $(GRADLE_WORKDIR) -g $(GRADLE_WORKDIR) build -q -p $(abs_top_srcdir)/modules/java -PsyslogBuildDir=$(abs_top_builddir)/modules/java nativeHeaders

all-local: java-binaries

java-install-exec-hook:
	$(mkinstalldirs) $(JAVA_DST_DIR)
	cp $(JARS) $(JAVA_DST_DIR)

java-clean-hook:
	$(GRADLE) --project-cache-dir $(GRADLE_WORKDIR) -g $(GRADLE_WORKDIR) -q -p $(abs_top_srcdir)/modules/java -PsyslogBuildDir=$(abs_top_builddir)/modules/java clean
	rm -f "$(top_builddir)/modules/java/*.log"
	rm -rf $(abs_top_builddir)/modules/java/.gradle

java-uninstall-exec-hook:
	rm -f $(JAVA_DST_DIR)/*.jar

eclipse:
	$(AM_V_GEN) $(GRADLE) --project-cache-dir $(GRADLE_WORKDIR) -g $(GRADLE_WORKDIR) build -p $(abs_top_srcdir)/modules/java -PsyslogBuildDir=$(abs_top_builddir)/modules/java eclipse


INSTALL_EXEC_HOOKS += java-install-exec-hook
UNINSTALL_HOOKS += java-uninstall-exec-hook
CLEAN_HOOKS += java-clean-hook



module_LTLIBRARIES += modules/java/libmod-java.la

modules_java_libmod_java_la_CFLAGS = \
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
    modules/java/proxies/java-destination-proxy.c \
    modules/java/proxies/java-destination-proxy.h \
    modules/java/proxies/java-logmsg-proxy.c \
    modules/java/proxies/java-logmsg-proxy.h \
    modules/java/proxies/java-template-proxy.c \
    modules/java/proxies/java-template-proxy.h \
    modules/java/proxies/internal-message-sender-proxy.c

modules_java_libmod_java_la_LIBADD =  $(JNI_LIBS)

modules_java_libmod_java_la_LDFLAGS = \
    -avoid-version -module -no-undefined

modules_java_libmod_java_la_DEPENDENCIES  = \
        $(MODULE_DEPS_LIBS)

JAVA_SRC_DIR = modules/java/src/main/java/org/syslog_ng

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
   $(top_builddir)/modules/java/org_syslog_ng_LogDestination.h \
   $(top_builddir)/modules/java/org_syslog_ng_LogMessage.h \
   $(top_builddir)/modules/java/org_syslog_ng_LogTemplate.h \
   $(top_builddir)/modules/java/org_syslog_ng_InternalMessageSender.h

$(JAVA_HEADER_FILES): java-headers

endif

BUILT_SOURCES += \
    modules/java/native/java-grammar.y \
    modules/java/native/java-grammar.c \
    modules/java/native/java-grammar.h

if ENABLE_JAVA
BUILT_SOURCES += $(JAVA_HEADER_FILES)

CLEANFILES += $(JAVA_HEADER_FILES)

endif

EXTRA_DIST += \
    modules/java/native/java-grammar.ym \
    $(JAVA_FILES) \
    modules/java/build.gradle
