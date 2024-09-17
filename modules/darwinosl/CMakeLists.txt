if(ENABLE_OBJC AND ENABLE_DARWIN_OSL)
  set(CMAKE_OBJC_FLAGS "${CMAKE_OBJC_FLAGS} -fobjc-arc")

  # Take care, this must follow any compiler setting overrides! (e.g. set(CMAKE_OBJC_COMPILER clang) etc.)
  enable_language(OBJC)

  set(DARWINOSL_SOURCES
    darwinosl-plugin.c
    darwinosl-source.m
    darwinosl-source.h
    darwinosl-source-oslog.m
    darwinosl-source-oslog.h
    darwinosl-source-persist.c
    darwinosl-source-persist.h
    darwinosl-parser.c
    darwinosl-parser.h
    darwinosl-grammar.y
    ${CMAKE_CURRENT_BINARY_DIR}/darwinosl-grammar.c
    ${CMAKE_CURRENT_BINARY_DIR}/darwinosl-grammar.h
  )

  if(ENABLE_DARWIN_OSL)
    include(CheckOBJCSourceCompiles)

    # TODO: Once gcc was able to compile our ObjC code correctly we should add here a more sopgisticated check
    check_objc_source_compiles("
      #include <OSLog/OSLog.h>
      int main(int, char**)
      { return 0; }
    " HAVE_OSLOG)

    if(HAVE_OSLOG)
      generate_y_from_ym(modules/darwinosl/darwinosl-grammar)

      bison_target(DarwinOSLGrammar
        ${CMAKE_CURRENT_BINARY_DIR}/darwinosl-grammar.y
        ${CMAKE_CURRENT_BINARY_DIR}/darwinosl-grammar.c
        COMPILE_FLAGS ${BISON_FLAGS})

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
    else()
      message(STATUS "OSLog is not supported on this system")
    endif()
  endif()
endif()
