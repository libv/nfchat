## Process this file with automake to produce Makefile.in

if DO_PERL
perl_c = perl.c
endif

noinst_LIBRARIES = libnfchatmain.a

INCLUDES = \
	-I$(includedir)

libnfchatmain_a_SOURCES = cfgfiles.c ctcp.c \
                history.c \
		inbound.c outbound.c \
		$(perl_c) plugin.c server.c \
		text.c themes-common.c userlist.c util.c xchat.c

noinst_HEADERS = history.h \
		plugin.h \
		userlist.h \
		util.h \
		xchat.h
