# basics
MAJORVERSION = 0
SUBVERSION   = 2
PATCHLEVEL   = 5
BUILDNAME    = vanilla

VERSION = ${MAJORVERSION}.${SUBVERSION}.${PATCHLEVEL}-${BUILDNAME}

# directories (KEEP TRAILING SLASH!)
INDIR = in/
METADIR = meta/
OUTDIR = out/

# flags
CFLAGS   = -std=c99 -Wall -Wextra -pedantic -Ofast -Iinclude -I.
CPPFLAGS = -D_XOPEN_SOURCE=700 -DVERSION=\"${VERSION}\"
LDFLAGS  = -Ofast -static
CCFLAGS  = ${CFLAGS} ${CPPFLAGS}

# compiler
CC = gcc
