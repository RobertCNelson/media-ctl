lib_LTLIBRARIES = libmedia.la libsubdev.la
libmedia_la_SOURCES = media.c
libsubdev_la_SOURCES = subdev.c
include_HEADERS = media.h subdev.h

bin_PROGRAMS = media-ctl
media_ctl_SOURCES = main.c options.c options.h tools.h
media_ctl_LDADD = libmedia.la libsubdev.la

