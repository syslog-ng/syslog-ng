set(AFSTREAMS_SOURCES
    afstreams.c
    afstreams.h
    afstreams-parser.c
    afstreams-parser.h
    afstreams-plugin.c
)

include (CheckIncludeFile)
check_include_file(sys/strlog.h HAVE_STRLOG)
check_include_file(stropts.h HAVE_STROPTS)

module_switch(ENABLE_SUN_STREAMS "Enable Sun Streams source" HAVE_STRLOG HAVE_STROPTS)
if (ENABLE_SUN_STREAMS)
  add_module(
    TARGET afstreams
    GRAMMAR afstreams-grammar
    SOURCES ${AFSTREAMS_SOURCES}
  )
endif()
