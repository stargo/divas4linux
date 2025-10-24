/*
 *  yesno.c -- implements the yes/no box
 *
 *  AUTHOR: Savio Lam (lam836@cs.cuhk.hk)
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "dialog.h"

/*
 * Display a dialog box with two buttons - Yes and No
 */
int
dialog_yesno (const char *title, const char *prompt, int height, int width)
{
    int i, x, y, key = 0, button = 0;
    WINDOW *dialog;

    /* center dialog box on screen */
    x = (COLS - width) / 2;
    y = (LINES - height) / 2;

    dialog = _dialog_newwin (height, width, y, x, 1);

    wattrset (dialog, border_attr);
    wmove (dialog, height - 3, 0);
    waddch (dialog, ACS_LTEE);
    for (i = 0; i < width - 2; i++)
	waddch (dialog, ACS_HLINE);
    wattrset (dialog, dialog_attr);
    waddch (dialog, ACS_RTEE);
    wmove (dialog, height - 2, 1);
    for (i = 0; i < width - 2; i++)
	waddch (dialog, ' ');

    if (title != NULL) {
	wattrset (dialog, title_attr);
	wmove (dialog, 0, (width - strlen (title)) / 2 - 1);
	waddch (dialog, ' ');
	waddstr (dialog, title);
	waddch (dialog, ' ');
    }
    _dialog_print_autowrap (dialog, prompt, width - 2, 1, 3);

    x = width / 2 - 10;
    y = height - 2;
    _dialog_print_button (dialog, "  No  ", y, x + 13, FALSE);
    _dialog_print_button (dialog, " Yes ", y, x, TRUE);
    wrefresh (dialog);

    while (key != ESC) {
	key = mouse_wgetch (dialog);
	switch (key) {
	case 'Y':
	case 'y':
	    delwin (dialog);
	    return 0;
	case 'N':
	case 'n':
	    delwin (dialog);
	    return 1;

	case M_EVENT + 'y':	/* mouse enter... */
	case M_EVENT + 'n':	/* use the code for toggling */
	    button = (key == M_EVENT + 'y');

	case TAB:
	case KEY_UP:
	case KEY_DOWN:
	case KEY_LEFT:
	case KEY_RIGHT:
	    if (!button) {
		button = 1;	/* "No" button selected */
		_dialog_print_button (dialog, " Yes ", y, x, FALSE);
		_dialog_print_button (dialog, "  No  ", y, x + 13, TRUE);
	    } else {
		button = 0;	/* "Yes" button selected */
		_dialog_print_button (dialog, "  No  ", y, x + 13, FALSE);
		_dialog_print_button (dialog, " Yes ", y, x, TRUE);
	    }
	    wrefresh (dialog);
	    break;
	case ' ':
	case '\n':
	    delwin (dialog);
	    return button;
	case ESC:
	    break;
	}
    }

    delwin (dialog);
    return -1;			/* ESC pressed */
}
