AM_CPPFLAGS = -I@top_srcdir@/include
AM_CXXFLAGS = -DSYSCONF_PATH=\"$(datadir)\" -std=c++11 -g -O3 -W -Wall -pedantic

bin_PROGRAMS = makemblem makembma checkmbma checkmblem testmbma froggen \
	morgen enr_chunkgen enr_nergen

makemblem_SOURCES = makemblem.cxx
checkmblem_SOURCES = checkmblem.cxx

makembma_SOURCES = makembma.cxx
checkmbma_SOURCES = checkmbma.cxx
testmbma_SOURCES = testmbma.cxx

froggen_SOURCES = froggen.cxx
morgen_SOURCES = morgen.cxx
enr_chunkgen_SOURCES = enr_chunkgen.cxx
enr_nergen_SOURCES = enr_nergen.cxx
