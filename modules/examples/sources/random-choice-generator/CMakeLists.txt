if(NOT ENABLE_CPP)
  return()
endif()

# C++ code

set(RANDOM_CHOICE_GENERATOR_CPP_SOURCES
  random-choice-generator.cpp
  random-choice-generator.hpp
  random-choice-generator.h
)

add_module(
  TARGET random-choice-generator-cpp
  SOURCES ${RANDOM_CHOICE_GENERATOR_CPP_SOURCES}
  LIBRARY_TYPE STATIC
)

# C code

set(RANDOM_CHOICE_GENERATOR_SOURCES
  random-choice-generator-parser.c
  random-choice-generator-parser.h
)

add_module(
  TARGET random-choice-generator
  GRAMMAR random-choice-generator-grammar
  DEPENDS random-choice-generator-cpp
  SOURCES ${RANDOM_CHOICE_GENERATOR_SOURCES}
  LIBRARY_TYPE STATIC # STATIC: Built as part of a larger libexamples module
)
