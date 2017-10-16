/*
 * $Id: view_slang.c,v 1.9 2017/10/14 00:57:34 tom Exp $
 *
 * This is adapted from slang's demo pager,
 * to make it behave like ncurses's view demo.
 */

#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

#include <slang.h>

#ifdef __GNUC__
static void
finish(int)
__attribute__((noreturn));
#endif

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
    /* Line_Window.border = 3; */

    return result;
}

static void
update_display(const char *filename)
{
    int row;
    MyData *line;

    /* All well behaved applications should block signals that may affect
     * the display while performing screen update.
     */
    SLsig_block_signals();

    Line_Window.nrows = (unsigned) (SLtt_Screen_Rows - 1);

    /* Always make the current line equal to the top window line. */
    if (Line_Window.top_window_line != NULL)
	Line_Window.current_line = Line_Window.top_window_line;

    SLscroll_find_top(&Line_Window);

    row = 0;
    line = (MyData *) Line_Window.top_window_line;

    SLsmg_normal_video();

    while (row < (int) Line_Window.nrows) {
	SLsmg_gotorc(row, 0);

	if (line != NULL) {
	    SLsmg_write_string(line->data);
	    line = line->next;
	}
	SLsmg_erase_eol();
	row++;
    }

    SLsmg_gotorc(row, 0);
    SLsmg_reverse_video();
    SLsmg_printf("%s | UTF-8 = %d",
		 (filename == NULL) ? "<stdin>" : filename,
		 SLutf8_is_utf8_mode());
    SLsmg_erase_eol();
    SLsmg_refresh();

    SLsig_unblock_signals();
}

static void
main_loop(const char *filename)
{
    int Screen_Start = 0;
    int screen_start;
    int done = 0;

    while (!done) {
	update_display(filename);
	switch (SLkp_getkey()) {
	case SL_KEY_ERR:
	case 'q':
	case 'Q':
	    done = 1;
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

	case SL_KEY_NPAGE:
	case ' ':
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
    fprintf(stderr, "Usage: %s [-c] [-i] [FILENAME]\n", pgm);
    exit(EXIT_FAILURE);
}

int
main(int argc, char **argv)
{
    int i;
    int try_color = 0;
    int ignore_sigs = 0;
    char *filename = NULL;
    MyData *my_data;

    while ((i = getopt(argc, argv, "ci")) != -1) {
	switch (i) {
	case 'c':
	    try_color = 1;
	    break;
	case 'i':
	    ignore_sigs = 1;
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

    main_loop(filename);
    finish(0);
}
