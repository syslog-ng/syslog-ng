# Syslog-ng packaging files

Our primary goal with these packaging files is to be able to provide binary
packages for a set of distributions, compiled from

1) the latest version of syslog-ng (for official releases)
2) any git commit in between releases (for a snapshot with a potential bugfix)

# Synchronization with distro official packages

syslog-ng is part of a number of distributions, either in the main archive
or supplemental/semi-official archives.

We find it beneficial that people may find syslog-ng in those repositories,
however in a number of cases those syslog-ng versions lag behind our master
with several releases.

In order to have best of both worlds, we are attempting to

  1) provide a packaging that is compatible with those in distros
  2) sync them with those distros from time to time.

Our basic aim is to make it possible to upgrade/downgrade between
official/distro packages.

In order to maintain this synchronization a couple of conventions/processes
needs to be maintained that are documented in this file.

# Definition of downstream/upstream packaging

We call the syslog-ng source tree the "upstream" source tree, to indicate
that syslog-ng is produced in this repository and distributions pick it up
further down the line. With the same logic, this set of packaging files are
referred to as "upstream" packaging.

With this said, distributions produce a "downstream" packaging, probably
maintained in their own repository and release cadence.

The difference between "upstream" and "downstream" packaging files is what
we want to keep to a minimum.

# Sync pull requests

Both "upstream" and "downstream" repositories accumulate changes that are
worthy of inclusion in the other.  For instance, "downstream" repository
keeps up with larger changes in the distribution (e.g.  policy updates)
whereas "upstream" changes follow up changes in syslog-ng itself (e.g.  new
module)

Since these are basically forks of each other some kind of activity is
required to apply the same set of changes to both repositories.

This is basically carried out by the following mechanism:
  - we produce a PR in the upstream repository which
    1) copies the entire downstream packaging to upstream AND then
    2) produces a series of patches on-top so we retain "upstream" specific functionality
  - the patches we apply on top of the "downstream packaging" can be categorized into:
    1) "upstreamonly" patches are never expected to be picked up by downstream
    2) "downstreamonly" patches are expected to be integrated into official distribution packages
  - we merge the PR into upstream, essentially syncing it with downstream + minimal changes
  - we also open a PR for downstream with "downstreamonly" patches, so when they merge that, our next sync round would become smaller.

If we do the above regularly enough, our packaging files would only be
mildly different from official packages, achieving our goal.

# Practical examples

Sometimes we need to do a minor maintenance in our "upstream" packages.
Since syslog-ng contributors only have a limited expertise with deb and
RPM packaging tools, this section helps them to achieve a couple of common
tasks, that are regularly needed as syslog-ng evolves.

## Adding a new build Dependency

Sometimes we need to add a new build dependency. Previously in these cases,
we needed to add it to our `packages.manifest` file, but nowadays we are
using the native tools build dependency resolution mechanisms, so we
would need to change the packaging files.

### Debian

To add a new build dependency, just open the file named `packaging/debian/control`
and locate the Build-Depends line in the first stanza.

Add the package you need to compile a specific function (usually some kind of -dev package).
If you need a specific version, make sure you also add the minimum version
number:

```
Build-Depends: ...
               libriemann-client-dev (>= 1.6.0~)
```

If you only want to enable the compilation conditionally, you will need to
add a build profile clause enclosed in brackes, like this:

```
Build-Depends: ...
               librdkafka-dev (>= 1.0.0) <!sng-nokafka>

```

The term "nokafka" in `build.manifest` would then be used to control this
build dependency.  This term would be mapped into a Debian profile named
`sng-nokafka`.


### RHEL

To add a new build dependency in case of RPM based distros (centos, fedora),
you need to change the file `packaging/rhel/syslog-ng.spec`. You need to add
a BuildRequires line like this:

```
BuildRequires: librdkafka-devel
```

If you need conditional compilation that can be achieved using conditionals,
like this:

```
%if %{with kafka}
BuildRequires: librdkafka-devel
BuildRequires: zlib-devel
%endif
```

The term "nokafka" in `build.manifest` would be translated into a `--without
kafka` command line option when building an rpm thereby omitting kafka
related lines in the spec file.

## Adding a new subpackage

Sometimes you want to create a new subpackage that is generated when
compiling syslog-ng, typically this would be a new syslog-ng module.

Albeit it might seem like it, but we don't put all modules in new packages,
this would roughly depend on the following criteria:

  * does it use extra dependencies at runtime? (if it does, we put it in a
    separate package)
  * does it constitute "core" functionality of syslog-ng? (if it doesn't, we
    put it in a separate package)

Answering the questions above can be tricky and unfortunately we haven't
been consistent with these in the past either. Debian is "more" modular
whereas RedHat packages are more monolithic.

### Debian

In Debian, all you need to do is to add a new Package stanza to the
`packaging/debian/control` file.

```
Package: syslog-ng-mod-slog
Architecture: any
Multi-Arch: foreign
Depends: ${shlibs:Depends}, ${misc:Depends}
Description: Enhanced system logging daemon (slog)
 syslog-ng is an enhanced log daemon, supporting a wide range of input
 and output methods: syslog, unstructured text, message queues,
 databases (SQL and NoSQL alike) and more.
 .
 Key features:
 .
  * receive and send RFC3164 and RFC5424 style syslog messages
  * work with any kind of unstructured data
  * receive and send JSON formatted messages
  * classify and structure logs with builtin parsers (csv-parser(),
    db-parser(), etc.)
  * normalize, crunch and process logs as they flow through the system
  * hand on messages for further processing using message queues (like
    AMQP), files or databases (like PostgreSQL or MongoDB).
 .
 This package provides the $(slog) template functions and command
 line utilities (slogencrypt, slogverify, slogkey).

```

Just copy-paste the description and adapt it to your module. Couple of
notes:

  * each line in debian/control is an RFC822 like header (e.g. like an email
    header)
  * you can use multiple lines by starting the next line with a whitespace
  * an empty line can be created with a continuation line (e.g. starting
    with space) and using a dot as a single character,

Once you have the entry in `debian/control`, create a file that lists the
files debhelper needs to install in the package. This should be named
`<package-name>.install`

Each line in this file is a shell-like wildcard pattern, like this:

```
usr/lib/syslog-ng/*/libsecure-logging.so
usr/bin/slog*
```

That's roughly it. Once these are in place, the extra deb package would be
created at build time.

### RHEL

With rpm packaging you will need to change `packaging/rhel/syslog-ng.spec`
file and add a `%package` stanza somewhere around the other similar packages:


```
%if %{with kafka}
%package kafka
Summary: kafka support for %{name}
Group: Development/Libraries
Requires: %{name}%{?_isa} = %{version}-%{release}

%description kafka
This module supports sending logs to kafka through librdkafka.
%endif

```

Then you will need to tell where the associated files are to be found:

```
%if %{with kafka}
%files kafka
%{_libdir}/%{name}/libkafka.so
%endif

```

You can see that building the kafka subpackage is conditional, which you can
control using the `--with kafka` or `--without kafka` command line arguments
to rpmbuild.
