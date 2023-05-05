AM_CPPFLAGS = -I@top_srcdir@/include
AM_CXXFLAGS = -DSYSCONF_PATH=\"$(datadir)\" -std=c++14 -g -O3 -W -Wall -pedantic

bin_PROGRAMS = makemblem makembma checkmbma checkmblem testmbma froggen \
	morgen chunkgen nergen

makemblem_SOURCES = makemblem.cxx
checkmblem_SOURCES = checkmblem.cxx

makembma_SOURCES = makembma.cxx
checkmbma_SOURCES = checkmbma.cxx
testmbma_SOURCES = testmbma.cxx

froggen_SOURCES = froggen.cxx
morgen_SOURCES = morgen.cxx
chunkgen_SOURCES = chunkgen.cxx
nergen_SOURCES = nergen.cxx
