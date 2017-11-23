/****************************************************************************
 * Copyright 2017 by Thomas E. Dickey                                       *
 *                                                                          *
 * Permission is hereby granted, free of charge, to any person obtaining a  *
 * copy of this software and associated documentation files (the            *
 * "Software"), to deal in the Software without restriction, including      *
 * without limitation the rights to use, copy, modify, merge, publish,      *
 * distribute, distribute with modifications, sublicense, and/or sell       *
 * copies of the Software, and to permit persons to whom the Software is    *
 * furnished to do so, subject to the following conditions:                 *
 *                                                                          *
 * This is a supporting work for discussion of the ncurses and slang        *
 * libraries, consequently the permission notice requires this URL to be    *
 * included:                                                                *
 *      https://invisible-island.net/ncurses/ncurses-slang.html             *
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
 * $Id: picsmap_slang.c,v 1.12 2017/11/23 20:21:42 tom Exp $
 */

#define COLORS	SLtt_Use_Ansi_Colors
#define endwin() done_display()

#include <picsmap.h>

static int save_d_opt;

static void
done_display(void)
{
}

#include <picsmap.c>

static void
exit_error_hook(char *fmt, va_list ap)
{
    SLang_reset_tty();
    SLsmg_reset_smg();

    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    exit(EXIT_FAILURE);
}

void
init_display(const char *palette_path, int d_option)
{
    if (isatty(fileno(stdout))) {
	const char *env = getenv("TERM");
	int fake = 0;
	int colors;
	char chr;

	save_d_opt = d_option;
	if (!strcmp(env, "xterm-direct")) {
	    putenv("COLORTERM=24bit");
	    if (d_option)
		putenv("COLORTERM_BCE=1");
	    fake = 1;
	}

	SLang_Exit_Error_Hook = exit_error_hook;

	SLsig_block_signals();
	SLtt_get_terminfo();

	/*
	 * The slang library attempts to read binary terminfo.  Someday it may
	 * handle ncurses's extensions.
	 */
	if (fake) {
	    logmsg("faking 24-bit color");
	    COLORS = 0x1000000;
	} else if ((sscanf(env, "xterm-%dcolo%c", &colors, &chr) == 2)
		   && (colors > COLORS)) {
	    logmsg("workaround for %s, update colors from %d to %d",
		   env, COLORS, colors);
	    COLORS = colors;
	}

	if (-1 == SLkp_init()) {
	    SLsig_unblock_signals();
	    return;
	}

	SLang_init_tty(-1, 0, 1);
	SLtty_set_suspend_state(1);
	(void) SLutf8_enable(-1);
	if (-1 == SLsmg_init_smg()) {
	    SLsig_unblock_signals();
	    return;
	}

	SLsig_unblock_signals();

	init_palette(palette_path);
	in_curses = TRUE;
	done_display();
    }
}

static void
show_picture(PICS_HEAD * pics)
{
    int y, x;
    int n;
    SLsmg_Color_Type my_pair;
    SLtt_Char_Type my_color;

    debugmsg("called show_picture");
    SLsmg_touch_screen();
    if (SLtt_Use_Ansi_Colors) {
	logmsg("...using %d colors", pics->colors);
	for (n = 0; n < pics->colors; ++n) {
	    my_pair = (SLsmg_Color_Type) (n + 1);
	    my_color = (SLtt_Char_Type) map_color(fg_color(pics, n));
	    /* slang does not support default-colors with this interface */
	    if ((long) my_color < 0) {
		my_color = 0;
	    }
	    debugmsg("color %3u -> %06X", my_pair, my_color);
	    SLtt_set_color_fgbg(my_pair, my_color, my_color);
	}
    }
    SLsmg_set_color(1);
    SLsmg_fill_region(0, 0, (unsigned) LINES, (unsigned) COLS, ' ');
    for (y = 0; y < pics->high; ++y) {
	if (y >= LINES)
	    break;
	SLsmg_gotorc(y, 0);
	for (x = 0; x < pics->wide; ++x) {
	    if (x >= COLS)
		break;
	    n = (y * pics->wide + x);
	    my_pair = (SLsmg_Color_Type) (pics->cells[n].fg + 1);
	    SLsmg_set_color(my_pair);
	    SLsmg_write_char((SLwchar_Type) pics->cells[n].ch);
	}
    }
    if (slow_time >= 0) {
	SLsmg_refresh();
	if (slow_time > 0) {
	    sleep((unsigned) slow_time);
	}
    } else {
	SLsmg_gotorc(0, 0);
	SLsmg_refresh();
	SLkp_getkey();
    }
    if (!quiet)
	endwin();
}
