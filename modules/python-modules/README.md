# syslog-ng Python modules

syslog-ng has a quite comprehensive support for implementing various logging
components in Python. The goal of this file is to document how to use this
functionality.

## When to use Python

These Python bindinds are useful if the facilities provided by the
syslog-ng configuration language is not sufficient, that is:

  * if there's no native functionality that would implement the integration with
    a specific service (e.g. API based log sources or information sources that
    you want to use for data enrichment purposes)
  * if the configuration language does not have the features to do some
    specific transformation need (but in that case, please let me know about
    your usecase)
  * if you intend to work on some complex data structures (e.g. JSON) which
    is more natural in a real programming language.

While Python is very powerful and you can produce clean and production ready
solutions using it, the drawback is usually performance. Python code tends
to be slower than the native functionality that syslog-ng provides.

To offset this impact of performance degradation, it's a good strategy to
only process a subset of the incoming log stream via Python code and to use
native configuration elements to select which subset is traversing said
Python code.


## Creating and Storing the Python code

syslog-ng is able to work with both Python code directly embedded into its
main configuration file or to work with Python modules.

### Embedding Python into syslog-ng configuration


You can simply use a top-level `python {}` block to embed your Python code,
like this:
```
@version: 4.0

python {

def template_function(msg):
    return b"Hello World from Python! Original message: " + msg['MSGHDR'] + msg['MESSAGE']


};

log {
    source { tcp(port(2000)); };
    destination { file("logfile" template("$ISODATE $(python template_function)")); };
};
```

### Using Python modules

You can also put your code into a proper Python module and then use it
from there. syslog-ng automatically adds `${sysconfdir}/python` to your
PYTHONPATH (normally `/etc/syslog-ng/python`), with that in mind
add the following code to `/etc/syslog-ng/python/mytemplate.py`:


```python
def template_function(msg):
    return b"Hello World from Python! Original message: " + msg['MSGHDR'] + msg['MESSAGE']
```

The Python glue in syslog-ng will automatically import modules when it
encounters an identifier in dotted notation, so if you use this syslog-ng
config:

```
@version: 4.0

log {
    source { tcp(port(2000)); };
    destination { file("logfile" template("$ISODATE $(python mytemplate.template_function)")); };
};
```

syslog-ng would recognize that `mytemplate.template_function` is a qualified
name and would attempt to import `mytemplate` as a module and then look up
`template_function` within that module.

Please note that modules are only imported once, e.g.  you will need to
restart syslog-ng for a change to take effect.

### syslog-ng reload and Python

When you reload syslog-ng (with syslog-ng-ctl reload or systemctl reload
syslog-ng) then the `python` block in your configuration is reloaded along
with the rest of the configuration file.

So any changes you make in Python code directly embedded in your
configuration will be considered as the reload completes.  This also means
that any global variables will be reset, you cannot store state accross
reloads in your `python {}` block.

The same behaviour is however not the case with code you import from modules.
Modules are only imported once and kept across reloads, even if the
syslog-ng configuration is reloaded in the meanwhile.

This means that global state can be stored in modules and they will be kept,
even as syslog-ng reinitializes the configuration.

In case you want to reload a module every time syslog-ng config is
reinitialized, you need to do this explicitly with a code similar to this:

```python
python {

import mymodule
import importlib

# reload mymodule every time syslog-ng reloads
importlib.reload(mymodule)

};
```


### Destination driver

A destination driver in Python needs to be derived from the `LogDestination`
class, as defined by the syslogng module, like the example below:

`mydestination.py`:

```python
from syslogng import LogDestination

class MyDestination(LogDestination):
    def send(self, msg):
	return True

```

The interface of the `LogDestination` class is documented in the
`syslogng.dest` module, which is stored in the file
`modules/python/pylib/syslogng/dest.py` of the source tree.

Once all required methods are implemented, you can use this using the
"python" destination in the syslog-ng configuration language.

```
destination whatever {
    python(class(mydestination.MyDestination));
};
```

There's a more complete example destination in the `python_example()`
destination plugin, that is located in the directory
`modules/python-modules/example/` within the source tree or the same files
installed under `${exec_prefix}/syslog-ng/python/syslogng/modules` in a
production deployment.

### Template Function plugin

Template functions extend the syslog-ng template language. They get a
`LogMessage` object and return a string which gets embedded into the
output of the template.

You can have syslog-ng call a Python function from the template language
using the `$(python)` template function, as you have seen in the previous
chapter.

```
@version: 4.0

python {

def template_function(msg):
    return b"Hello World from Python! Original message: " + msg['MSGHDR'] + msg['MESSAGE']

};

...

destination d_file {
    file("/var/log/whatever" template("$(python template_function)"));
};
```

The Python function needs to be any kind of callable, it receives a
`LogMessage` instance and returns a string (`str` or `bytes`).

The message passed to a template function is read-only, if you are trying to
change a name-value pair, you will receive an exception.

### Parser plugin

A parser plugin in Python needs to be derived from the `LogParser` class as
this example shows:


```python

from syslogng import LogParser

class MyParser(LogParser):

    def parse(self, msg):
        msg['name'] = 'value'
        return True
```

In contrast to template functions, parsers receive a read-writable
`LogMessage` object, so you are free to modify its contents.

### Source driver based on LogFetcher

There are two kinds of source plugins that can be implemented in Python: (a)
`LogFetcher` and (b) `LogSource`.  Both of which are defined by the
`syslogng.source` module.

The main entry point that you will need to implement for a `LogFetcher`
class is the `fetch()` method, which is automatically invoked by syslog-ng,
whenever it is able to consume incoming messages.

```
@version: 4.0

python {
from syslogng import LogFetcher
from syslogng import LogMessage
import time

class MyFetcher(LogFetcher):
    def fetch(self):
        time.sleep(1)
        msg = LogMessage.parse("<5>2022-02-02T10:23:45+02:00 HOST program[pid]: foobar", self.parse_options)
        return self.SUCCESS, msg

};


log {
    source { python-fetcher(class(MyFetcher)); };
    destination { file("messages"); };
};
```

This example would generate one message every second, based on a literal
string that is parsed as a syslog message.

Our source in this case is running in a dedicated thread, so it is free to
block.

Please note the `time.sleep(1)` call as the very first line in our `fetch()`
method.  As you can see we were sleeping 1 second between invocations of our
method, thereby limiting the rate the source is producing messages.  If that
sleep wasn't there, we would be producing somewhere between 100-110k
messages per second, depending on the speed of your CPU, the performance of
the Python interpreter and the syslog-ng core.

Usually, when our fetcher would be connecting to an external API, the sleep
would not be needed and the response times of the API would be the limiting
factor.

#### Adding persistent state

Usually, if we are fetching messages from an API, we will need to keep track
where we are in fetching messages. If we were to store the position in a
variable, that value would be lost when syslog-ng is reloaded or restarted
(depending on where we store that variable, in our `python {}` block or in a
module).

A better option is to use the `Persist()` class that ties into syslog-ng's
own persistent state handling functionality.  This allows you to store
variables persisted in a file that gets stored in
`${localstatedir}/syslog-ng.persist` along with the rest of syslog-ng's
states.

```python
class MyFetcher(LogFetcher):

    def init(self, options):
        self.persist = Persist("MyFetcher_persistent_data", defaults={"counter": 1})
        return True

    def fetch(self):
        time.sleep(1)
        counter = self.persist['counter']
        self.persist['counter'] += 1
        msg = LogMessage.parse("<5>2022-02-02T10:23:45+02:00 HOST program[pid]: foobar %d" % counter, self.parse_options)
        return self.SUCCESS, msg

```

Once initialized, a `Persist()` instance behaves as a dict where you can
store Python values (currently only `str`, `bytes` and `int` are supported).

Anything you store in a persist instance will be remembered even across
restarts.

The entries themselves are backed up to disk immediately after changing
them (using an `mmap()`-ped file), so no need to care about committing them to
disk explicitly.

Albeit you can store position information in a `Persist()` entry, it's not
always the best choice.  In syslog-ng, producing of messages and their
delivery is decoupled: sometimes a message is still in-flight for a while
before being delivered.  This time can be significant, if a destination
consumes messages at a slow rate.  In this case, if you store the position
once fetched, the message would still be sitting in a queue waiting to be
delivered. If the queue is not backed by disk (e.g. disk-buffer), then these
messages would be lost, once syslog-ng is restarted.

To anticipate this case, you will need to use bookmarks, as described in the
next section.

#### Using bookmarks in a source

The bookmarking mechanism allows messages to carry individual markers that
uniquely identify a message and its position in a source stream.

In a source file for example, the bookmark would contain the position of the
message within that file. An API may have a similar mechanism in place in
which the source API associates an opaque to each message, which signifies
its position in the repository.

A specific example for bookmarks is systemd-journald, which has a "cursor"
indicating the position of each journal record. The cursor can then be used
to restart the reading of the log stream.

Once you identified what mechanism the source offers that would map to the
bookmark concept, you can decide how you want syslog-ng to track these
bookmarks.

Which one you will need from syslog-ng's selection of bookmark
tracking strategies is again up to the API specifics.

Sometimes an API is sequential in nature, thus you can only acknowledge the
"last fetch position" in that sequence.  In other cases the API allows you
to acknowledge messages individually. These are both supported by syslog-ng.

But first, here's a Python example that only updates the current position in
a source stream when the messages in sequence were acknowledged by syslog-ng
destinations (e.g.  they were properly sent).

```python

class MyFetcher(LogFetcher):
    counter = 0

    def init(self, options):
        self.persist = Persist("MyFetcher_persistent_data", defaults={"position": 0})
        self.counter = self.persist['position']

	# pass self.message_acked method as ACK callback
        self.ack_tracker = ConsecutiveAckTracker(ack_callback=self.message_acked)
        return True

    def message_acked(self, acked_message_bookmark):
	# update current persisted position when syslog-ng delivered the
	# message, but only then.
        self.persist['position'] = acked_message_bookmark

    def fetch(self):
        time.sleep(1)
        self.counter += 1

	# depending on the speed of our consumer and the setting of
	# flags(flow-control), the current counter and the acked value may
	# differ in the messages generated.
        msg = LogMessage.parse("<5>2022-02-02T10:23:45+02:00 HOST program[pid]: foobar %d (acked so far %d)" % (self.counter, self.persist['position']), self.parse_options)

	# this is where we set the bookmark for the message
        msg.set_bookmark(self.counter)
        return self.SUCCESS, msg

```

#### Acknowledgement tracking strategies

As mentioned some APIs will provide simple others somewhat more complex ways
to track messages that are processed. There are three strategies within
syslog-ng to cope with them.

  * instant tracking (`InstantAckTracker`): messages are considered delivered
    as soon as our destination driver (or the disk based queue) acknowledges
    them.  Out-of-order deliveries are reported as they happen, so an
    earlier message may be acknowledged later than a message originally
    encountered later in the source stream.

  * consecutive tracking (`ConsecutiveAckTracker`): messages are assumed to
    form a stream and the bookmark to be a position in that stream.
    Unordered deliveries are properly handled by only acknowledging messages
    that were delivered in order.  If an unordered delivery happens, the
    tracker waits for the sequence to fill up, e.g.  waits all preceeding
    messages to be delivered as well.

  * batched tracking (`BatchedAckTracker`): messages are assumed to be
    independent, not forming a sequence of events.  Each message is
    individually tracked, the source driver has the means to get delivery
    notifications of each and every message independently.  The
    acknowledgements are accumulated until a timeout happens, at which point
    they get reported as a single batch.

You can initialize your ack_tracker in the `init` method, like this:

```python

class MyFetcher(LogFetcher):

    ...

    def init(self, options):
        # pass self.message_acked method as ACK callback
        self.ack_tracker = ConsecutiveAckTracker(ack_callback=self.message_acked)
        return True

    def message_acked(self, acked_message_bookmark):
        pass

    def fetch(self):
        ...
        msg.set_bookmark("whatever-bookmark-value-that-denotes-position")
    ...

```

In the case above, we were using `ConsecutiveAckTracker`, e.g.  we would only
get acknowledgements in the order messages were generated.  The argument of
the `message_acked` callback would be the "bookmark" value that we set using
`set_bookmark()`.

Using `InstantAckTracker` is very similar, just replace
`ConsecutiveAckTracker` with `InstantAckTracker`. In this case you'd get a
callback as soon as a message is delivered without preserving the original
ordering.

```python

class MyFetcher(LogFetcher):

    ...

    def init(self, options):
        self.ack_tracker = InstantAckTracker(ack_callback=self.message_acked)
        return True

    def message_acked(self, acked_message_bookmark):
        pass
```

While `ConsecutiveAckTracker()` seems to provide a much more useful service,
but `InstantAckTracker()` performs better, as it does not have to track
acknowledgements of individual messages.

The most complex scenario is implemented by `BatchedAckTracker`, this allows
you to track the acks for individual messages, as they happen, not enforcing
any kind of ordering.

```python

class MyFetcher(LogFetcher):

    ...

    def init(self, options):
        self.ack_tracker = BatchedAckTracker(timeout=500, batch_size=100,
                                             batched_ack_callback=self.messages_acked)
        return True

    def messages_acked(self, acked_message_bookmarks):
        pass
```

`BatchedAckTracker` would call your callback every now and then, as specified
by the `timeout` argument in milliseconds. `batch_size` specifies the number of
outstanding messages at a time. With this interface it's quite easy to
perform acknowledgements back to the source interface where per-message
acknowledgements are needed (e.g. Google PubSub).

#### Accessing the `flags()` option

The state of the `flags()` option is mapped to the `self.flags` variable, which is
a `Dict[str, bool]`, for example:
```python
{
    'parse': True,
    'check-hostname': False,
    'syslog-protocol': True,
    'assume-utf8': False,
    'validate-utf8': False,
    'sanitize-utf8': False,
    'multi-line': True,
    'store-legacy-msghdr': True,
    'store-raw-message': False,
    'expect-hostname': True,
    'guess-timezone': False,
    'header': True,
    'rfc3164-fallback': True,
}
```

### Creating a syslog-ng LogSource based Source plugin

While `LogFetcher` gives us a convinient interface for fetching messages from
backend services via blocking APIs, it is limited to performing the fetching
operation in a sequential manner: you fetch a batch of messages, feed the
syslog-ng pipeline with those messages and then repeat.

`LogSource` is more low-level but allows the use of an asynchronous framework
(`asyncio`, etc) to perform message fetching along multiple threads of
execution.

This sample uses `asyncio` to generate two independent sequences of messages,
one is generated every second, the other is every 1.5 seconds, running
concurrently via an `asyncio` event loop.

It is also pretty easy to create a source that implements a HTTP server,
and which injects messages coming via HTTP to the syslog-ng pipeline.

```python

from syslogng import LogSource
from syslogng import LogMessage
import asyncio

class MySource(LogSource):
    cancelled = False

    def run(self):
        asyncio.run(self.main())

    async def main(self):
        await asyncio.gather(self.sequence1(), self.sequence2())

    async def sequence1(self):
        while not self.cancelled:
            await asyncio.sleep(1)
            self.post_message(LogMessage("message1"))

    async def sequence2(self):
        while not self.cancelled:
            await asyncio.sleep(1.5)
            self.post_message(LogMessage("message2"))

    def request_exit(self):
        self.cancelled = True

```

Acknowledgement mechanisms (`ConsecutiveAckTracker`, `BatchedAckTracker`) and
`flags()` mapping can be used similarly to how it was described at `LogFetcher`.

## Making it more native config-wise

All the examples above used some form of `python()` driver to be actually
used, for instance, this was our Python based destination driver:

```
destination whatever {
    python(class(mydestination.MyDestination)
           options(option1 => value,
                   option2 => value));
};
```

while this works, this syntax is a bit foreign from a syslog-ng perspective
and somewhat hard to read. The usual syntax for referencing regular drivers
is something like this:

```
destination whatever {
    my-destination(option1(value) option2(value));
};
```

To make the syntax more native, you can use syslog-ng's block functionality
to wrap your Python driver, hide that it's actually Python and provide a
syntax to users of your code that is more convinient to use.

```
block destination my-destination(option1(value)
                                 option2(value)) {
    python(class(mydestination.MyDestination)
                 options(option1 => `option1`,
                         option2 => `option2`));
};
```

This block allows the use of the more "native" syntax, completely hiding the
fact that the implementation is Python based, concentrating on
functionality.

You should add this wrapper to your Python module in an "scl" subdirectory
as a file with a .conf extension. syslog-ng will automatically include these
files along the rest of the SCL.

## Adding the code to syslog-ng

To add Python based modules to syslog-ng, create a Python package (e.g.  a
directory with `__init__.py` and anything that file references).  Add these
files to `modules/python-modules/<subdirectory>` and open a pull request.

With that a "make install" command should install your module along the
rest of the syslog-ng binaries.

You will also need to add your files to the source tarball by listing them
in the EXTRA_DIST variable of the `modules/python-modules/Makefile.am` file.

## External dependencies

If your Python code depends on third party libraries, those need to be
installed on the system where your code is deployed.

If your deployment mechanism is based on packages (deb or rpm), make sure
that you add these OS level dependencies to the packages generated.

In deb this usually means adding the right package to your Depends line.

In rpm this means adding the right package as a Requires line in the .spec
file.

If you would like to use pip/requirements.txt to deploy dependencies, you
can choose to: invoke pip at "make install" time so that syslog-ng's private
Python directory would contain all the dependencies that you require.

## Adding Python code to syslog-ng deb package

To add your module to the syslog-ng deb package, create a new file in
`packaging/debian/` with a name like `syslog-ng-mod-<yourmodule>.install`.
Populate this file with wildcard patterns that capture the files of your
package after installation.

For example:

```
usr/lib/syslog-ng/python/syslogng/modules/<yourmodule>/*
```

You will also need to add an entry to `packaging/debian/control`:

```
Package: syslog-ng-mod-<yourmodule>
Architecture: any
Multi-Arch: foreign
Depends: ${shlibs:Depends}, ${misc:Depends}, syslog-ng-core (>= ${source:Version}), syslog-ng-core (<< ${source:Version}.1~), syslog-ng-mod-python
Description: The short description of the package
 This is a longer description with dots separating paragraphs.
 .
 This package provides a collection of example plugins.
```

Make sure that your .install file is included in the tarball by adding it to
EXTRA_DIST in the Makefile.am

## Adding Python code to syslog-ng RPM package

The RPM package is less modular than the Debian one and it automatically
captures all Python modules in the `syslog-ng-python` package without having
to list them explicitly.

If you need to customize installation, you can find our spec file in
`packaging/rhel/syslog-ng.spec` which is populated and copied to the root at
tarball creation.
