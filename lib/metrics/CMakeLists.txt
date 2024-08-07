set(METRICS_HEADERS
    metrics/metrics.h
    metrics/dyn-metrics-store.h
    metrics/metrics-tls-cache.h
    metrics/metrics-template.h
    metrics/label-template.h
    PARENT_SCOPE)

set(METRICS_SOURCES
    metrics/metrics.c
    metrics/dyn-metrics-store.c
    metrics/metrics-tls-cache.c
    metrics/metrics-template.c
    metrics/label-template.c
    PARENT_SCOPE)

#add_test_subdirectory(tests)
