# Contributing to syslog-ng

syslog-ng is developed as a community project, and relies on
volunteers to produce syslog-ng. Reporting bugs, testing changes,
writing code, or simply providing feedback are all important
contributions. This guide provides guidelines to make contributing
easier.

## Table of contents

1. [Issues](#issues)
 1. [Reporting issues](#reporting-issues)
 2. [Feature requests](#feature-requests)
 3. [Testing](#testing)
2. [Patches](#patches)
3. [Additional resources](#additional-resources)

## Issues

One of the easiest ways to contribute to the development of syslog-ng
is to participate in the discussions about features, bugs and design.
Some of these discussions started on the
[mailing list][ar:mailing-list], some in the
[issue tracker][ar:issue-tracker]. Bugs tagged
[`help`][ar:issues:help] are generally good targets to contribute your
feedback - but pretty much any open issue can be a good start!

### Reporting bugs

When you report a bug, it is important to share as much relevant
information as you can, including:
 * version number of syslog-ng used;
 * the platform (operating system and its version, architecture, etc);
 * a backtrace from the core file if the issue is a crash (this can be
invaluable);
 * if possible, a configuration that triggers the problem;
 * a detailed description of the issue.

To make it easy to read reports, if you send a configuration snippet,
or a backtrace, use
[fenced code blocks](https://help.github.com/articles/github-flavored-markdown#fenced-code-blocks)
around them.

### Feature requests

We use the same [issue tracker][ar:issue-tracker] to handle features
requests (they're all tagged with the
[`enhancement`](https://github.com/syslog-ng/syslog-ng/labels/enhancement)
label. You are welcome to share your ideas on existing requests, or to
submit your own.

### Testing

An incredibly useful way to contribute is to test patches and pull
requests - there's only so much [automated testing][ar:travis] can do.
For example, you can help testing on platforms the developers do not
have access to, or try configurations not thought of before.

## Patches

Of course, we also accept patches. If you want to submit a patch, the
guidelines are very, very simple:

 1. Open an issue first, if there is none open for the particular
    issue for the topic already in the
    [issue tracker][ar:issue-tracker]. That way, other contributors
    and developers can comment the issue and the patch.
 2. If you submit a pull request that fixes an existing issue, mention
    the issue somewhere in the pull request, so we can close the
    original issue as well.
 3. We are using a coding style very similar to
    [GNU Coding Standards](https://www.gnu.org/prep/standards/standards.html#Writing-C)
    for syslog-ng. Please try to follow the existing conventions.
 4. Start your commit message with the following format:
    `block-name: short description of the applied changes`.

    Always add a `Signed-off-by` tag to the end of **every** commit
    message you submit.
 5. Always create a separate branch for the pull request, forked off
    from the appropriate syslog-ng branch.
 6. If your patch should be applied to multiple branches, submit
    against the latest one only, and mention which other branches are
    affected. There is no need to submit pull requests for each
    branch.
 7. If possible, write tests! We love tests.
 8. A well-documented pull request is much easier to review and merge.

## Licensing

Please ensure that your contribution is clean in respect to licensing and
copyright.

If your contribution is eligible for copyright, you should also extend the list
of copyright holders at the top of the relevant files which carry your
modifications.

The absolute minimum to specify is the identity of the author entity, which is
usually one or more of an e-mail address and your full name or the name of the
legal entity who holds the intellectual rights if it is not you.
Please make it clear which is the case, because this may depend on your
contract if you are employed or are a subcontractor.

Note that from time to time, we may rephrase the exact text surrounding
attributions, however the specified identities and the license binding a given
contribution will not be changed in a legally incompatibly manner.

Every new file must carry a standard copyright notice and be compatible with
our licensing scheme described in COPYING.
You should observe some of the existing files for reference.

## Additional resources

For additional information, have a look at the
[syslog-ng.org](http://www.syslog-ng.org/) website, which is a
recommended starting point for finding out more about syslog-ng.

To contact us, visit the [mailing list][ar:mailing-list] where you can
ask questions, and discuss your feature requests with a wider
audience.

We also have a [Gitter channel][ar:gitter], where developers hang out.

We use [GitHub issues][ar:issue-tracker] to track issues, feature requests
and patches. We are also using [Travis CI][ar:travis] for automatic
testing.

 [ar:gitter]: https://gitter.im/syslog-ng/syslog-ng
 [ar:mailing-list]: http://lists.balabit.com/mailman/listinfo/syslog-ng
 [ar:issue-tracker]: https://github.com/syslog-ng/syslog-ng/issues
 [ar:issues:help]: https://github.com/syslog-ng/syslog-ng/labels/help
 [ar:travis]: https://travis-ci.org/syslog-ng/syslog-ng/
