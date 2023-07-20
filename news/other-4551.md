metrics: replace `driver_instance` (`stats_instance`) with metric labels

The new metric system had a label inherited from legacy: `driver_instance`.

This non-structured label has been removed and different driver-specific labels have been added instead, for example:

Before:
```
syslogng_output_events_total{driver_instance="mongodb,localhost:27017,defaultdb,,coll",id="#anon-destination1#1",result="queued"} 4
```

After:
```
syslogng_output_events_total{driver="mongodb",host="localhost:27017",database="defaultdb",collection="coll",id="#anon-destination1#1",result="queued"} 4
```

This change may affect legacy stats outputs (`syslog-ng-ctl stats`), for example, `persist-name()`-based naming
is no longer supported in this old format.
