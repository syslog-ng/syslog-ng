option(ENABLE_APPMODEL "Enable AppModel" ON)

if (ENABLE_APPMODEL)

set (APPMODEL_SOURCES
    appmodel.c
    appmodel-parser.c
    appmodel-plugin.c
    appmodel-context.c
    app-parser-generator.c
    application.c
    ${CMAKE_CURRENT_BINARY_DIR}/appmodel-grammar.c
)

set (APPMODEL_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

generate_y_from_ym(modules/appmodel/appmodel-grammar)

bison_target(AppModelGrammar
    ${CMAKE_CURRENT_BINARY_DIR}/appmodel-grammar.y
    ${CMAKE_CURRENT_BINARY_DIR}/appmodel-grammar.c
    COMPILE_FLAGS ${BISON_FLAGS})

add_library(appmodel SHARED ${APPMODEL_SOURCES})
target_include_directories (appmodel PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(appmodel PRIVATE syslog-ng)

install(TARGETS appmodel LIBRARY DESTINATION lib/syslog-ng/ COMPONENT appmodel)

add_test_subdirectory(tests)

endif()
