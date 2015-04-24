# tcpkatest: test TCP keep-alive configuration

This is a small tool for testing TCP keep-alive configuration parameters.  There
are four relevant socket options:

* `SO_KEEPALIVE`: enables TCP keep-alive (see
  [tcp(7p)](http://illumos.org/man/7p/tcp) and
  [RFC 1122](http://www.ietf.org/rfc/rfc1122.txt)).
* `TCP_KEEPIDLE`: sets the number of seconds that must elapse with no data
  transmitted before the operating system starts sending TCP keep-alive probes.
* `TCP_KEEPCNT`: sets the number of keepalive probes that will be sent before
  giving up on the connection.
* `TCP_KEEPINTVL`: sets the number of seconds to wait before sending consecutive
  keepalive probes when no response has been received, and the number of seconds
  to wait after the last probe before giving up on the connection.

The way to think about this is that if you set `SO_KEEPALIVE` to `1` on a
TCP socket, then when the socket has received no data for `TCP_KEEPIDLE`
seconds, the system sends a keepalive probe.  If `TCP_KEEPINTVL` seconds elapse,
the system sends another probe.  If `TCP_KEEPCNT` probes are sent and
`TCP_KEEPINTVL` seconds elapses after the last one, the connection is considered
broken.  poll(2) asserts POLLIN and read(2) returns -1 with errno = ETIMEDOUT.

These parameters appear to be supported on illumos and GNU/Linux systems.  It's
not clear that `TCP_KEEPCNT` and `TCP_KEEPINTVL` are supported on BSD or OS X.
I'm not sure at all about Windows.


## Trying it out

You run this program as:

    ./tcpkatest [-c CNT] [-d IDLE] [-i INTVL] HOST:PORT

The program creates a TCP socket and sets the `SO_KEEPALIVE` socket option to
`1`.  If any of the "-c", "-d", and "-i" options are specified, then the
`TCP_KEEPCNT`, `TCP_KEEPIDLE`, and `TCP_KEEPINTVL` socket options are set on the
socket to the respective option arguments.  Then the program connects to
HOST:PORT and enters a loop between calling poll(2) for the POLLIN event and
reading data.

You can use this with a program like [snoop(1M)](http://illumos.org/man/snoop)
to see exactly what's going on.  First, I'll start a server socket on a remote
system:

    # nc -k -l -p 12345

Back on my test system, I'll start up snoop:

    # snoop -r -t a -d net1 port 12345

(The "-r" means to avoid resolving DNS names, and "-t a" tells snoop to print
absolute timestamps.)

Now, I'll kick off "tcpkatest" with a 10-second initial delay, followed by up to
three keep-alive probes two seconds apart:

    ./tcpkatest -d 10 -i 2 -c 3 172.25.10.4:12345
    will connect to: 172.25.10.4 port 12345
    SO_KEEPALIVE  = 1
    TCP_KEEPIDLE  = 10
    TCP_KEEPCNT   = 3
    TCP_KEEPINTVL = 2
    2015-04-23T23:09:05Z: connected

Now I'll kick off snoop and watch a few 10-second intervals fly by:

    16:09:35.62205 172.25.10.105 -> 172.25.10.4  TCP ...
    16:09:35.62210  172.25.10.4 -> 172.25.10.105 TCP ...
    16:09:45.63004 172.25.10.105 -> 172.25.10.4  TCP ...
    16:09:45.63008  172.25.10.4 -> 172.25.10.105 TCP ...

At 16:09:54Z, I use a firewall to block inbound TCP packets on port 12345.
(I'm using [ipf(1M)](http://illumos.org/man/ipf) to do this, using the rule
"block in from any port = 12345 to any".)  Now, snoop shows:

    16:09:55.64026 172.25.10.105 -> 172.25.10.4  TCP
    16:09:55.64032  172.25.10.4 -> 172.25.10.105 TCP
    16:09:57.65051 172.25.10.105 -> 172.25.10.4  TCP
    16:09:57.65055  172.25.10.4 -> 172.25.10.105 TCP
    16:09:59.66043 172.25.10.105 -> 172.25.10.4  TCP
    16:09:59.66051  172.25.10.4 -> 172.25.10.105 TCP

(Note that we see the keep-alive probes and responses because the responses are
dropped by the firewall *after* snoop sees them.)  Importantly, we see the :55
keepalive that we'd expect, followed by one two seconds later at :57, then one
two seconds after that at :59.  Finally, "tcpkatest" prints:

    2015-04-23T23:10:01Z: poll events: 0x1
    2015-04-23T23:10:01Z: tcpkatest: read: Connection timed out

which is exactly two seconds after the last probe was sent.


# Node.js

There's a similar Node.js program in this repo that you can use to test in a
similar way.

There are a few issues that affect Node programs trying to use TCP KeepAlive:

* It's not possible to configure `TCP_KEEPCNT` and `TCP_KEEPINTVL`. (Node issue
  [#4109](https://github.com/joyent/node/issues/4109).)  As a result, you're
  left with the system defaults for these.
* Because of issue [#21079](https://github.com/joyent/node/issues/21079), you
  should only call setKeepAlive after the socket is connected.
* Because of issue [#21080](https://github.com/joyent/node/issues/21080), you
  want to be sure your value for the timeout is supported on your system.
