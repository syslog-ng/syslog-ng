set(PSEUDOFILE_SOURCES
    pseudofile.c
    pseudofile.h
    pseudofile-parser.c
    pseudofile-parser.h
    pseudofile-plugin.c
)

add_module(
  TARGET pseudofile
  GRAMMAR pseudofile-grammar
  SOURCES ${PSEUDOFILE_SOURCES}
)

