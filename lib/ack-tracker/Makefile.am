ack_tracker_includedir			= ${pkgincludedir}/ack-tracker

EXTRA_DIST += lib/ack-tracker/CMakeLists.txt \
							lib/ack-tracker/tests/CMakeLists.txt

ack_tracker_include_HEADERS = \
	lib/ack-tracker/ack_tracker.h	\
	lib/ack-tracker/ack_tracker_factory.h \
	lib/ack-tracker/ack_tracker_types.h \
	lib/ack-tracker/bookmark.h \
	lib/ack-tracker/batched_ack_tracker.h \
	lib/ack-tracker/consecutive_ack_record_container.h \
	lib/ack-tracker/consecutive_ack_tracker.h \
	lib/ack-tracker/instant_ack_tracker.h

ack_tracker_sources = \
	lib/ack-tracker/ack_tracker_factory.c \
	lib/ack-tracker/batched_ack_tracker.c \
	lib/ack-tracker/batched_ack_tracker_factory.c \
	lib/ack-tracker/instant_ack_tracker.c \
	lib/ack-tracker/instant_ack_tracker_bookmarkless.c	\
	lib/ack-tracker/consecutive_ack_tracker.c \
	lib/ack-tracker/consecutive_ack_tracker_factory.c \
	lib/ack-tracker/consecutive_ack_record_container_static.c \
	lib/ack-tracker/consecutive_ack_record_container_dynamic.c \
	lib/ack-tracker/instant_ack_tracker_factory.c \
	lib/ack-tracker/instant_ack_tracker_bookmarkless_factory.c

include lib/ack-tracker/tests/Makefile.am
