bin_PROGRAMS = xchat

if DO_PANEL
panel_c = panel.c
endif

CFLAGS = $(GUI_CFLAGS)
LIBS = $(GUI_LIBS)

INCLUDES = \
        -DLOCALEDIR=\"$(datadir)/locale\" \
        -I$(includedir) \
        $(GNOME_INCLUDEDIR)

xchat_LDADD = ../common/libxchatmain.a

xchat_SOURCES =		about.c ascii.c chanlist.c dccgui.c dialog.c \
			editlist.c fe-gtk.c fkeys.c \
			gtkutil.c ignoregui.c \
			maingui.c \
			menu.c notifygui.c palette.c $(panel_c) \
			plugingui.c rawlog.c \
			search.c serverlist.c settings.c \
			textgui.c themes.c usermenu.c urlgrab.c userlistgui.c xtext.c
