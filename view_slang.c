/* This file pager demo illustrates the screen management and
 * keyboard routines.
 */

#include <stdio.h>
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
	exit(1);
    }
    exit(sig);
}

static void
exit_error_hook(char *fmt, va_list ap)
{
    SLang_reset_tty();
    SLsmg_reset_smg();

    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    exit(1);
}

static int
InitializeTerminal(int tty, int smg)
{
    SLang_Exit_Error_Hook = exit_error_hook;

    SLsig_block_signals();
    SLtt_get_terminfo();

    if (tty && (-1 == SLkp_init())) {
	SLsig_unblock_signals();
	return -1;
    }

    SLsignal(SIGINT, finish);
    SLsignal(SIGQUIT, finish);
    SLsignal(SIGTERM, finish);

    if (tty)
	SLang_init_tty(-1, 0, 1);
    if (tty)
	SLtty_set_suspend_state(1);
    if (smg) {
	(void) SLutf8_enable(-1);

	if (-1 == SLsmg_init_smg()) {
	    SLsig_unblock_signals();
	    return -1;
	}
    }

    SLsig_unblock_signals();

    return 0;
}

static void
usage(char *pgm)
{
    fprintf(stderr, "Usage: %s [FILENAME]\n", pgm);
    exit(1);
}

static char *File_Name;		/* if NULL, use stdin */

/* The SLscroll routines will be used for pageup/down commands.  They assume
 * a linked list of lines.  The first element of the structure MUST point to
 * the NEXT line, the second MUST point to the PREVIOUS line.
 */
typedef struct _File_Line_Type {
    struct _File_Line_Type *next;
    struct _File_Line_Type *prev;
    char *data;			/* pointer to line data */
} File_Line_Type;

static File_Line_Type *File_Lines;

/* The SLscroll routines will use this structure. */
static SLscroll_Window_Type Line_Window;

static void
free_lines(void)
{
    File_Line_Type *line, *next;

    line = File_Lines;
    while (line != NULL) {
	next = line->next;
	if (line->data != NULL)
	    free(line->data);
	free(line);
	line = next;
    }
    File_Lines = NULL;
}

static File_Line_Type *
create_line(char *buf)
{
    File_Line_Type *line;

    line = (File_Line_Type *) malloc(sizeof(File_Line_Type));
    if (line == NULL)
	return NULL;

    memset((char *) line, 0, sizeof(File_Line_Type));

    line->data = SLmake_string(buf);	/* use a slang routine */
    if (line->data == NULL) {
	free(line);
	return NULL;
    }

    return line;
}

static int
ReadFile(char *file)
{
    FILE *fp;
    char buf[1024];
    File_Line_Type *line, *last_line;
    unsigned int num_lines;

    if (file == NULL)
	fp = stdin;
    else
	fp = fopen(file, "r");

    if (fp == NULL)
	return -1;

    last_line = NULL;
    num_lines = 0;

    while (NULL != fgets(buf, sizeof(buf), fp)) {
	num_lines++;

	if (NULL == (line = create_line(buf))) {
	    fprintf(stderr, "Out of memory.");
	    free_lines();
	    return -1;
	}

	if (last_line == NULL)
	    File_Lines = line;
	else
	    last_line->next = line;

	line->prev = last_line;
	line->next = NULL;

	last_line = line;
    }

    memset((char *) &Line_Window, 0, sizeof(SLscroll_Window_Type));

    Line_Window.current_line = (SLscroll_Type *) File_Lines;
    Line_Window.lines = (SLscroll_Type *) File_Lines;
    Line_Window.line_num = 1;
    Line_Window.num_lines = num_lines;
    /* Line_Window.border = 3; */

    return 0;
}

static void
update_display(void)
{
    int row;
    File_Line_Type *line;

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
    line = (File_Line_Type *) Line_Window.top_window_line;

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
		 (File_Name == NULL) ? "<stdin>" : File_Name,
		 SLutf8_is_utf8_mode());
    SLsmg_erase_eol();
    SLsmg_refresh();

    SLsig_unblock_signals();
}

static void
main_loop(void)
{
    int Screen_Start = 0;
    int screen_start;
    int done = 0;

    while (!done) {
	update_display();
	switch (SLkp_getkey()) {
	case SL_KEY_ERR:
	case 'q':
	case 'Q':
	    done = 1;
	    break;

	case SL_KEY_RIGHT:
	    Screen_Start += 1;
	    screen_start = Screen_Start;
	    SLsmg_set_screen_start(NULL, &screen_start);
	    break;

	case SL_KEY_LEFT:
	    Screen_Start -= 1;
	    if (Screen_Start < 0)
		Screen_Start = 0;
	    screen_start = Screen_Start;
	    SLsmg_set_screen_start(NULL, &screen_start);
	    break;

	case SL_KEY_UP:
	    SLscroll_prev_n(&Line_Window, 1);
	    Line_Window.top_window_line = Line_Window.current_line;
	    break;

	case '\r':
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

int
main(int argc, char **argv)
{
    if (argc == 2) {
	File_Name = argv[1];
    } else if ((argc != 1) || (1 == isatty(fileno(stdin))))
	usage(argv[0]);

    if (-1 == ReadFile(File_Name)) {
	fprintf(stderr, "Unable to read %s\n", File_Name);
	return EXIT_FAILURE;
    }

    if (-1 == InitializeTerminal(1, 1)) {
	fprintf(stderr, "Unable to initialize terminal.");
	return EXIT_FAILURE;
    }

    main_loop();
    finish(0);
}
