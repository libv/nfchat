## Process this file with automake to produce Makefile.in

noinst_LIBRARIES = libnfchatmain.a

INCLUDES = \
	-I$(includedir)

libnfchatmain_a_SOURCES = cfgfiles.c ctcp.c \
                history.c \
		inbound.c outbound.c \
		signals.c server.c \
		text.c userlist.c util.c xchat.c

noinst_HEADERS = history.h \
		signals.h \
		userlist.h \
		util.h \
		xchat.h
