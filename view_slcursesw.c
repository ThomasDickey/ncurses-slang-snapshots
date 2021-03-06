/****************************************************************************
 * Copyright (c) 2017 Thomas E. Dickey                                      *
 * Copyright (c) 1998-2015,2016 Free Software Foundation, Inc.              *
 *                                                                          *
 * Permission is hereby granted, free of charge, to any person obtaining a  *
 * copy of this software and associated documentation files (the            *
 * "Software"), to deal in the Software without restriction, including      *
 * without limitation the rights to use, copy, modify, merge, publish,      *
 * distribute, distribute with modifications, sublicense, and/or sell       *
 * copies of the Software, and to permit persons to whom the Software is    *
 * furnished to do so, subject to the following conditions:                 *
 *                                                                          *
 * The above copyright notice and this permission notice shall be included  *
 * in all copies or substantial portions of the Software.                   *
 *                                                                          *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS  *
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF               *
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.   *
 * IN NO EVENT SHALL THE ABOVE COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,   *
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR    *
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR    *
 * THE USE OR OTHER DEALINGS IN THE SOFTWARE.                               *
 *                                                                          *
 * Except as contained in this notice, the name(s) of the above copyright   *
 * holders shall not be used in advertising or otherwise to promote the     *
 * sale, use or other dealings in this Software without prior written       *
 * authorization.                                                           *
 ****************************************************************************/
/*
 * view.c -- a silly little viewer program
 *
 * written by Eric S. Raymond <esr@snark.thyrsus.com> December 1994
 * to test the scrolling code in ncurses.
 *
 * modified by Thomas Dickey <dickey@clark.net> July 1995 to demonstrate
 * the use of 'resizeterm()', and May 2000 to illustrate wide-character
 * handling.
 *
 * Takes a filename argument.  It's a simple file-viewer with various
 * scroll-up and scroll-down commands.
 *
 * n	-- scroll one line forward
 * p	-- scroll one line back
 *
 * Either command accepts a numeric prefix interpreted as a repeat count.
 * Thus, typing `5n' should scroll forward 5 lines in the file.
 *
 * The way you can tell this is working OK is that, in the trace file,
 * there should be one scroll operation plus a small number of line
 * updates, as opposed to a whole-page update.  This means the physical
 * scroll operation worked, and the refresh() code only had to do a
 * partial repaint.
 *
 * $Id: view_slcursesw.c,v 1.7 2017/10/13 21:07:29 tom Exp $
 */

#define _XOPEN_SOURCE 600	/* See feature_test_macros(7) */
#include <slcurses.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <wchar.h>
#include <assert.h>
#include <signal.h>
#include <locale.h>

#include <time.h>

#undef CTRL			/* conflict on AIX 5.2 with <sys/ioctl.h> */

/*
 * Bear in mind that this program stores an array of characters so that it can
 * shift left/right without distortion.  slang lacks most of the functionality
 * needed to make that work; it is up to applications to provide this.
 *
 * The array has to store Unicode base-characters with their combining
 * characters, if any.
 *
 * slcurses has the analogous type to curses "cchar_t", however slcurses mixes
 * the base character with color/attributes since the base is just a chtype.
 *
 * It has an array of combining characters -- but those are set indirectly via
 * the "addch" call.  There is no "setcchar" or "getcchar", nor is there any
 * point in providing one here (see note in "add_wchstr").
 *
 * Alternatively there's SLsmg_Char_Type, but introducing it here would be
 * no improvement.
 */
typedef SLcurses_Cell_Type cchar_t;
#define CCHARW_MAX SLSMG_MAX_CHARS_PER_CELL
typedef SLcurses_Char_Type attr_t;

#define UChar(c) (unsigned char)(c)

#define IGNORE_RC(func) (void) func
#define reset_mbytes(state) IGNORE_RC(mblen(NULL, 0)), IGNORE_RC(mbtowc(NULL, NULL, 0))
#define count_mbytes(buffer,length,state) mblen(buffer,length)
#define check_mbytes(wch,buffer,length,state) \
	(int) mbtowc(&wch, buffer, length)
#define state_unused

#define my_pair 1

#define OK (0)

#undef CTRL
#define CTRL(x)	((x) & 0x1f)

static int shift = 0;
static bool try_color = FALSE;

static char *fname;
static cchar_t **vec_lines;
static cchar_t **lptr;
static int num_lines;

static void
usage(void)
{
    static const char *msg[] =
    {
	"Usage: view [options] file"
	,""
	,"Options:"
	," -c       use color if terminal supports it"
	," -i       ignore INT, QUIT, TERM signals"
	," -n NUM   specify maximum number of lines (default 1000)"
	," -s       start in single-step mode, waiting for input"
#ifdef TRACE
	," -t       trace screen updates"
	," -T NUM   specify trace mask"
#endif
    };
    size_t n;
    for (n = 0; n < sizeof(msg) / sizeof(msg[0]); n++)
	fprintf(stderr, "%s\n", msg[n]);
    exit(EXIT_FAILURE);
}

#ifdef SLANG_VERSION

/* MISSING */
static void
add_wchstr(cchar_t *s)
{
    int n;

    while (s->main) {
	/*
	 * slcurses's "addch" is sufficiently different from curses that the
	 * only useful part is the character:
	 * a) only the foreground color is used, but
	 * b) the color+video attributes are OR'd with the window attributes,
	 * c) largely because slcurses confuses bold and reverse with color.
	 */
	addch(s->main & A_CHARTEXT);
	for (n = 0; n < SLSMG_MAX_CHARS_PER_CELL; ++n) {
	    addch(s->combining[n]);
	}
	++s;
    }
}

/* MISSING */
static void
halfdelay(int n)
{
    wtimeout(stdscr, n);
}

/* MISSING */
static char *
keyname(int c)
{
    static char result[80];
    if (c >= 256) {
	sprintf(result, "\\x%04x", c & 0xffff);
    } else if (c >= 128) {
	char temp[80];
	strcpy(temp, keyname(c - 128));
	sprintf(result, "M-%s", temp);
    } else if (c >= 127) {
	sprintf(result, "^?");
    } else if (c >= 32) {
	sprintf(result, "%c", c);
    } else {
	sprintf(result, "^%c", 64 | c);
    }
    return result;
}

/* MISSING */
static void
redrawwin(WINDOW *w)
{
    /* the touchwin() macro introduces a compiler warning, fixed here */
    SLsmg_touch_lines((int) (w)->_begy, (w)->nrows);
    wrefresh(w);
}

#if 0
/* slcurses doesn't have scrolling regions, though this function exists.
 * It simply doesn't work with slcurses.
 */
static void
setscrreg(int t, int b)
{
    SLtt_set_scroll_region(t, b);
}
#endif

#endif /* SLANG_VERSION */

static int
ch_len(cchar_t *src)
{
    int result = 0;

    while (src->main) {
	src++;
	result++;
    }
    return result;
}

/*
 * MISSING
 */
static int
setcchar(cchar_t *wcval, const wchar_t *wch, const attr_t attrs, short
	 color_pair, void *opts)
{
    int j;
    memset(wcval, 0, sizeof(*wcval));
    wcval->main = (SLcurses_Char_Type) wch[0];
    for (j = 1; wch[j] != 0; ++j)
	wcval->combining[j - 1] = (SLwchar_Type) wch[j];
    return OK;
}

/*
 * Allocate a string into an array of cchar_t's.
 */
static cchar_t *
ch_dup(char *src)
{
    unsigned len = (unsigned) strlen(src);
    cchar_t *dst = calloc(sizeof(cchar_t), len + 1);
    size_t j, k;
    wchar_t wstr[SLSMG_MAX_CHARS_PER_CELL + 1];
    wchar_t wch;
    int l = 0;
    size_t rc;
    int width;

    reset_mbytes(state);
    for (j = k = 0; j < len; j++) {
	rc = (size_t) check_mbytes(wch, src + j, len - j, state);
	if (rc == (size_t) -1 || rc == (size_t) -2) {
	    break;
	}
	j += rc - 1;
	width = wcwidth(wch);
	if (width == 0) {
	    if (l == 0) {
		wstr[l++] = L' ';
	    }
	} else if ((l > 0) || (l == CCHARW_MAX)) {
	    wstr[l] = L'\0';
	    l = 0;
	    if (setcchar(dst + k, wstr, 0, 0, NULL) != OK) {
		break;
	    }
	    ++k;
	}
	wstr[l++] = wch;
    }
    if (l > 0) {
	wstr[l] = L'\0';
	if (setcchar(dst + k, wstr, 0, 0, NULL) == OK)
	    ++k;
    }
    wstr[0] = L'\0';
    setcchar(dst + k, wstr, 0, 0, NULL);
    return dst;
}

#ifdef __GNUC__
#define GCC_NORETURN __attribute__((noreturn))
#else
#define GCC_NORETURN		/* nothing */
#endif

static void finish(int) GCC_NORETURN;

static void
finish(int sig)
{
    endwin();
    exit(sig != 0 ? EXIT_FAILURE : EXIT_SUCCESS);
}

static void
show_all(const char *tag)
{
    int i;
    char temp[BUFSIZ];
    cchar_t *s;
    time_t this_time;

    (void) tag;
    sprintf(temp, "view %.*s", (int) sizeof(temp) - 7, fname);

    move(0, 0);
    printw("%.*s", COLS, temp);
    clrtoeol();
    this_time = time((time_t *) 0);
    strncpy(temp, ctime(&this_time), (size_t) 30);
    if ((i = (int) strlen(temp)) != 0) {
	temp[--i] = 0;
	if (move(0, (unsigned) (COLS - i - 2)) != ERR)
	    printw("  %s", temp);
    }

    scrollok(stdscr, FALSE);	/* prevent screen from moving */
    for (i = 1; i < LINES; i++) {
	move((unsigned) i, 0);
	printw("%3ld:", (long) (lptr + i - vec_lines));
	clrtoeol();
	if ((s = lptr[i - 1]) != 0) {
	    int len = ch_len(s);
	    if (len > shift) {
		add_wchstr(s + shift);
	    }
	}
    }
    scrollok(stdscr, TRUE);
    /* MISSING setscrreg(1, LINES - 1); */
    refresh();
}

int
main(int argc, char *argv[])
{
    int MAXLINES = 1000;
    FILE *fp;
    char buf[BUFSIZ];
    int i;
    int my_delay = 0;
    cchar_t **olptr;
    int value = 0;
    bool done = FALSE;
    bool got_number = FALSE;
    bool single_step = FALSE;
    const char *my_label = "Input";

    setlocale(LC_ALL, "");

    /*
     * We know ncurses will catch SIGINT if we don't establish our own handler.
     * Other versions of curses may/may not catch it.
     */
    (void) signal(SIGINT, finish);	/* arrange interrupts to terminate */

    while ((i = getopt(argc, argv, "cin:stT:")) != -1) {
	switch (i) {
	case 'c':
	    try_color = TRUE;
	    break;
	case 'i':
	    signal(SIGINT, SIG_IGN);
	    signal(SIGQUIT, SIG_IGN);
	    signal(SIGTERM, SIG_IGN);
	    break;
	case 'n':
	    if ((MAXLINES = atoi(optarg)) < 1 ||
		(MAXLINES + 2) <= 1)
		usage();
	    break;
	case 's':
	    single_step = TRUE;
	    break;
#ifdef TRACE
	case 'T':
	    {
		char *next = 0;
		int tvalue = (int) strtol(optarg, &next, 0);
		if (tvalue < 0 || (next != 0 && *next != 0))
		    usage();
		trace((unsigned) tvalue);
	    }
	    break;
	case 't':
	    trace(TRACE_CALLS);
	    break;
#endif
	default:
	    usage();
	}
    }
    if (optind + 1 != argc)
	usage();

    if ((vec_lines = calloc(1, sizeof(cchar_t) * (size_t) MAXLINES + 2)) == 0)
	  usage();

    assert(vec_lines != 0);

    fname = argv[optind];
    if ((fp = fopen(fname, "r")) == 0) {
	perror(fname);
	exit(EXIT_FAILURE);
    }

    for (lptr = &vec_lines[0]; (lptr - vec_lines) < MAXLINES; lptr++) {
	if (fgets(buf, sizeof(buf), fp) == 0)
	    break;

	*lptr = ch_dup(buf);
    }
    (void) fclose(fp);
    num_lines = (int) (lptr - vec_lines);

    (void) initscr();		/* initialize the curses library */
    keypad(stdscr, TRUE);	/* enable keyboard mapping */
    (void) nonl();		/* tell curses not to do NL->CR/NL on output */
    (void) cbreak();		/* take input chars one at a time, no wait for \n */
    (void) noecho();		/* don't echo input */
    if (!single_step)
	nodelay(stdscr, TRUE);

    if (try_color) {
	if (has_colors()) {
	    start_color();
	    init_pair(my_pair, COLOR_WHITE, COLOR_BLUE);
	    attron(COLOR_PAIR(my_pair));	/* needed for slcurses */
	} else {
	    try_color = FALSE;
	}
    }

    lptr = vec_lines;
    while (!done) {
	int n, c;

	if (!got_number)
	    show_all(my_label);

	for (;;) {
	    c = getch();
	    if ((c < 127) && isdigit(c)) {
		if (!got_number) {
		    mvprintw(0, 0, "Count: ");
		    clrtoeol();
		}
		addch(UChar(c));
		value = 10 * value + (c - '0');
		got_number = TRUE;
	    } else
		break;
	}
	if (got_number && value) {
	    n = value;
	} else {
	    n = 1;
	}

	if (c != ERR)
	    my_label = keyname(c);
	switch (c) {
	case KEY_DOWN:
	case 'n':
	    olptr = lptr;
	    for (i = 0; i < n; i++)
		if ((lptr - vec_lines) < (num_lines - LINES + 1))
		    lptr++;
		else
		    break;
	    scrl((int) (lptr - olptr));
	    break;

	case KEY_UP:
	case 'p':
	    olptr = lptr;
	    for (i = 0; i < n; i++)
		if (lptr > vec_lines)
		    lptr--;
		else
		    break;
	    scrl((int) (lptr - olptr));
	    break;

	case 'h':
	case KEY_HOME:
	    lptr = vec_lines;
	    break;

	case 'e':
	case KEY_END:
	    if (num_lines > LINES)
		lptr = vec_lines + num_lines - LINES + 1;
	    else
		lptr = vec_lines;
	    break;

	case 'r':
	case KEY_RIGHT:
	    shift += n;
	    break;

	case 'l':
	case KEY_LEFT:
	    shift -= n;
	    if (shift < 0) {
		shift = 0;
		beep();
	    }
	    break;

	case 'q':
	    done = TRUE;
	    break;

	case 's':
	    if (got_number) {
		halfdelay(my_delay = n);
	    } else {
		nodelay(stdscr, FALSE);
		my_delay = -1;
	    }
	    break;
	case ' ':
	    nodelay(stdscr, TRUE);
	    my_delay = 0;
	    break;
	case CTRL('L'):
	    redrawwin(stdscr);
	    break;
	case ERR:
	    if (!my_delay)
		napms(50);
	    break;
	default:
	    beep();
	    break;
	}
	if (c >= KEY_MIN || (c > 0 && !isdigit(c))) {
	    got_number = FALSE;
	    value = 0;
	}
    }

    finish(0);			/* we're done */
}
