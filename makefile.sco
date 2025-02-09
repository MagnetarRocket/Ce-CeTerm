SCCSID      = "%Z% %M% %I% - %G% %U% AG CS Phoenix"
#   Makefile for Novel Unixware,  tested on montague 
#   UnixWare 2.1 (montague)
#   uname -a -> UNIX_SV montague 4.2MP 2.1 i386 x86at


CC           =  /usr/local/bin/gcc	

DFLAGS       =\
 -DDebuG\
 -DPAD\
 -DCE_LITTLE_ENDIAN\
 -DNeedFunctionPrototypes\
 -DARPUS_CE\
 -DNO_LICENSE\
 -Dsolaris\
 -DSCO\
 -DBZERO


#-Dlinux                KBP
#-DUSE_BSD_SIGNALS
#+Dsolaris


CFLAGS       = -g $(DFLAGS)
GLAGS       = 

# for UNIX_SV slider 4.2 1.1.3 i386 386/AT
* use:
#LIBS         =  /lib/libX11.so.5.0 -l gen -l nsl -l socket
# for UNIX_SV sniper 4.2MP 2.1.3 i386 x86at
# use:
LIBS         =  /usr/lib/libX11.so /usr/lib/libsocket.so /lib/libgen.a

# The following include sets variable CRPAD_OBS
include makefile.objs

EXTRA_OBJS    =  usleep.o


SRCMODS  = $(CRPAD_OBS:.o=.c) $(EXTRA_OBJS:.o=.c)  xdmc.c ceapi.c ce_isceterm.c

default: ce xdmc ce_isceterm

# The following include sets variable HFILES and sets up the .h file dependencies
include makefile.depends

# rule puts cflags at the end so we cans what is being compiled
.c.o:
	$(CC) -c $< $(CFLAGS)


expidate.o: expidate.h expidate.c expistamp
	$(CC) -c expidate.c $(CFLAGS)
	./expistamp -d 9999/12/31 1001 expidate.o
#	expistamp expidate.o 1994/12/31 1001

md: memdata_test.o memdata.o debug.o undo.o search.o re.o
	$(CC) $(CFLAGS) -o md memdata_test.o memdata.o debug.o undo.o search.o re.o

keytest: keytest.o keysym.o
	$(CC) $(CFLAGS) -o keytest keytest.o keysym.o $(LIBS)

delcc: delcc.o
	$(CC) $(CFLAGS) -o delcc delcc.o $(LIBS)

ccdump: ccdump.c debug.o cc.c hexdump.o
	$(CC) $(CFLAGS) -o ccdump ccdump.c  debug.o hexdump.o $(LIBS)

wdfdump: wdfdump.o
	$(CC) $(CFLAGS) -o wdfdump wdfdump.o $(LIBS)

xdmccc.o : cc.o  xdmccc.c
	$(CC) $(CFLAGS) -DPGM_XDMC -c xdmccc.c

xdmccc.c : 
	ln -s cc.c xdmccc.c

xdmc: xdmc.c getxopts.o debug.o xdmccc.o netlist.o hexdump.o link_date.o
	$(CC) $(CFLAGS) -o xdmc xdmc.c xdmccc.o getxopts.o debug.o netlist.o hexdump.o link_date.o $(LIBS)

ce: $(HFILES) $(CRPAD_OBS) $(EXTRA_OBJS) link_date.o
	$(CC) -o ce $(CFLAGS) $(CRPAD_OBS) $(EXTRA_OBJS) link_date.o $(LIBS)
#	chown root ce
#	chmod 4755 ce

ce_isceterm: ce_isceterm.o hexdump.o
	$(CC) $(CFLAGS) -o ce_isceterm ce_isceterm.o hexdump.o

apipastebuf.o : pastebuf.o  apipastebuf.c
	$(CC) $(CFLAGS) -DPGM_CEAPI -c apipastebuf.c

apipastebuf.c : 
	ln -s pastebuf.c apipastebuf.c

apinormalize.o : normalize.o  apinormalize.c
	$(CC) $(CFLAGS) -DPGM_CEAPI -UARPUS_CE -UPAD -c apinormalize.c

apinormalize.c : 
	ln -s normalize.c apinormalize.c

libceapi.a: ceapi.o debug.o xdmccc.o apipastebuf.o apinormalize.o hexdump.o
	rm -f libceapi.a
	ar r libceapi.a `lorder ceapi.o debug.o xdmccc.o apipastebuf.o apinormalize.o hexdump.o | tsort 2> /dev/null`

getxopts: getxopts.c  debug.o
	$(CC) $(CFLAGS) -DGETXOPTS_STAND_ALONE -o getxopts getxopts.c debug.o $(LIBS)

propnames: propnames.c
	$(CC) $(CFLAGS) -o propnames propnames.c $(LIBS)

pixels: pixels.c
	$(CC) $(CFLAGS) -o pixels pixels.c $(LIBS)

sync:
	(echo "$(SRCMODS) $(HFILES)  expilist.c expistamp.c link_date.c makefile.unixware makefile.objs makefile.depends" | (for i in `cat` ;do echo "/home/stymar/ce/$$i <holodeck> (f) ^/projects/ce/src.dev/$$i^" ; done) | node_audit -istdin -lstderr -q6061 -x -z -a)


link_date.o: $(CRPAD_OBS)
	rm -f link_date.c
	date '+char   *compile_date = "%h %d 19%y";' > link_date.c
	date '+char   *compile_time = "%T";' >> link_date.c
	$(CC) $(CFLAGS) -c link_date.c

$(SRCMODS) $(HFILES) expistamp.c expilist.c link_date.c:
	ln -s Xlib/$@ $@

tar:
	tar cf - $(SRCMODS) $(HFILES) expistamp.c expilist.c xdmc.c > ce.tar


