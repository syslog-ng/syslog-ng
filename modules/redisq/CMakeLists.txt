set(REDIS_QUEUE_HEADERS
    "${CMAKE_CURRENT_BINARY_DIR}/redisq-grammar.h"
    "redisq-parser.h"
    "redisq.h"
    "logqueue-redis.h"
    "redisq-options.h"
)

set(REDIS_QUEUE_SOURCES
    "${CMAKE_CURRENT_BINARY_DIR}/redisq-grammar.c"
    "redisq-parser.c"
    "redisq.c"
    "logqueue-redis.c"
    "redisq-options.c"
)

generate_y_from_ym(modules/redisq/redisq-grammar)

find_package(Hiredis)

if (HIREDIS_FOUND)
  option(ENABLE_REDIS_QUEUE "Enable redis module" ON)
else()
  option(ENABLE_REDIS_QUEUE "Enable redis module" OFF)
endif()

if (ENABLE_REDIS_QUEUE)

  bison_target(RedisQueueGrammar
    ${CMAKE_CURRENT_BINARY_DIR}/redisq-grammar.y
    ${CMAKE_CURRENT_BINARY_DIR}/redisq-grammar.c
    COMPILE_FLAGS ${BISON_FLAGS})

  add_library(redis-queue SHARED ${REDIS_QUEUE_SOURCES})

  target_include_directories(redis-queue
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
    
  target_include_directories (redis-queue PRIVATE ${HIREDIS_INCLUDE_DIR})
    
  target_link_libraries(redis-queue PRIVATE syslog-ng ${HIREDIS_LIBRARIES})
  install(TARGETS redis-queue LIBRARY DESTINATION lib/syslog-ng COMPONENT redisqueue)

  add_executable(rqtool rqtool.c)
  target_link_libraries(rqtool PRIVATE eventlog syslog-ng-redis-queue ${HIREDIS_LIBRARIES})
  target_include_directories(rqtool PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

endif()

add_test_subdirectory(tests)
