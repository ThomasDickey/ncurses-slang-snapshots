##############################################################################
# Copyright 2017 by Thomas E. Dickey                                         #
#                                                                            #
# Permission is hereby granted, free of charge, to any person obtaining a    #
# copy of this software and associated documentation files (the "Software"), #
# to deal in the Software without restriction, including without limitation  #
# the rights to use, copy, modify, merge, publish, distribute, distribute    #
# with modifications, sublicense, and/or sell copies of the Software, and to #
# permit persons to whom the Software is furnished to do so, subject to the  #
# following conditions:                                                      #
#                                                                            #
# This is a supporting work for discussion of the ncurses and slang          #
# libraries, consequently the permission notice requires this URL to be      #
# included:                                                                  #
#      https://invisible-island.net/ncurses/ncurses-slang.html               #
#                                                                            #
# The above copyright notice and this permission notice shall be included in #
# all copies or substantial portions of the Software.                        #
#                                                                            #
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR #
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,   #
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL   #
# THE ABOVE COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER      #
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING    #
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER        #
# DEALINGS IN THE SOFTWARE.                                                  #
#                                                                            #
# Except as contained in this notice, the name(s) of the above copyright     #
# holders shall not be used in advertising or otherwise to promote the sale, #
# use or other dealings in this Software without prior written               #
# authorization.                                                             #
##############################################################################
# $Id: makefile,v 1.7 2017/11/15 23:25:31 tom Exp $

PROGS	= \
	dots_slcurses \
	picsmap_slang \
	view_slang \
	view_slcurses \
	view_slcursesw

CC	= gcc-normal -W
CPPFLAGS= -I.

LIBS	= -lslang -lm

.c:
	$(CC) -o $@ $(CPPFLAGS) $< $(LIBS)

all: $(PROGS)

clean:
	rm -f $(PROGS) *.o

