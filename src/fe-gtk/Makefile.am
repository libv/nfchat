bin_PROGRAMS = nfchat

CFLAGS = $(GUI_CFLAGS)
LIBS = $(GUI_LIBS)

INCLUDES = \
        -DLOCALEDIR=\"$(datadir)/locale\" \
        -I$(includedir)

nfchat_LDADD = ../common/libnfchatmain.a

nfchat_SOURCES =	fe-gtk.c fkeys.c \
			gtkutil.c maingui.c \
			menu.c palette.c \
			textgui.c userlistgui.c xtext.c
