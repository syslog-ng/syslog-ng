if (APPLE)
  enable_language(OBJC)
  set(CMAKE_OBJC_FLAGS  "${CMAKE_OBJC_FLAGS} -fobjc-arc")

  set(DARWINOSL_SOURCES
    darwinosl-plugin.c
    darwinosl-source.m
    darwinosl-source.h
    darwinosl-source-persist.m
    darwinosl-source-persist.h
    darwinosl-parser.c
    darwinosl-parser.h
    darwinosl-grammar.y
    ${CMAKE_CURRENT_BINARY_DIR}/darwinosl-grammar.c
    ${CMAKE_CURRENT_BINARY_DIR}/darwinosl-grammar.h
  )

  include (CheckOBJCSourceCompiles)
  check_objc_source_compiles("
    #include <OSLog/OSLog.h>
    int main(int, char**)
    { return 0; }
  " HAVE_OSLLOG)

  generate_y_from_ym(modules/darwinosl/darwinosl-grammar)

  bison_target(DarwinOSLGrammar
    ${CMAKE_CURRENT_BINARY_DIR}/darwinosl-grammar.y
    ${CMAKE_CURRENT_BINARY_DIR}/darwinosl-grammar.c
  COMPILE_FLAGS ${BISON_FLAGS})

  module_switch(ENABLE_DARWIN_OSL "Enable Darwin OSLog source" HAVE_OSLLOG)
  if (ENABLE_DARWIN_OSL)
    add_module(
      TARGET darwinosl
      GRAMMAR darwinosl-grammar
      SOURCES ${DARWINOSL_SOURCES}
    )
    target_link_libraries(darwinosl PRIVATE
      "-framework Foundation"
      "-framework OSLog"
    )
    target_link_options(darwinosl PRIVATE -ObjC)
  endif()
endif()
