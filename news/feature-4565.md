add metrics for message delays: a new metric is introduced that measures the
delay the messages accumulate while waiting to be delivered by syslog-ng.
The measurement is sampled, e.g. syslog-ng would take the very first message
in every second and expose its delay as a value of the new metric.

There are two new metrics:
  * syslogng_output_message_delay_sample_seconds -- contains the latency of
    outgoing messages
  * output_message_delay_sample_age_seconds -- contains the age of the last
    measurement, relative to the current time.
