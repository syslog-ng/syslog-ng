`log` paths: Added option to create named log paths.

E.g.:
```
log top-level {
    source(s_local);

    log inner-1 {
        filter(f_inner_1);
        destination(d_local_1);
    };

    log inner-2 {
        filter(f_inner_2);
        destination(d_local_2);
    };
};
```

Each named log path counts its ingress and egress messages:
```
syslogng_log_path_ingress{id="top-level"} 114
syslogng_log_path_ingress{id="inner-1"} 114
syslogng_log_path_ingress{id="inner-2"} 114
syslogng_log_path_egress{id="top-level"} 103
syslogng_log_path_egress{id="inner-1"} 62
syslogng_log_path_egress{id="inner-2"} 41
```

Note that the egress statistics only count the messages which have been have not been filtered out from the related
log path, it does care about whether there are any destinations in it or that any destination delivers or drops the message.
