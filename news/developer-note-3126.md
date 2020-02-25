`snmp test in Light`: snmp destination test case written in Light test framework

snmp destination related functional tests can be run from Light framework.
This test requires snmptrapd as an external dependency. If you don't want to
run this test, you can use the pytest's marker discovery feature:

```python -m pytest ... -m 'not snmp'``` 

The test is run by syslog-ng's Travis workflow.
