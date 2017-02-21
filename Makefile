# contrib/pgexportdoc/Makefile

PGFILEDESC = "pgexportdoc - export XML, JSON, BYTEA documents from PostgreSQL"
PGAPPICON = win32

PROGRAM = pgexportdoc
OBJS	= pgexportdoc.o $(WIN32RES)

PG_CPPFLAGS = -I$(libpq_srcdir)
PG_LIBS = $(libpq_pgport)

ifdef NO_PGXS
subdir = contrib/pgexportdoc
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
else
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
endif

