# Accepted requests

...

# Ideas

# Make lots of member functions static

## Reason

* some never use `self` anyway
* some could be easily refactored to not use that
* then we could factor out all these statics of all classes
* then we could reorganize classes
* the end result will be much less coupling, maybe we could even drop `global_config`

# Travis must fail if tests do not pass

Check where exit code gets lost

# GlobalRegistry should be a `singleton`

## Reason

* port & path registration should be global to the program (and ideally to the machine as well)

## log_level should be accessible later on


# throw RetryTestException in transient failure conditions

## Case 1

* if a port is reserved via Registry (it was free at this time)
* it has not been bind()-ed while syslog-ng starts
* it is bind later to test later establishing a connection
* by this time another process could have theoretically bind() by another process
* our bind should fail with an exception
* this exception should be caught outside and retry this test case from the start without logging any error

# GlobalRegister should return initialized objects, not IDs

## if a port needs to be available from the start
    gotten_port = global_registry.bind_me_a_new_port()
    gotten_path = global_registry.get_me_a_file('foo=42,fee=24')
    gotten_csv_path = global_registry.get_me_a_file(content='foo,42,fee,24', extension='csv')
    config.build(gotten_port, gotten_path, gotten_csv_path)

## if a port needs to be unavailable at the start, but available later on
    reserved_port = global_registry.reserve_me_an_unused_port()
    config.build(reserved_port.get_port())
    # start syslog-ng with non-bind() port
    # wait
    reserved_port.bind()
    # send messages
    # destructor could close it

# Scratch notes

...
