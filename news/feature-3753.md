`stats`: create new statistics counter

the new statistics counters:
- msg_size_max: The current largest message size of the given source/destination.
- msg_size_avg: The current average message size of the given source/destination.
- batch_size_max: When batching is enabled, then this shows the current largest batch size of the given source/destination.
- batch_size_avg: When batching is enabled, then this shows the current average batch size of the given source/destination.


- eps_last_1h: The last hour eps value.
- eps_last_24h: The last day eps value.
- eps_since_start: Since driver beggining.
    Note:
        - source side: incoming raw message length
        - destination side: outgoing formated message length
        - This counters is updated every 60 second!
        - The eps means event per second, in this case event means message arrive/send out. This value only an approximate value [importent!].
