# Accepted requests

...

# Ideas

# Travis must fail if tests do not pass

Check where exit code gets lost

# GlobalRegistry should be a `singleton`

Reason

* port & path registration should be global to the program (and ideally to the machine as well)

## log_level should be accessible later on


# throw RetryTestException if transient failure conditions

Case 1

* if a port is reserved via Registry (it was free at this time)
* it is not bind() while syslog-ng starts
* it is bind later to test later establishing a connection
* by this time another process could have theoretically bind() by another process
* our bind should fail with an exception
* this exception should be caught outside and retry this test case from the start without logging any error

# Scratch notes

...
