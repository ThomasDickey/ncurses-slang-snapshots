PROGS	= \
	dots_slcurses \
	view_slang \
	view_slcurses \
	view_slcursesw

CC	= gcc-normal -W

LIBS	= -lslang -lm

.c:
	$(CC) -o $@ $< $(LIBS)

all: $(PROGS)

clean:
	rm -f $(PROGS) *.o

