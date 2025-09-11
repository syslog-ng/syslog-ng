set(AFSTREAMS_SOURCES
    afstreams.c
    afstreams.h
    afstreams-parser.c
    afstreams-parser.h
    afstreams-plugin.c
)

include(CheckCSourceCompiles)

# AIX has these, but cannot be linked and used, not supported anymore
check_c_source_compiles("
  #include <sys/strlog.h>
  struct strbuf b;
  int main(void) {
    struct log_ctl lc;
    lc.mid = 0;
    return 0;
  }
" HAVE_STRLOG)

check_c_source_compiles("
  #include <stropts.h>
  int main(void) {
    int x = I_PUSH;
    return x;
  }
" HAVE_STROPTS)

module_switch(ENABLE_SUN_STREAMS "Enable Sun Streams source" HAVE_STRLOG HAVE_STROPTS)
if (ENABLE_SUN_STREAMS)
  add_module(
    TARGET afstreams
    GRAMMAR afstreams-grammar
    SOURCES ${AFSTREAMS_SOURCES}
  )
endif()
