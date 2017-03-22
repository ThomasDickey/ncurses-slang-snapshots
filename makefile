PROGS	= \
	dots_slcurses \
	view_slang \
	view_slcurses

CC	= gcc-normal

LIBS	= -lslang -lm

.c:
	$(CC) -o $@ $< $(LIBS)

all: $(PROGS)

clean:
	rm -f $(PROGS) *.o

