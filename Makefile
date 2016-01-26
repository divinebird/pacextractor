#CFLAGS=-O3 -Wall
CFLAGS=-g -ggdb -Wall
#LDLIBS=-l

all: pacextractor

version.h:
	if [ ! -f version.h ]; then \
	if [ -d .git ]; then \
	echo '#define VERSION_STR "$(shell git describe --tags --abbrev=0)"' > version.h; \
	else \
	echo '#define VERSION_STR ""' > version.h; \
	fi \
	fi

pacextractor.o: version.h

clean:
	rm -f pacextractor *.o version.h

.PHONY:	clean all