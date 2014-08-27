include modules/syslogformat/Makefile.am
include modules/afsocket/Makefile.am
include modules/afsql/Makefile.am
include modules/afstreams/Makefile.am
include modules/affile/Makefile.am
include modules/afprog/Makefile.am
include modules/afuser/Makefile.am
include modules/afamqp/Makefile.am
include modules/afmongodb/Makefile.am
include modules/afsmtp/Makefile.am
include modules/csvparser/Makefile.am
include modules/confgen/Makefile.am
include modules/system-source/Makefile.am
include modules/linux-kmsg-format/Makefile.am
include modules/pacctformat/Makefile.am
include modules/basicfuncs/Makefile.am
include modules/cryptofuncs/Makefile.am
include modules/dbparser/Makefile.am
include modules/json/Makefile.am
include modules/tfgeoip/Makefile.am
include modules/afstomp/Makefile.am
include modules/redis/Makefile.am
include modules/pseudofile/Makefile.am
include modules/graphite/Makefile.am
include modules/riemann/Makefile.am
include modules/systemd-journal/Makefile.am

SYSLOG_NG_MODULES	=	\
	mod-afsocket mod-afstreams mod-affile mod-afprog \
	mod-usertty mod-amqp mod-mongodb mod-smtp mod-json \
	mod-syslogformat mod-linux-kmsg mod-pacctformat \
	mod-confgen mod-system-source mod-csvparser mod-dbparser \
	mod-basicfuncs mod-cryptofuncs mod-geoip mod-afstomp \
	mod-redis mod-pseudofile mod-graphite mod-riemann

modules modules/: ${SYSLOG_NG_MODULES}

modules_test_subdirs	=	\
	modules_afsocket modules_afstreams modules_affile \
	modules_afprog modules_usertty modules_afamqp \
	modules_afmongodb modules_afsmtp modules_json \
	modules_syslogformat modules_linux_kmsg \
	modules_pacctformat modules_confgen modules_system_source \
	modules_csvparser modules_dbparser modules_basicfuncs \
	modules_cryptofuncs modules_geoip modules_afstomp \
	modules_graphite modules_riemann

.PHONY: modules modules/
