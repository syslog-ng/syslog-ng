set(GEOIP_SOURCES
    geoip-parser.c
    geoip-parser.h
    geoip-parser-parser.c
    geoip-parser-parser.h
    geoip-plugin.c
    geoip-helper.c
    ${CMAKE_CURRENT_BINARY_DIR}/geoip-parser-grammar.c
    ${CMAKE_CURRENT_BINARY_DIR}/geoip-parser-grammar.h
)

set(GEOIP_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

generate_y_from_ym(modules/geoip/geoip-parser-grammar)

find_package(LibGeoIP)

if(LIBGEOIP_FOUND)
    option(ENABLE_GEOIP "Enable GeoIP parser" ON)
else()
    option(ENABLE_GEOIP "Enable GeoIP parser" OFF)
endif()

if (ENABLE_GEOIP)
    bison_target(GeoIPGrammar
        ${CMAKE_CURRENT_BINARY_DIR}/geoip-parser-grammar.y
        ${CMAKE_CURRENT_BINARY_DIR}/geoip-parser-grammar.c
        COMPILE_FLAGS ${BISON_FLAGS})

    add_library(geoip-plugin SHARED ${GEOIP_SOURCES})
    target_link_libraries (geoip-plugin PUBLIC ${LibGeoIP_LIBRARY})
    target_include_directories (geoip-plugin SYSTEM PRIVATE ${LibGeoIP_INCLUDE_DIR})
    target_include_directories (geoip-plugin PRIVATE ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(geoip-plugin PRIVATE syslog-ng)

    install(TARGETS geoip-plugin LIBRARY DESTINATION lib/syslog-ng/ COMPONENT geoip)

    add_subdirectory(tests)
endif()
