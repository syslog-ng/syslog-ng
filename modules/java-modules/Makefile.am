if ENABLE_JAVA_MODULES


GRADLE_PROJECT_DIR ?= $(abs_top_srcdir)/modules/java-modules
GRADLE_PROJECT_CACHE_DIR_DEFAULT = $(abs_top_builddir)/modules/java-modules/.gradle
GRADLE_PROJECT_CACHE_DIR ?= $(GRADLE_PROJECT_CACHE_DIR_DEFAULT)
GRADLE_FLAGS ?=
GRADLE_ARGS = $(GRADLE_FLAGS) \
	--project-dir $(GRADLE_PROJECT_DIR) \
	--project-cache-dir=$(GRADLE_PROJECT_CACHE_DIR) \
	-PsyslogBuildDir=$(abs_top_builddir)/modules/java-modules \
	-PsyslogDepsDir=$(abs_top_builddir)/modules/java/syslog-ng-core/libs
GRADLE_COMMAND = $(AM_V_GEN) $(GRADLE) $(GRADLE_ARGS)

JAVA_MODULEDIR=$(DESTDIR)/$(moduledir)/java-modules
JAVA_BUILT_JARS=$(shell find $(abs_top_builddir)/modules/java-modules -name '*.jar' -not -path "$(GRADLE_PROJECT_CACHE_DIR)/*")
JAVA_INSTALLED_JARS=$(shell find $(JAVA_MODULEDIR) -name '*.jar')

java-modules: $(SYSLOG_NG_CORE_JAR)
	$(GRADLE_COMMAND) build

all-local: java-modules

log4j-copy-jar:
	$(GRADLE_COMMAND) -PjarDestDir=$(JAVA_MODULEDIR) copyLog4j

jest-copy-jar:
	$(GRADLE_COMMAND) -PjarDestDir=$(JAVA_MODULEDIR) copyJestRuntimeDeps

java-modules-install-exec-hook: log4j-copy-jar jest-copy-jar
	$(mkinstalldirs) $(JAVA_MODULEDIR)
	cp $(JAVA_BUILT_JARS) $(JAVA_MODULEDIR)

java-modules-uninstall-exec-hook:
	rm -f $(JAVA_INSTALLED_JARS)

java-modules-clean-hook:
	$(GRADLE_COMMAND) clean
	rm -f $(abs_top_builddir)/modules/java-modules/*.log
	[ "$(GRADLE_PROJECT_CACHE_DIR)" = "$(GRADLE_PROJECT_CACHE_DIR_DEFAULT)" ] && rm -rf "$(GRADLE_PROJECT_CACHE_DIR)" || true

INSTALL_EXEC_HOOKS += java-modules-install-exec-hook
UNINSTALL_HOOKS += java-modules-uninstall-exec-hook
CLEAN_HOOKS += java-modules-clean-hook

endif

EXTRA_DIST += \
	modules/java-modules/common/build.gradle  \
	modules/java-modules/elastic-v2/build.gradle \
	modules/java-modules/kafka/build.gradle \
	modules/java-modules/hdfs/build.gradle    \
	modules/java-modules/dummy/build.gradle   \
	modules/java-modules/http/build.gradle   \
	modules/java-modules/settings.gradle \
	modules/java-modules/build.gradle \
	modules/java-modules/internal_repositories.gradle \
	modules/java-modules/kafka/src/main/java/org/syslog_ng/KafkaDestination.java \
	modules/java-modules/kafka/src/main/java/org/syslog_ng/KafkaDestinationOptions.java \
	modules/java-modules/kafka/src/main/java/org/syslog_ng/KafkaDestinationProperties.java \
	modules/java-modules/common/src/main/java/org/syslog_ng/logging/SyslogNgInternalLogger.java \
	modules/java-modules/common/src/main/java/org/syslog_ng/options/InvalidOptionException.java \
	modules/java-modules/common/src/main/java/org/syslog_ng/options/RequiredOptionDecorator.java \
	modules/java-modules/common/src/main/java/org/syslog_ng/options/Option.java \
	modules/java-modules/common/src/main/java/org/syslog_ng/options/TemplateOption.java \
	modules/java-modules/common/src/main/java/org/syslog_ng/options/PortCheckDecorator.java \
	modules/java-modules/common/src/main/java/org/syslog_ng/options/OptionDecorator.java \
	modules/java-modules/common/src/main/java/org/syslog_ng/options/BooleanOptionDecorator.java \
	modules/java-modules/common/src/main/java/org/syslog_ng/options/EnumOptionDecorator.java \
	modules/java-modules/common/src/main/java/org/syslog_ng/options/Options.java \
	modules/java-modules/common/src/main/java/org/syslog_ng/options/IntegerRangeCheckOptionDecorator.java \
	modules/java-modules/common/src/main/java/org/syslog_ng/options/IntegerOptionDecorator.java \
	modules/java-modules/common/src/main/java/org/syslog_ng/options/StringOption.java \
	modules/java-modules/common/src/test/java/org/syslog_ng/options/test/TestEnumOptionDecorator.java \
	modules/java-modules/common/src/test/java/org/syslog_ng/options/test/TestRequiredOptionDecorator.java \
	modules/java-modules/common/src/test/java/org/syslog_ng/options/test/TestStringOption.java \
	modules/java-modules/common/src/test/java/org/syslog_ng/options/test/TestPortCheckDecorator.java \
	modules/java-modules/common/src/test/java/org/syslog_ng/options/test/TestBooleanOptionDecorator.java \
	modules/java-modules/common/src/test/java/org/syslog_ng/options/test/TestIntegerRangeCheckOptionDecorator.java \
	modules/java-modules/common/src/test/java/org/syslog_ng/options/test/TestOption.java \
	modules/java-modules/common/src/test/java/org/syslog_ng/options/test/TestIntegerOptionDecorator.java \
	modules/java-modules/common/src/test/java/org/syslog_ng/logging/test/MockLogDestination.java \
	modules/java-modules/hdfs/src/main/java/org/syslog_ng/hdfs/HdfsDestination.java \
	modules/java-modules/hdfs/src/main/java/org/syslog_ng/hdfs/HdfsOptions.java \
	modules/java-modules/hdfs/src/main/java/org/syslog_ng/hdfs/HdfsFile.java \
	modules/java-modules/http/src/main/java/org/syslog_ng/http/HTTPDestination.java \
	modules/java-modules/http/src/main/java/org/syslog_ng/http/HTTPDestinationOptions.java \
	modules/java-modules/dummy/src/main/java/org/syslog_ng/DummyTextDestination.java \
	modules/java-modules/elastic-v2/src/main/java/org/syslog_ng/elasticsearch_v2/messageprocessor/ESIndex.java \
	modules/java-modules/elastic-v2/src/main/java/org/syslog_ng/elasticsearch_v2/messageprocessor/ESMessageProcessor.java \
	modules/java-modules/elastic-v2/src/main/java/org/syslog_ng/elasticsearch_v2/messageprocessor/esnative/ESNativeMessageProcessor.java \
	modules/java-modules/elastic-v2/src/main/java/org/syslog_ng/elasticsearch_v2/messageprocessor/esnative/DummyProcessorNative.java \
	modules/java-modules/elastic-v2/src/main/java/org/syslog_ng/elasticsearch_v2/messageprocessor/esnative/ESBulkNativeMessageProcessor.java \
	modules/java-modules/elastic-v2/src/main/java/org/syslog_ng/elasticsearch_v2/messageprocessor/ESMessageProcessorFactory.java \
	modules/java-modules/elastic-v2/src/main/java/org/syslog_ng/elasticsearch_v2/messageprocessor/esnative/ESSingleNativeMessageProcessor.java \
	modules/java-modules/elastic-v2/src/main/java/org/syslog_ng/elasticsearch_v2/ElasticSearchDestination.java \
	modules/java-modules/elastic-v2/src/main/java/org/syslog_ng/elasticsearch_v2/client/ESClient.java \
	modules/java-modules/elastic-v2/src/main/java/org/syslog_ng/elasticsearch_v2/client/esnative/ESTransportClient.java \
	modules/java-modules/elastic-v2/src/main/java/org/syslog_ng/elasticsearch_v2/client/esnative/ESNativeClient.java \
	modules/java-modules/elastic-v2/src/main/java/org/syslog_ng/elasticsearch_v2/client/esnative/ESNodeClient.java \
	modules/java-modules/elastic-v2/src/main/java/org/syslog_ng/elasticsearch_v2/client/UnknownESClientModeException.java \
	modules/java-modules/elastic-v2/src/main/java/org/syslog_ng/elasticsearch_v2/client/ESClientFactory.java \
	modules/java-modules/elastic-v2/src/main/java/org/syslog_ng/elasticsearch_v2/ElasticSearchOptions.java \
	modules/java-modules/elastic-v2/src/main/java/org/syslog_ng/elasticsearch_v2/client/esnative/ESTransportSearchGuardClient.java \
	modules/java-modules/elastic-v2/src/main/java/org/syslog_ng/elasticsearch_v2/client/http/ESHttpClient.java \
	modules/java-modules/elastic-v2/src/main/java/org/syslog_ng/elasticsearch_v2/client/http/ESHttpsClient.java \
	modules/java-modules/elastic-v2/src/main/java/org/syslog_ng/elasticsearch_v2/messageprocessor/http/HttpBulkMessageProcessor.java \
	modules/java-modules/elastic-v2/src/main/java/org/syslog_ng/elasticsearch_v2/messageprocessor/http/HttpMessageProcessor.java \
	modules/java-modules/elastic-v2/src/main/java/org/syslog_ng/elasticsearch_v2/messageprocessor/http/HttpSingleMessageProcessor.java \
	modules/java-modules/kafka/src/test/java/org/syslog_ng/test/MockLogDestination.java \
	modules/java-modules/kafka/src/test/java/org/syslog_ng/test/TestBooleanOptionDecorator.java \
	modules/java-modules/kafka/src/test/java/org/syslog_ng/test/TestEnumOptionDecorator.java \
	modules/java-modules/kafka/src/test/java/org/syslog_ng/test/TestIntegerOptionDecorator.java \
	modules/java-modules/kafka/src/test/java/org/syslog_ng/test/TestIntegerRangeCheckOptionDecorator.java \
	modules/java-modules/kafka/src/test/java/org/syslog_ng/test/TestOption.java \
	modules/java-modules/kafka/src/test/java/org/syslog_ng/test/TestPortCheckDecorator.java \
	modules/java-modules/kafka/src/test/java/org/syslog_ng/test/TestRequiredOptionDecorator.java \
	modules/java-modules/kafka/src/test/java/org/syslog_ng/test/TestStringOption.java \
	modules/java-modules/CMakeLists.txt \
	modules/java-modules/common/README.md	\
	modules/java/README.md
