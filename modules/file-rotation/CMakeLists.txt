module_switch(ENABLE_FILE_ROTATION "Enable file-rotation")
if (NOT ENABLE_FILE_ROTATION)
  return()
endif()

set(FILE_ROTATION_SOURCES
    file-rotation-parser.h
    file-rotation-plugin.c
    file-rotation-parser.c
    file-rotation.c
    file-rotation.h
)

add_module(
  TARGET file_rotation
  GRAMMAR file-rotation-grammar
  SOURCES ${FILE_ROTATION_SOURCES}
)

add_test_subdirectory(tests)
