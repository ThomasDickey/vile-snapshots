CC = gcc
CFLAGS = -O2 -pipe -Wall -DHAVE_CONFIG_H -I.. $(SH_CFLAGS)
LDFLAGS = -s $(SH_LDFLAGS)

SH_CFLAGS = -DPIC -fPIC
SH_LDFLAGS = -shared

PLUGINS = test
SHLIBS = $(PLUGINS:%=vile-%-plugin.so)

all : $(SHLIBS)

vile-%-plugin.so : %.o
	$(CC) $(LDFLAGS) -o $@ $<

