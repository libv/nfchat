if USE_MYGLIB
glibc = glib.c
endif

bin_PROGRAMS = xchat-text

xchat_text_LDADD = ../common/libxchatmain.a

xchat_text_SOURCES = fe-text.c $(glibc)
