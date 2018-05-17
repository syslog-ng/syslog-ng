set(SYSLOG_NG_REDIS_QUEUE_SOURCES
    redisq-options.h
    redisq-options.c
)

add_library(syslog-ng-redis-queue ${SYSLOG_NG_REDIS_QUEUE_SOURCES})
target_link_libraries(syslog-ng-redis-queue PUBLIC syslog-ng)

set(REDIS_QUEUE_SOURCES
    redisq.c
    redisq.h
    redisq-parser.c
    redisq-parser.h
    redisq-plugin.c
    ${CMAKE_CURRENT_BINARY_DIR}/redisq-grammar.c
    ${CMAKE_CURRENT_BINARY_DIR}/redisq-grammar.h
)

generate_y_from_ym(modules/redisq/redisq-grammar)

bison_target(RedisQueueGrammar
    ${CMAKE_CURRENT_BINARY_DIR}/redisq-grammar.y
    ${CMAKE_CURRENT_BINARY_DIR}/redisq-grammar.c
    COMPILE_FLAGS ${BISON_FLAGS})

add_library(redis-queue SHARED ${REDIS_QUEUE_SOURCES})
target_link_libraries(redis-queue PRIVATE syslog-ng-redis-queue)
target_include_directories(redis-queue
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
  PRIVATE ${CMAKE_CURRENT_BINARY_DIR}
)
target_link_libraries(redis-queue PRIVATE syslog-ng)

install(TARGETS redis-queue LIBRARY DESTINATION lib/syslog-ng COMPONENT redisqueue)

add_test_subdirectory(tests)
