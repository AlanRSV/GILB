# The music renderer (render.c): the port's OPL2 synthesis and MUS player, built for the PC.
#   make -C ports/doom/tools/music        ./doom_music DOOM1.WAD OUTDIR
CC     ?= gcc
CFLAGS := -O2 -std=gnu99 -w -Ihost -I../../include

doom_music: render.c ../../source/opl2.c ../../source/mus.c $(wildcard host/*.h)
	$(CC) $(CFLAGS) render.c ../../source/opl2.c ../../source/mus.c -o $@

clean:
	rm -f doom_music
.PHONY: clean
