/*
 * $Id: view_slang.c,v 1.18 2017/11/02 19:28:42 tom Exp $
 *
 * This is adapted from slang's demo pager,
 * to make it behave like ncurses's view demo.
 */

#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>

#include <slang.h>
#include <slcurses.h>

#ifdef __GNUC__
#define GCC_NORETURN __attribute__((noreturn))
#else
#define GCC_NORETURN		/* nothing */
#endif

static void finish(int) GCC_NORETURN;

static void
finish(int sig)
{
    SLang_reset_tty();
    SLsmg_reset_smg();

    if (sig) {
	fprintf(stderr, "Exiting on signal %d\n", sig);
	exit(EXIT_FAILURE);
    }
    exit(EXIT_SUCCESS);
}

/* MISSING */
static const char *
unctrl(int c)
{
    static char table[256][6];
    char *result = 0;
    if (c >= 0 && c < 256) {
	result = &table[c][0];
	if (*result == 0) {
	    char *s = result;
	    if (c >= 128) {
		*s++ = 'M';
		*s++ = '-';
		c -= 128;
	    }
	    if (c < 32)
		sprintf(s, "^%c", c | '@');
	    else if (c < 127)
		sprintf(s, "%c", c);
	    else
		sprintf(s, "^?");
	}
    }
    return (const char *) result;
}

/* MISSING */
/*
 * These could be supplied by the slang library, but are here just for the
 * ncurses-examples which use these symbols.
 */
#define KEY_BTAB	0x114
#define KEY_PREVIOUS	0x115
#define KEY_NEXT	0x116
#define KEY_IC	SL_KEY_IC
#define KEY_DC	SL_KEY_DELETE

static const char *
keyname(int c)
{
#define SL_KEYNAMES(name) { name, #name }
    static struct {
	int code;
	const char *name;
    } table[] = {
	SL_KEYNAMES(KEY_DOWN),
	    SL_KEYNAMES(KEY_UP),
	    SL_KEYNAMES(KEY_LEFT),
	    SL_KEYNAMES(KEY_RIGHT),
	    SL_KEYNAMES(KEY_A1),
	    SL_KEYNAMES(KEY_C1),
	    SL_KEYNAMES(KEY_B2),
	    SL_KEYNAMES(KEY_A3),
	    SL_KEYNAMES(KEY_C3),
	    SL_KEYNAMES(KEY_REDO),
	    SL_KEYNAMES(KEY_UNDO),
	    SL_KEYNAMES(KEY_BACKSPACE),
	    SL_KEYNAMES(KEY_PPAGE),
	    SL_KEYNAMES(KEY_NPAGE),
	    SL_KEYNAMES(KEY_HOME),
	    SL_KEYNAMES(KEY_END),
	    SL_KEYNAMES(KEY_ENTER),
	    SL_KEYNAMES(KEY_IC),
	    SL_KEYNAMES(KEY_DC),
	/* not in SLcurses */
	    SL_KEYNAMES(KEY_BTAB),
	    SL_KEYNAMES(KEY_PREVIOUS),
	    SL_KEYNAMES(KEY_NEXT),
	{
	    0, 0
	}
    };
#undef SL_KEYNAMES
    const char *result = 0;
    if (c < 256) {
	result = unctrl(c);
    } else if (c < KEY_F0) {
	int n;
	for (n = 0; table[n].code; ++n) {
	    if (table[n].code == c) {
		result = table[n].name;
		break;
	    }
	}
    } else if (c < KEY_F0 + 60) {
	static char dummy[20];
	sprintf(dummy, "KEY_F%d", c - KEY_F0);
	result = dummy;
    }
    return result;
}

static void
exit_error_hook(char *fmt, va_list ap)
{
    SLang_reset_tty();
    SLsmg_reset_smg();

    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    exit(EXIT_FAILURE);
}

static int
InitializeTerminal(void)
{
    SLang_Exit_Error_Hook = exit_error_hook;

    SLsig_block_signals();
    SLtt_get_terminfo();

    if (-1 == SLkp_init()) {
	SLsig_unblock_signals();
	return -1;
    }

    SLang_init_tty(-1, 0, 1);
    SLtty_set_suspend_state(1);
    (void) SLutf8_enable(-1);
    if (-1 == SLsmg_init_smg()) {
	SLsig_unblock_signals();
	return -1;
    }

    SLsig_unblock_signals();

    return 0;
}

/* The SLscroll routines will be used for pageup/down commands.  They assume
 * a linked list of lines.  The first element of the structure MUST point to
 * the NEXT line, the second MUST point to the PREVIOUS line.
 */
typedef struct _MyData {
    struct _MyData *next;
    struct _MyData *prev;
    char *data;			/* pointer to line data */
} MyData;

/* The SLscroll routines will use this structure. */
static SLscroll_Window_Type Line_Window;

static void
free_lines(MyData * lines)
{
    MyData *line, *next;

    line = lines;
    while (line != NULL) {
	next = line->next;
	if (line->data != NULL)
	    free(line->data);
	free(line);
	line = next;
    }
}

static MyData *
create_line(char *buf)
{
    MyData *line;

    line = (MyData *) malloc(sizeof(MyData));
    if (line == NULL)
	return NULL;

    memset((char *) line, 0, sizeof(MyData));

    line->data = SLmake_string(buf);	/* use a slang routine */
    if (line->data == NULL) {
	free(line);
	return NULL;
    }

    return line;
}

static MyData *
ReadFile(char *filename)
{
    FILE *fp;
    char buf[1024];
    MyData *result = NULL;
    MyData *line, *last_line;
    unsigned int num_lines;

    if (filename == NULL)
	fp = stdin;
    else
	fp = fopen(filename, "r");

    if (fp == NULL)
	return NULL;

    last_line = NULL;
    num_lines = 0;

    while (NULL != fgets(buf, sizeof(buf), fp)) {
	num_lines++;

	if (NULL == (line = create_line(buf))) {
	    fprintf(stderr, "Out of memory.");
	    free_lines(result);
	    return NULL;
	}

	if (last_line == NULL)
	    result = line;
	else
	    last_line->next = line;

	line->prev = last_line;
	line->next = NULL;

	last_line = line;
    }

    memset((char *) &Line_Window, 0, sizeof(SLscroll_Window_Type));

    Line_Window.current_line = (SLscroll_Type *) result;
    Line_Window.lines = (SLscroll_Type *) result;
    Line_Window.line_num = 1;
    Line_Window.num_lines = num_lines;

    return result;
}

static void
update_header(const char *filename, int key)
{
    time_t now = time((time_t *) 0);
    int start_col = 0;

    SLsig_block_signals();

    SLsmg_set_screen_start(NULL, &start_col);

    SLsmg_gotorc(0, 0);
    /*
     * FIXME:
     * The pager.c demo has this call, which is misleading:
     *     SLsmg_reverse_video();
     * Rather than reverse-video, slang chooses to use the cyan color.
     * Like many of the screen management functions, this is undocumented.
     */
    SLsmg_printf("view %s %s",
		 (key > 0) ? keyname(key) : "",
		 (filename == NULL) ? "<stdin>" : filename);
    SLsmg_erase_eol();
    SLsmg_gotorc(0, SLtt_Screen_Cols - 24);
    SLsmg_printf("%s", ctime(&now));

    SLsmg_refresh();

    SLsig_unblock_signals();
}

static int
get_lineno(MyData * list, MyData * find)
{
    MyData *item = list;
    int result = 0;
    if (item != 0) {
	do {
	    ++result;
	    if (item == find)
		break;
	    item = item->next;
	} while ((item != 0) && (item != list));
    }
    return result;
}

static void
update_display(MyData * data, int start_col, int final_row, int no_number)
{
    int row;
    int param_col = start_col;
    int digit_col;
    int start_row;
    int digits = no_number ? -1 : 7;
    /*
     * FIXME:
     * SLscroll uses the whole-screen for tabs, has no way to expand
     * tabs except by referring to column-0, unlike ncurses.  So the number
     * of digits is chosen to make its tab-expansion work.
     */
    MyData *line;

    /* All well behaved applications should block signals that may affect
     * the display while performing screen update.
     */
    SLsig_block_signals();

    SLsmg_set_screen_start(NULL, &start_col);

    Line_Window.nrows = (unsigned) (SLtt_Screen_Rows - 1);

    /* Always make the current line equal to the top window line. */
    if (Line_Window.top_window_line != NULL)
	Line_Window.current_line = Line_Window.top_window_line;

    SLscroll_find_top(&Line_Window);

    row = 1;
    line = (MyData *) Line_Window.top_window_line;
    start_row = get_lineno(data, line);

    SLsmg_normal_video();

    start_col = param_col;
    SLsmg_set_screen_start(NULL, &start_col);

    while (row <= (int) Line_Window.nrows) {
	SLsmg_gotorc(row, digits + 1);

	if (line != NULL) {
	    SLsmg_write_string(line->data);
	    line = line->next;
	}
	SLsmg_erase_eol();
	row++;
    }

    if (!no_number) {
	digit_col = 0;
	SLsmg_set_screen_start(NULL, &digit_col);

	for (row = 1; row <= (int) Line_Window.nrows; ++row) {
	    SLsmg_gotorc(row, 0);
	    if (row + start_row - 1 > final_row) {
		SLsmg_erase_eos();
		SLsmg_gotorc(row - 1, param_col + digits + 1);
		break;
	    }
	    SLsmg_printf("%*d:", digits, row + start_row - 1);
	}

	start_col = param_col;
	SLsmg_set_screen_start(NULL, &start_col);
	SLsmg_gotorc(row - 1, param_col + digits + 1);
    }
    SLsmg_refresh();

    SLsig_unblock_signals();
}

/*
 * FIXME:
 * documentation for the SIGWINCH handler omits the "void" needed to compile.
 */
static volatile int Screen_Size_Changed;
static void
sigwinch_handler(int sig)
{
    Screen_Size_Changed = sig;
    SLsignal(SIGWINCH, sigwinch_handler);
}

static void
main_loop(MyData * data, const char *filename, int single_step, int no_number)
{
    int Screen_Start = 0;
    int screen_start;
    int screen_final = get_lineno(data, data->prev);
    int done = 0;
    int last_key = -1;

    SLsignal(SIGWINCH, sigwinch_handler);
    while (!done) {
	if (Screen_Size_Changed) {
	    Screen_Size_Changed = 0;
	    SLtt_get_screen_size();
	    SLsmg_reinit_smg();
	}
	update_header(filename, last_key);
	update_display(data, Screen_Start, screen_final, no_number);
	if (!single_step && !SLang_input_pending(-50))
	    continue;
	switch (last_key = SLkp_getkey()) {
	case SL_KEY_ERR:
	case 'q':
	case 'Q':
	    done = 1;
	    break;

	case 's':
	    single_step = 1;
	    break;

	case 'r':
	case SL_KEY_RIGHT:
	    Screen_Start += 1;
	    screen_start = Screen_Start;
	    SLsmg_set_screen_start(NULL, &screen_start);
	    break;

	case 'l':
	case SL_KEY_LEFT:
	    Screen_Start -= 1;
	    if (Screen_Start < 0)
		Screen_Start = 0;
	    screen_start = Screen_Start;
	    SLsmg_set_screen_start(NULL, &screen_start);
	    break;

	case 'p':
	case SL_KEY_UP:
	    SLscroll_prev_n(&Line_Window, 1);
	    Line_Window.top_window_line = Line_Window.current_line;
	    break;

	case '\r':
	case 'n':
	case SL_KEY_DOWN:
	    SLscroll_next_n(&Line_Window, 1);
	    Line_Window.top_window_line = Line_Window.current_line;
	    break;

	case ' ':
	    if (single_step) {
		single_step = 0;
		break;
	    }
	    /* FALLTHRU */
	case SL_KEY_NPAGE:
	case 4:
	    SLscroll_pagedown(&Line_Window);
	    break;

	case SL_KEY_PPAGE:
	case 127:
	case 21:
	    SLscroll_pageup(&Line_Window);
	    break;

	case SL_KEY_HOME:
	    while (-1 != SLscroll_pageup(&Line_Window)) ;
	    break;

	case SL_KEY_END:
	    while (-1 != SLscroll_pagedown(&Line_Window)) ;
	    break;

	default:
	    SLtt_beep();
	}
    }
}

static void
usage(char *pgm)
{
    fprintf(stderr, "Usage: %s [-c] [-i] [-n] [-s] [FILENAME]\n", pgm);
    exit(EXIT_FAILURE);
}

int
main(int argc, char **argv)
{
    int i;
    int try_color = 0;
    int ignore_sigs = 0;
    int single_step = 0;
    int no_numbers = 0;
    char *filename = NULL;
    MyData *my_data;

    while ((i = getopt(argc, argv, "cins")) != -1) {
	switch (i) {
	case 'c':
	    try_color = 1;
	    break;
	case 'i':
	    ignore_sigs = 1;
	    break;
	case 'n':
	    /*
	     * FIXME:
	     * SLscroll puts a '<' at the left margin when not able to show
	     * a double-cell character.  But that only works for column-0.
	     * If you display line-numbers (as in ncurses), that doesn't
	     * work.  This option is used to show the feature.
	     */
	    no_numbers = 1;
	    break;
	case 's':
	    single_step = 1;
	    break;
	default:
	    usage(argv[0]);
	}
    }
    if (optind + 1 != argc)
	usage(argv[0]);

    filename = argv[optind];

    if ((my_data = ReadFile(filename)) == NULL) {
	fprintf(stderr, "Unable to read %s\n", filename);
	return EXIT_FAILURE;
    }

    SLsignal(SIGINT, ignore_sigs ? SIG_IGN : finish);
    SLsignal(SIGQUIT, ignore_sigs ? SIG_IGN : finish);
    SLsignal(SIGTERM, ignore_sigs ? SIG_IGN : finish);

    if (-1 == InitializeTerminal()) {
	fprintf(stderr, "Unable to initialize terminal.");
	return EXIT_FAILURE;
    }

    if (try_color)
	SLtt_set_color(0, NULL, "white", "blue");
    main_loop(my_data, filename, single_step, no_numbers);
    finish(0);
}
