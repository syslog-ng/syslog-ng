export abs_top_srcdir
export abs_top_builddir

AUTOMAKE_OPTIONS = subdir-objects

if ENABLE_JAVA

JAVA_MOD_DST_DIR=$(DESTDIR)/$(moduledir)/java-modules
JARS=$(shell find $(abs_top_builddir)/modules/java-modules -name '*.jar')

all-local:
	gradle -p $(abs_top_srcdir)/modules/java-modules -PsyslogBuildDir=$(abs_top_builddir)/modules/java-modules -PsyslogDepsDir=$(abs_top_builddir)/modules/java build

install-exec-local:
	$(mkinstalldirs) $(JAVA_MOD_DST_DIR)
	cp $(JARS) $(JAVA_MOD_DST_DIR)

uninstall-local:
	rm -f $(JAVA_MOD_DST_DIR)/*.jar

endif

EXTRA_DIST = \
	settings.gradle 	\
	dummy/build.gradle	\
	dummy/src/main/java/org/syslog_ng/DummyTextDestination.java  \
	hdfs/build.gradle	\
	hdfs/src/main/java/org/syslog_ng/hdfs/HdfsDestination.java

