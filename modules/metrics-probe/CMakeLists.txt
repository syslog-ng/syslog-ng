set(METRICS_PROBE_SOURCES
    metrics-probe.c
    metrics-probe.h
    metrics-probe-parser.c
    metrics-probe-parser.h
    metrics-probe-plugin.c
)

add_module(
  TARGET metrics-probe
  GRAMMAR metrics-probe-grammar
  SOURCES ${METRICS_PROBE_SOURCES}
)

add_test_subdirectory(tests)
