set(REDIS_HEADERS
    "${CMAKE_CURRENT_BINARY_DIR}/redis-grammar.h"
    "redis-parser.h"
    "redis.h"
)

set(REDIS_SOURCES
    "${CMAKE_CURRENT_BINARY_DIR}/redis-grammar.c"
    "redis-parser.c"
    "redis.c"
)

generate_y_from_ym(modules/redis/redis-grammar)

find_package(Hiredis)

module_switch(ENABLE_REDIS "Enable redis module" HIREDIS_FOUND)
if (ENABLE_REDIS)
  bison_target(RedisGrammar
      ${CMAKE_CURRENT_BINARY_DIR}/redis-grammar.y
      ${CMAKE_CURRENT_BINARY_DIR}/redis-grammar.c
      COMPILE_FLAGS ${BISON_FLAGS})

  add_library(redis MODULE ${REDIS_SOURCES})
  target_include_directories(redis
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${CMAKE_CURRENT_BINARY_DIR}
  )
  target_include_directories (redis PRIVATE ${HIREDIS_INCLUDE_DIR})
  target_link_libraries(redis PRIVATE syslog-ng ${HIREDIS_LIBRARIES})

  install(TARGETS redis LIBRARY DESTINATION lib/syslog-ng/ COMPONENT redis)
endif()
