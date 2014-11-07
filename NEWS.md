3.7.0alpha1
===========

<!-- Fri, 07 Nov 2014 12:39:45 +0100 -->

This is the first alpha release of the syslog-ng OSE 3.7
branch.

Changes compared to the latest stable release (3.6.1):

Features
--------

 * It is possible to create templates without braces.

 * User defined template-function support added.
   User can define template functions in her/his configuration the same
   way she/he would define a template.

 * $(format-cim) template function added into an SCL module.

 * A new choice for inherit-properties implemented that will merge
   all name-value pairs into the new synthetic message, with the most recent
   being beferred over older values.

Developer notes
---------------

 * Added implementation for user-defined template functions.
   A new API added, user_template_function_register() that allows
   registering a LogTemplate instance as a template function, dynamically.

Credits
-------

syslog-ng is developed as a community project, and as such it relies
on volunteers, to do the work necessarily to produce syslog-ng.

Reporting bugs, testing changes, writing code or simply providing
feedback are all important contributions, so please if you are a user
of syslog-ng, contribute.

We would like to thank the following people for their contribution:

Andras Mitzki, Balazs Scheidler, Fabien Wernli, Gergely Nagy, Laszlo Budai,
Peter Czanik, Viktor Juhasz, Viktor Tusa
