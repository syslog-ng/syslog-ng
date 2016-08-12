functional test cases for pytest

# Common functionality

## Simple cases that are expected to succeed

Sequence:

* load config
* wait for startup
* message workflow
    * feed 2 messages
        1. to see whether it works,
        1. second one to check that internal state is not corrupted after the first one)
    * wait for processing
    * inspect processed messages at destination
* reload config
    * message workflow

## Specific cases

Options:

* fault injection to source/destination
* option fuzzing
* ...

# Imported from tests/functional

## Python

* source:
   * anything
* destination:
   * Python destination given class from `tests/functional/sngtestmod.py`
* precondition
    * if Python module is not available, skip test and give warning
* operation:
    * feed messages
    * expect to encounter the transformed messages in the file written by the Python module (or adjust Python module to only use standard output)
* Relevant config snippet:

```
destination d_python {
    python(class(sngtestmod.DestTest)
           value-pairs(key('MSG') pair('HOST', 'bzorp') pair('DATE', '$ISODATE') key('MSGHDR')));
};
```


## input driver

ok, 1 test/function

..... TODO


## filter

* source
    * anything
* destination
    * anything
* filter
    * various levels
        * debug
        * info
        * notice
        * warning..crit
    * various facilities
        * syslog
        * kern
        * mail
        * daemon,auth,lpr
* check whether a message is picked up by the corresponding filter and only that filter
* Relevant config snippet:

```
options { ts_format(iso); chain_hostnames(no); keep_hostname(yes); threaded(yes); };
```


# New tests

## file source & file destination

new test


# Tests which are out of scopoe

## SQL

not yet

## performance

drop

## file source wildcard

drop
