bin_PROGRAMS = nfchat

CFLAGS = $(GUI_CFLAGS)
LIBS = $(GUI_LIBS)

INCLUDES = \
        -DLOCALEDIR=\"$(datadir)/locale\" \
        -I$(includedir)

nfchat_LDADD = ../common/libnfchatmain.a

nfchat_SOURCES =	ascii.c dialog.c \
			editlist.c fe-gtk.c fkeys.c \
			gtkutil.c \
			maingui.c \
			menu.c palette.c \
			textgui.c themes.c userlistgui.c xtext.c
