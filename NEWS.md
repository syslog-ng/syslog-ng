3.31.2
======

## Bugfixes

 * `syslog-parser()`: fix a potential crash in case parsing the message
   fails and tags are already applied to the message.

## Packaging

 * `python2`: Direct `python2` support is dropped, we no longer test it in our CI.
   No `python2` related source codes were removed as for now, but we do not
   guarantee that it will work in the future.

   ([#3603](https://github.com/syslog-ng/syslog-ng/pull/3603))

## Credits

syslog-ng is developed as a community project, and as such it relies
on volunteers, to do the work necessarily to produce syslog-ng.

Reporting bugs, testing changes, writing code or simply providing
feedback are all important contributions, so please if you are a user
of syslog-ng, contribute.

We would like to thank the following people for their contribution:


Andras Mitzki, Antal Nemes, Attila Szakacs, Balazs Scheidler,
Gabor Nagy, Laszlo Budai, Laszlo Szemere, László Várady,
Norbert Takacs, Peter Kokai, Ryan Faircloth, Zoltan Pallagi
