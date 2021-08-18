set(STATS_AGGREGATOR_HEADERS
    stats/aggregator/stats-aggregator.h
    stats/aggregator/stats-aggregator-registry.h
    PARENT_SCOPE)

set(STATS_AGGREGATOR_SOURCES
    stats/aggregator/stats-aggregator.c
    stats/aggregator/stats-average.c
    stats/aggregator/stats-maximum.c
    stats/aggregator/stats-change-per-second.c
    stats/aggregator/stats-aggregator-registry.c
    PARENT_SCOPE)
