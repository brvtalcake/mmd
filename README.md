mmd - Miniature Markdown Library
================================

![Version](https://img.shields.io/github/v/release/michaelrsweet/mmd?include_prereleases)
![Apache 2.0](https://img.shields.io/github/license/michaelrsweet/mmd)
[![Build Status](https://img.shields.io/github/workflow/status/michaelrsweet/mmd/Build)](https://github.com/michaelrsweet/mmd/actions/workflows/build.yml)
[![Coverity Scan Status](https://img.shields.io/coverity/scan/22387.svg)](https://scan.coverity.com/projects/michaelrsweet-mmd)
[![LGTM Grade](https://img.shields.io/lgtm/grade/cpp/github/michaelrsweet/mmd)](https://lgtm.com/projects/g/michaelrsweet/mmd/context:cpp)
[![LGTM Alerts](https://img.shields.io/lgtm/alerts/github/michaelrsweet/mmd)](https://lgtm.com/projects/g/michaelrsweet/mmd/)

`mmd` is a miniature markdown parsing "library" consisting of a single C source
file and accompanying header file.  `mmd` mostly conforms to the [CommonMark][]
version of markdown syntax with the following exceptions:

- Embedded HTML markup and entities are explicitly not supported or allowed;
  the reason for this is to better support different kinds of output from the
  markdown "source", including XHTML, man, and `xml2rfc`.

- Tabs are silently expanded to the markdown standard of four spaces since HTML
  uses eight spaces per tab.

- Some pathological nested link and inline style features supported by
  CommonMark (`******Really Strong Text******`) are not supported by `mmd`.

In addition, `mmd` supports a couple (otherwise undocumented) markdown
extensions:

- Metadata as used by Jekyll and other web markdown solutions.

- "@" links which resolve to headings within the file.

- Tables and task lists as used by the [Github Flavored Markdown Spec][GFM].

`mmd` also includes a standalone utility called `mmdutil` that can be used to
generate HTML and man page source from markdown.

I'm providing `mmd` as open source under the Apache License Version 2.0 with
exceptions for use with GPL2/LGPL2 applications which allows you do pretty much
do whatever you like with it.  Please do provide feedback and report bugs to the
Github project page at <https://www.msweet.org/mmd> so that everyone can
benefit.

[CommonMark]: https://spec.commonmark.org
[GFM]: https://github.github.com/gfm


Requirements
------------

You'll need a C compiler.


How to Incorporate in Your Project
----------------------------------

Add the `mmd.c` and `mmd.h` files to your project.  Include the `mmd.h`
header in any file that needs to read/convert markdown files.


"Kicking the Tires"
-------------------

The supplied makefile allows you to build the unit tests on Linux and macOS (at
least), which verify that all of the functions work as expected to produce a
HTML file called `testmmd.html`:

    make test

The makefile also builds the `mmdutil` program.


Installing `mmdutil`
--------------------

You can install the `mmdutil` program by copying it to somewhere appropriate or
run:

    make install

to install it in `/usr/local` along with a man page.


Changes in v2.0
---------------

- Added `mxmlLoadString` API and added a document pointer to the other load
  functions to allow concatenation of markdown files.


Changes in v1.9
---------------

- Added support for the Github-flavored markdown task list extension (check
  boxes in lists)
- Addressed some issues found by the Clang static analyzer.


Changes in v1.8
---------------

- Markdown of the form `([title](link))` did not parse correctly.
- Addressed an issue identified by the LGTM code scanner.
- Addressed some issues identified by the Cppcheck code scanner.
- Addressed some issues identified by the Coverity code scanner.
- Changed the makefile to only run the unit test program when using the "test"
  target.
- Added a Cppcheck target ("cppcheck") to use this code scanning program against
  the `mmd` sources.


Changes in v1.7
---------------

The following changes were made for v1.7:

- Fixed table parsing (Issue #11)
- Fixed block-quoted Setext heading parsing.


Changes in v1.6
---------------

The following changes were made for v1.6:

- Fixed some parsing bugs (Issue #7)
- Fixed a crash bug in mmdutil (Issue #8)
- Code fences using "~~~" are now supported.
- Auto-links now properly handle preceding text (Issue #8)
- Inline styles can now span multiple lines (Issue #8)
- Links can now span multiple lines (Issue #8)
- Shortcut links (`[reference]`) didn't work (Issue #8)
- Fixed some issues with inline styles being incorrectly applied for things
  like "* *".
- The `testmmd` program now supports running tests from the CommonMark
  specification and/or from the CommonMark test suite (Issue #9)
- More CommonMark features (code languages, link titles, space-filled thematic
  breaks) and edge cases are now supported (Issue #10)
- Added new `mmdGetOptions` and `mmdSetOptions` functions to control which
  extensions are supported.
- Added new `mmdGetExtra` function to get the link title or code language
  string associated with certain nodes.


Changes in v1.5
---------------

The following changes were made for v1.5:

- Added support for referenced links (Issue #1)
- Added support for `__bold__`, `_italic_`, `~~strikethrough~~`, and hard
  line breaks (Issue #4)


Changes in v1.4
---------------

The following changes were made for v1.4:

- Fixed a table parsing bug where trailing pipes would add empty cells on the
  right side.
- Tweaked the `mmdutil` program's default HTML stylesheet.
- Fixed `mmdutil` error messages that incorrectly called the program `mmdbook`.
- Fixed some Clang static analyzer warnings in `mmd.c`.
- Fixed a build issue with Visual Studio.


Changes in v1.3
---------------

The following changes were made for v1.3:

- Added `mmdCopyAllText` function that returns all of the text under the given
  node.
- Added `mmdutil` program for converting markdown to HTML and man files.


Changes in v1.2
---------------

The following changes were made for v1.2:

- Changed license to Apache License Version 2.0
- Added support for markdown tables (Issue #3)


Changes in v1.1
---------------

The following changes were made for v1.1:

- The `mmd.h` header now includes the C++ `extern "C"` wrapper around the C
  function prototypes.
- Added a `mmdLoadFile` function that loads a markdown document from a `FILE`
  pointer.
- Fixed a parsing bug for emphasized, bold, and code text containing whitespace.
- Fixed a parsing bug for escaped characters followed by unescaped formatting
  sequences.
- Fixed a parsing bug for headings that follow a list.


Legal Stuff
-----------

Copyright © 2017-2022 by Michael R Sweet.

mmd is licensed under the Apache License Version 2.0 with an (optional)
exception to allow linking against GPL2/LGPL2-only software.  See the files
"LICENSE" and "NOTICE" for more information.
