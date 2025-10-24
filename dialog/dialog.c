/*
 *  dialog - Display simple dialog boxes from shell scripts
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

static void Usage (const char *name);

static int separate_output = 0;

typedef int (jumperFn) (const char *title, int argc, const char * const * argv);

struct Mode {
    char *name;
    int argmin, argmax, argmod;
    jumperFn *jumper;
};

jumperFn j_yesno, j_msgbox, j_infobox, j_textbox, j_menu;
jumperFn j_checklist, j_radiolist, j_inputbox, j_gauge;

/*
 * All functions are used in the slackware root disk, apart from "gauge"
 */

static struct Mode modes[] =
{
    {"--yesno", 5, 5, 1, j_yesno},
    {"--msgbox", 5, 5, 1, j_msgbox},
    {"--infobox", 5, 5, 1, j_infobox},
    {"--textbox", 5, 5, 1, j_textbox},
    {"--menu", 8, 0, 2, j_menu},
    {"--checklist", 9, 0, 3, j_checklist},
    {"--radiolist", 9, 0, 3, j_radiolist},
    {"--inputbox", 5, 6, 1, j_inputbox},
#ifdef HAVE_GAUGE
    {"--gauge", 6, 6, 1, j_gauge},
#endif
    {NULL, 0, 0, 0, NULL}
};

static struct Mode *modePtr;

#ifdef LOCALE
#include <locale.h>
#endif

int
main (int argc, const char * const * argv)
{
    int offset = 0, clear_screen = 0, end_common_opts = 0, retval;
    const char *title = NULL;

#ifdef LOCALE
    (void) setlocale (LC_ALL, "");
#endif

    if (argc < 2) {
	Usage (argv[0]);
	exit (-1);
    }
#ifdef HAVE_RC_FILE

    else if (!strcmp (argv[1], "--create-rc")) {
#ifdef HAVE_NCURSES
	if (argc != 3) {
	    Usage (argv[0]);
	    exit (-1);
	}
	_dialog_create_rc (argv[2]);
	return 0;
#else
	fprintf (stderr, "This option is currently unsupported "
		 "on your system.\n");
	return -1;
#endif
    }
#endif

    while (offset < argc - 1 && !end_common_opts) {	/* Common options */
	if (!strcmp (argv[offset + 1], "--title")) {
	    if (argc - offset < 3 || title != NULL) {
		Usage (argv[0]);
		exit (-1);
	    } else {
		title = argv[offset + 2];
		offset += 2;
	    }
	} else if (!strcmp (argv[offset + 1], "--backtitle")) {
	    if (backtitle != NULL) {
		Usage (argv[0]);
		exit (-1);
	    } else {
		backtitle = argv[offset + 2];
		offset += 2;
	    }
	} else if (!strcmp (argv[offset + 1], "--separate-output")) {
	    separate_output = 1;
	    offset++;
	} else if (!strcmp (argv[offset + 1], "--clear")) {
	    if (clear_screen) {	/* Hey, "--clear" can't appear twice! */
		Usage (argv[0]);
		exit (-1);
	    } else if (argc == 2) {	/* we only want to clear the screen */
		dialog_init ();
		refresh ();	/* dialog_init() will clear the screen for us */
		dialog_end ();
		return 0;
	    } else {
		clear_screen = 1;
		offset++;
	    }
	} else			/* no more common options */
	    end_common_opts = 1;
    }

    if (argc - 1 == offset) {	/* no more options */
	Usage (argv[0]);
	exit (-1);
    }
    /* use a table to look for the requested mode, to avoid code duplication */

    for (modePtr = modes; modePtr->name; modePtr++)	/* look for the mode */
	if (!strcmp (argv[offset + 1], modePtr->name))
	    break;

    if (!modePtr->name)
	Usage (argv[0]);
    if (argc - offset < modePtr->argmin)
	Usage (argv[0]);
    if (modePtr->argmax && argc - offset > modePtr->argmax)
	Usage (argv[0]);
    if ((argc - offset) % modePtr->argmod)
	Usage (argv[0]);

    dialog_init ();
    retval = (*(modePtr->jumper)) (title, argc - offset, argv + offset);

    if (clear_screen) {		/* clear screen before exit */
	_dialog_attr_clear (stdscr, LINES, COLS, screen_attr);
	refresh ();
    }
    dialog_end ();

    exit (retval);
}

/*
 * Print program usage
 */
static void
Usage (const char *name)
{
    fprintf (stderr, "\
\ndialog version " DIALOG_VERSION ".\
\nDisplay dialog boxes from shell scripts *\
\n\
\nUsage: %s --clear\
\n       %s --create-rc <file>\
\n       %s [--title <title>] [--separate-output] [--backtitle <backtitle>] [--clear] <Box options>\
\n\
\nBox options:\
\n\
\n  --yesno     <text> <height> <width>\
\n  --msgbox    <text> <height> <width>\
\n  --infobox   <text> <height> <width>\
\n  --inputbox  <text> <height> <width> [<init>]\
\n  --textbox   <file> <height> <width>\
\n  --menu      <text> <height> <width> <menu height> <tag1> <item1>...\
\n  --checklist <text> <height> <width> <list height> <tag1> <item1> <status1>...\
\n  --radiolist <text> <height> <width> <list height> <tag1> <item1> <status1>...\
\n  --gauge     <text> <height> <width> <percent>\n", name, name, name);
    exit (-1);
}

/*
 * These are the program jumpers
 */

int
j_yesno (const char *t, int ac, const char * const * av)
{
    return dialog_yesno (t, av[2], atoi (av[3]), atoi (av[4]));
}

int
j_msgbox (const char *t, int ac, const char * const * av)
{
    return dialog_msgbox (t, av[2], atoi (av[3]), atoi (av[4]), 1);
}

int
j_infobox (const char *t, int ac, const char * const * av)
{
    return dialog_msgbox (t, av[2], atoi (av[3]), atoi (av[4]), 0);
}

int
j_textbox (const char *t, int ac, const char * const * av)
{
    return dialog_textbox (t, av[2], atoi (av[3]), atoi (av[4]));
}

int
j_menu (const char *t, int ac, const char * const * av)
{
    int ret = dialog_menu (t, av[2], atoi (av[3]), atoi (av[4]),
			atoi (av[5]), (ac - 6) / 2, av + 6);
    if (ret >= 0) {
	fprintf(stderr, av[6 + ret * 2]);
	return 0;
    } else if (ret == -2)
	return 1;	/* CANCEL */
    return ret;		/* ESC pressed, ret == -1 */
}

int
j_checklist (const char *t, int ac, const char * const * av)
{
    int item_no = (ac - 6) / 3;
    int i, ret, *status = malloc (sizeof (int) * item_no);
    if (status == NULL) {
	endwin ();
	fprintf (stderr, "\nCan't allocate memory in dialog_checklist().\n");
	exit (-1);
    }
    ret = dialog_checklist (t, av[2], atoi (av[3]), atoi (av[4]),
	atoi (av[5]), item_no, av + 6, FLAG_CHECK, status);
    if (ret == 0) {
	for (i = 0; i < item_no; i++)
	    if (status[i]) {
		if (separate_output)
		    fprintf (stderr, "%s\n", (av + 6)[i * 3]);
		else
		    fprintf (stderr, "\"%s\" ", (av + 6)[i * 3]);
	    }
    }
    free (status);
    return ret;
}

int
j_radiolist (const char *t, int ac, const char * const * av)
{
    int item_no = (ac - 6) / 3;
    int i, ret, *status = malloc (sizeof (int) * item_no);
    if (status == NULL) {
	endwin ();
	fprintf (stderr, "\nCan't allocate memory in dialog_checklist().\n");
	exit (-1);
    }
    ret = dialog_checklist (t, av[2], atoi (av[3]), atoi (av[4]),
	atoi (av[5]), item_no, av + 6, FLAG_RADIO, status);
    if (ret == 0) {
	for (i = 0; i < item_no; i++)
	    if (status[i])
		fprintf (stderr, "%s", (av + 6)[i * 3]);
    }
    free (status);
    return ret;
}

int
j_inputbox (const char *t, int ac, const char * const * av)
{
    int ret = dialog_inputbox (t, av[2], atoi (av[3]), atoi (av[4]),
			    ac == 6 ? av[5] : (char *) NULL);
    if (ret == 0)
	fprintf(stderr, dialog_input_result);
    return ret;
}

#ifdef HAVE_GAUGE
int
j_gauge (const char *t, int ac, const char * const * av)
{
    return dialog_gauge (t, av[2], atoi (av[3]), atoi (av[4]),
			 atoi (av[5]));
}
#endif
