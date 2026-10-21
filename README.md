[![ci](https://github.com/mobile-shell/mosh/actions/workflows/ci.yml/badge.svg)](https://github.com/mobile-shell/mosh/actions/workflows/ci.yml)

Mosh: the mobile shell
======================

Mosh is a remote terminal application that supports intermittent
connectivity, allows roaming, and provides speculative local echo
and line editing of user keystrokes.

It aims to support the typical interactive uses of SSH, plus:

   * Mosh keeps the session alive if the client goes to sleep and
     wakes up later, or temporarily loses its Internet connection.

   * Mosh allows the client and server to "roam" and change IP
     addresses, while keeping the connection alive. Unlike SSH, Mosh
     can be used while switching between Wi-Fi networks or from Wi-Fi
     to cellular data to wired Ethernet.

   * The Mosh client runs a predictive model of the server's behavior
     in the background and tries to guess intelligently how each
     keystroke will affect the screen state. When it is confident in
     its predictions, it will show them to the user while waiting for
     confirmation from the server. Most typing and uses of the left-
     and right-arrow keys can be echoed immediately.

     As a result, Mosh is usable on high-latency links, e.g. on a
     cellular data connection or spotty Wi-Fi. In distinction from
     previous attempts at local echo modes in other protocols, Mosh
     works properly with full-screen applications such as emacs, vi,
     alpine, and irssi, and automatically recovers from occasional
     prediction errors within an RTT. On high-latency links, Mosh
     underlines its predictions while they are outstanding and removes
     the underline when they are confirmed by the server.

Mosh does not support X forwarding or the non-interactive uses of SSH,
including port forwarding.

Other features
--------------

   * Mosh adjusts its frame rate so as not to fill up network queues
     on slow links, so "Control-C" always works within an RTT to halt
     a runaway process.

   * Mosh warns the user when it has not heard from the server
     in a while.

   * Mosh supports lossy links that lose a significant fraction
     of their packets.

   * Mosh handles some Unicode edge cases better than SSH and existing
     terminal emulators by themselves, but requires a UTF-8
     environment to run.

   * Mosh leverages SSH to set up the connection and authenticate
     users. Mosh does not contain any privileged (root) code.

Getting Mosh
------------

  [The Mosh web site](https://mosh.org/#getting) has information about
  packages for many operating systems, as well as instructions for building
  from source.

  Note that `mosh-client` receives an AES session key as an environment
  variable.  If you are porting Mosh to a new operating system, please make
  sure that a running process's environment variables are not readable by other
  users.  We have confirmed that this is the case on GNU/Linux, OS X, and
  FreeBSD.

Usage
-----

  The `mosh-client` binary must exist on the user's machine, and the
  `mosh-server` binary on the remote host.

  The user runs:

    $ mosh [user@]host

  If the `mosh-client` or `mosh-server` binaries live outside the user's
  `$PATH`, `mosh` accepts the arguments `--client=PATH` and `--server=PATH` to
  select alternate locations. More options are documented in the mosh(1) manual
  page.

  There are [more examples](https://mosh.org/#usage) and a
  [FAQ](https://mosh.org/#faq) on the Mosh web site.

Default foreground/background queries in this fork
---------------------------------------------------

This fork supports OSC 10/11 queries, including BEL and ST terminators.
`mosh-client` queries the local terminal asynchronously at connection startup,
after resume, and every five seconds. Valid RGB replies (one to four hex digits
per component) travel as an optional user-input protocol extension. The server
caches those values and answers application queries locally, without an extra
network round trip. This follows the cached-response approach proposed in
[Mosh PR #1394](https://github.com/mobile-shell/mosh/pull/1394), with asynchronous
client probing in place of the launcher's synchronous startup probe.

Both binaries must be rebuilt: the client on macOS and the server on Linux.
The Perl launcher is unchanged. Old clients and servers still interoperate;
they provide no default-color replies. No transport version bump is needed.
The existing Unicode, clipboard, terminal rendering, and agent-forwarding
patches remain in place.

Unsupported terminals produce no color response. Missing replies never stop
typing or rendering; incomplete input escape prefixes expire after 50 ms.
Ordinary keys and complete non-color escape sequences pass through immediately,
and bracketed paste contents are preserved. At most 16 early queries per color
are held until the first valid color report. Theme changes are detected on the
next five-second probe; an application that caches its startup colors, such as
Codex, may need restarting after a theme change. OSC color-setting commands and
indexed palette queries (OSC 4) are outside this implementation.

Build on Linux without replacing the installed binaries:

```sh
cd ~/Development/mosh
make -j4
make -j4 check
```

For a separate macOS checkout, run these commands in Ghostty. This clones the
committed Linux fork, including its previous patches, without publishing it or
replacing another checkout. Homebrew dependencies are installed only if needed.
Use the compiler and libraries from the same Homebrew Protobuf installation;
configure checks that their versions match.

```sh
brew install autoconf automake pkgconf protobuf openssl@3 ncurses tmux
export PATH="$(brew --prefix protobuf)/bin:$PATH"
export PKG_CONFIG_PATH="$(brew --prefix protobuf)/lib/pkgconfig:$(brew --prefix openssl@3)/lib/pkgconfig:$(brew --prefix ncurses)/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
git clone --branch patched joe-dev:Development/mosh ~/Development/mosh-osc-colors
cd ~/Development/mosh-osc-colors
./autogen.sh
./configure --prefix="$HOME/.local/mosh-osc-colors"
make -j"$(sysctl -n hw.ncpu)"
make -C src/tests check TESTS='terminal-osc-default-colors osc-colors.test'
./scripts/mosh --client="$PWD/src/frontend/mosh-client" --server=/home/jtoscano42/Development/mosh/src/frontend/mosh-server joe-dev
```

The last command explicitly selects both new binaries. It starts a new session;
existing sessions continue using their original binaries. No `make install` is
required. To stop using the patch, exit that session and reconnect with the
usual Mosh command. macOS compilation and Ghostty's visual result must be
validated on the Mac; the Linux test uses a simulated local terminal.

Inside the new Mosh session, run:

```sh
python3 ~/Development/mosh/scripts/osc-color-probe.py
codex
```

The probe should report both real terminal colors (the original Ghostty sample
was foreground `rgb:ebeb/dbdb/b2b2`, background `rgb:2828/2828/2828`). Compare the
same probe and a freshly launched Codex through ordinary SSH. The final visual
check is that Codex's composer has the same shaded background in both sessions.
For a light/dark theme change, wait at least five seconds, rerun the probe,
and restart Codex if needed. Colors are discovered, never hardcoded.

The regression tests include byte-fragmented ST/BEL replies, malformed and
missing responses, Unicode typing, bracketed paste, bounded early queries,
protocol serialization, and a real client/server UDP session with delayed
replies and a theme change. The PTY test requires Python 3. Run it with:

```sh
make -C src/tests check TESTS='terminal-osc-default-colors osc-colors.test'
```

How it works
------------

  The `mosh` program will SSH to `user@host` to establish the connection.
  SSH may prompt the user for a password or use public-key
  authentication to log in.

  From this point, `mosh` runs the `mosh-server` process (as the user)
  on the server machine. The server process listens on a high UDP port
  and sends its port number and an AES-128 secret key back to the
  client over SSH. The SSH connection is then shut down and the
  terminal session begins over UDP.

  If the client changes IP addresses, the server will begin sending
  to the client on the new IP address within a few seconds.

  To function, Mosh requires UDP datagrams to be passed between client
  and server. By default, `mosh` uses a port number between 60000 and
  61000, but the user can select a particular port with the -p option.
  Please note that the -p option has no effect on the port used by SSH.

Advice to distributors
----------------------

A note on compiler flags: Mosh is security-sensitive code. When making
automated builds for a binary package, we recommend passing the option
`--enable-compile-warnings=error` to `./configure`. On GNU/Linux with
`g++` or `clang++`, the package should compile cleanly with
`-Werror`. Please report a bug if it doesn't.

Where available, Mosh builds with a variety of binary hardening flags
such as `-fstack-protector-all`, `-D_FORTIFY_SOURCE=2`, etc.  These
provide proactive security against the possibility of a memory
corruption bug in Mosh or one of the libraries it uses.  For a full
list of flags, search for `HARDEN` in `configure.ac`.  The `configure`
script detects which flags are supported by your compiler, and enables
them automatically.  To disable this detection, pass
`--disable-hardening` to `./configure`.  Please report a bug if you
have trouble with the default settings; we would like as many users as
possible to be running a configuration as secure as possible.

Mosh ships with a default optimization setting of `-O2`. Some
distributors have asked about changing this to `-Os` (which causes a
compiler to prefer space optimizations to time optimizations). We have
benchmarked with the included `src/examples/benchmark` program to test
this. The results are that `-O2` is 40% faster than `-Os` with g++ 4.6
on GNU/Linux, and 16% faster than `-Os` with clang++ 3.1 on Mac OS
X. In both cases, `-Os` did produce a smaller binary (by up to 40%,
saving almost 200 kilobytes on disk). While Mosh is not especially CPU
intensive and mostly sits idle when the user is not typing, we think
the results suggest that `-O2` (the default) is preferable.

Our Debian and Fedora packaging presents Mosh as a single package.
Mosh has a Perl dependency that is only required for client use.  For
some platforms, it may make sense to have separate mosh-server and
mosh-client packages to allow mosh-server usage without Perl.

Notes for developers
--------------------

To start contributing to Mosh, install the following dependencies:

Debian, Windows Subsystem for Linux:

```
$ sudo apt install -y build-essential protobuf-compiler \
    libprotobuf-dev pkg-config libutempter-dev zlib1g-dev libncurses5-dev \
    libssl-dev bash-completion tmux less
```

MacOS:

```
$ brew install protobuf automake
```

Once you have forked the repository, run the following to build and test Mosh:

```
$ ./autogen.sh
$ ./configure
$ make
$ make check
```

Mosh supports producing code coverage reports by tests, but this feature is
disabled by default. To enable it, make sure `lcov` is installed on your
system. Then, configure and run tests:

```
$ ./configure --enable-code-coverage
$ make check-code-coverage
```

This will run all tests and produce a coverage report in HTML form that can be
opened with your favorite browser. Ideally, newly added code should strive for
90% (or better) incremental test coverage.

More info
---------

  * Mosh Web site:

    <https://mosh.org>

  * `mosh-devel@mit.edu` mailing list:

    <https://mailman.mit.edu/mailman/listinfo/mosh-devel>

  * `mosh-users@mit.edu` mailing list:

    <https://mailman.mit.edu/mailman/listinfo/mosh-users>

  * `#mosh` channel on [Libera Chat](https://libera.chat/)

    https://web.libera.chat/#mosh
