`redis`: add batching support 

Performance improvement is significant (at least +200%, but it peaked at almost +500%)

I used the following configuration to enable batching:
```
destination d_redis {
    redis(
        host("localhost")
        port(6379)
        command("HINCRBY", "hosts", "$HOST", "1")
        workers(8)
        log-fifo-size(100000)
        batch-lines(100)
        batch-timeout(10000)
    );
};
```
