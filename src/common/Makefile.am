## Process this file with automake to produce Makefile.in

if DO_PERL
perl_c = perl.c
endif

noinst_LIBRARIES = libnfchatmain.a

INCLUDES = \
	-I$(includedir)

libnfchatmain_a_SOURCES = cfgfiles.c ctcp.c dcc.c \
      history.c ignore.c \
		inbound.c notify.c outbound.c \
		$(perl_c) plugin.c server.c \
		text.c themes-common.c userlist.c util.c xchat.c

noinst_HEADERS = dcc.h \
		history.h \
		notify.h \
		plugin.h \
		userlist.h \
		util.h \
		xchat.h
