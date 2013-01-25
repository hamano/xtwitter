AM_CFLAGS=-g -Wall -Wunused-function -Wunused-but-set-variable
bin_PROGRAMS = xtwitter
xtwitter_SOURCES = xtwitter.c libtwitter.c libtwitter.h
xtwitter_CFLAGS = $(AM_CFLAGS) @X11_CFLAGS@ @IMLIB_CFLAGS@ @CURL_CFLAGS@ @OAUTH_CFLAGS@ @JSON_CFLAGS@
xtwitter_LDFLAGS = @X11_LIBS@ @IMLIB_LIBS@ @CURL_LIBS@ @OAUTH_LIBS@ @JSON_LIBS@
