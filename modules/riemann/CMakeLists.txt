set (RIEMANN_SOURCES
    riemann-grammar.y
    riemann.c
    riemann.h
    riemann-parser.c
    riemann-parser.h
    riemann-plugin.c
    ${CMAKE_CURRENT_BINARY_DIR}/riemann-grammar.c
    ${CMAKE_CURRENT_BINARY_DIR}/riemann-grammar.h
)

generate_y_from_ym(modules/riemann/riemann-grammar)

bison_target(RiemannGrammar
    ${CMAKE_CURRENT_BINARY_DIR}/riemann-grammar.y
    ${CMAKE_CURRENT_BINARY_DIR}/riemann-grammar.c
    COMPILE_FLAGS ${BISON_FLAGS})

find_package(RiemannClient)

if (Riemann_FOUND)
    option(ENABLE_RIEMANN "Enable riemann destination" ON)
else()
    option(ENABLE_RIEMANN "Enable riemann destination" OFF)
endif()

if (ENABLE_RIEMANN)
    add_library(riemann MODULE ${RIEMANN_SOURCES})
    target_include_directories (riemann PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_include_directories (riemann PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
    target_include_directories (riemann PRIVATE ${Riemann_INCLUDE_DIR})
    target_link_libraries(riemann PRIVATE syslog-ng ${Riemann_LIBRARIES})

    install(TARGETS riemann LIBRARY DESTINATION lib/syslog-ng/)
endif()
