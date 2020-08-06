ack_tracker_includedir			= ${pkgincludedir}/ack-tracker

EXTRA_DIST += lib/ack-tracker/CMakeLists.txt \
							lib/ack-tracker/tests/CMakeLists.txt

ack_tracker_include_HEADERS = \
	lib/ack-tracker/ack_tracker.h	\
	lib/ack-tracker/bookmark.h \
	lib/ack-tracker/consecutive_ack_record_container.h \
	lib/ack-tracker/consecutive_ack_tracker.h \
	lib/ack-tracker/instant_ack_tracker.h

ack_tracker_sources = \
	lib/ack-tracker/ack_tracker.c \
	lib/ack-tracker/instant_ack_tracker.c	\
	lib/ack-tracker/consecutive_ack_tracker.c \
	lib/ack-tracker/consecutive_ack_record_container_static.c \
	lib/ack-tracker/consecutive_ack_record_container_dynamic.c

include lib/ack-tracker/tests/Makefile.am
