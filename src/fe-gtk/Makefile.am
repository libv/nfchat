bin_PROGRAMS = nfchat

CFLAGS = $(GUI_CFLAGS)
LIBS = $(GUI_LIBS)

INCLUDES = \
        -DLOCALEDIR=\"$(datadir)/locale\" \
        -I$(includedir)

nfchat_LDADD = ../common/libnfchatmain.a

nfchat_SOURCES =	ascii.c chanlist.c dccgui.c dialog.c \
			editlist.c fe-gtk.c fkeys.c \
			gtkutil.c ignoregui.c \
			maingui.c \
			menu.c notifygui.c palette.c \
			plugingui.c rawlog.c \
			settings.c \
			textgui.c themes.c usermenu.c urlgrab.c userlistgui.c xtext.c
