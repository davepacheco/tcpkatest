#
# Makefile for tcpkatest
#

CLEANFILES	 = tcpkatest
CFLAGS		+= -O2 -Werror -Wall -Wextra -m64 -fno-omit-frame-pointer
LDFLAGS		+= -lgen -lsocket -lnsl

tcpkatest: tcpkatest.c
	$(CC) -o $@ $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $^

clean:
	rm -f $(CLEANFILES)
