
#  C_OPTS can be set from the shell environment to turn on -g
#  set C_OPTS='-g'; export C_OPTS

CC           =  /usr/bin/gcc	
DFLAGS       =\
 -DSYS5\
 -DPAD\
 -DDebuG\
 -DCE_LITTLE_ENDIAN\
 -DNeedFunctionPrototypes\
 -DARPUS_CE\
 -DHAVE_GETPT\
 -DHAVE_SNPRINTF\
 -DHAVE_X11_SM_SMLIB_H\
 -DNO_LICENSE
CFLAGS       = -g $(C_OPTS)  -I/usr/X11R6/include $(DFLAGS)
GFLAGS       = 
LIBS         = -L /usr/X11R6/lib/ -lX11 -lICE -lSM


# The following include sets variable CRPAD_OBS
include makefile.objs

EXTRA_OBJS    =  usleep.o

MAKEFILES = makefile.apollo makefile.bsdi makefile.cygnus makefile.decalpha makefile.decultrix \
 makefile.freebsd makefile.hpux10 makefile.hpux11 makefile.hpux9 \
 makefile.ibm makefile.linux makefile.omvs makefile.sco makefile.sgi \
 makefile.solaris makefile.solarisX86 makefile.stratus makefile.sunos makefile.unixware \
 makefile.objs makefile.depends



SRCMODS  = $(CRPAD_OBS:.o=.c)  $(EXTRA_OBJS:.o=.c) xdmc.c ceapi.c ce_isceterm.c

default: ce xdmc ce_isceterm

# The following include sets variable HFILES and sets up the .h file dependencies
include makefile.depends


# rule puts cflags at the end so we cans what is being compiled
#.c.o:
#	$(CC) -c $< $(CFLAGS)


#expidate.o: expidate.h expidate.c expistamp
#	$(CC) -c expidate.c $(CFLAGS)
#	./expistamp -d 9999/12/31 1001 expidate.o
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
	$(CC) $(CFLAGS) -o xdmc xdmc.c xdmccc.o getxopts.o debug.o netlist.o hexdump.o usleep.o link_date.o $(LIBS)

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
	ar r libceapi.a ceapi.o debug.o xdmccc.o apipastebuf.o apinormalize.o hexdump.o

getxopts: getxopts.c  debug.o
	$(CC) $(CFLAGS) -DGETXOPTS_STAND_ALONE -o getxopts getxopts.c debug.o $(LIBS)

propnames: propnames.c
	$(CC) $(CFLAGS) -o propnames propnames.c $(LIBS)

sm_test: sm_test.c
	$(CC) $(CFLAGS) -o sm_test sm_test.c  -L/usr/X11R6/lib -lICE -lSM $(LIBS)

sync:
	(echo "$(SRCMODS) $(HFILES)  link_date.c makefile.unixware makefile.objs makefile.depends" | (from="/projects/ce/src.dev";to="/home/stymar/ce"; for i in `cat` ;do echo "$$to/$$i <//holodeck> (f) ^$$from/$$i^" ; done) | node_audit -istdin -lstderr -q6061 -x -z -a)

# Run from styma5 with NFS mounts
update:
	/home/stymar/bin/update_sccs  etg/ce $(HFILES) $(SRCMODS) pad4NT.c execute4NT.c TESTPLAN ce_init ce_install ce_deinstall ce_update ce_report makefile.objs makefile.depends

# Run from styma5 with NFS mounts
backup:
	/home/stymar/bin/backup_sccs  $(SRCMODS) $(HFILES)  $(MAKEFILES) tsim.c pad4NT.c execute4NT.c  TESTPLAN ce_init ce_install ce_deinstall ce_update ce_report 

# Run from with NFS mounts
diff:
	/home/stymar/bin/diff_sccs  etg/ce $(HFILES) $(SRCMODS) pad4NT.c execute4NT.c TESTPLAN ce_init ce_install ce_deinstall ce_update ce_report makefile.objs makefile.depends


link_date.o: $(CRPAD_OBS)
	rm -f link_date.c
	date '+char   *compile_date = "%h %d %Y";' > link_date.c
	date '+char   *compile_time = "%T";' >> link_date.c
	$(CC) $(CFLAGS) -c link_date.c

#$(SRCMODS) $(HFILES) link_date.c:
#	ln -s Xlib/$@ $@

tar:
	tar czf cesrc.tgz  $(SRCMODS) $(HFILES) xdmc.c ce_isceterm.c pad4NT.c execute4NT.c makefile.*


