/* $Id$ */

/*
 *  (C) Copyright 2003 Wojtek Kaniewski <wojtekka@irc.pl>
 * 	 	  2004 Piotr Kupisiewicz <deletek@ekg2.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License Version 2 as
 *  published by the Free Software Foundation.
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

#include "ekg2-config.h"

#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#include <ekg/plugins.h>
#include <ekg/stuff.h>
#include <ekg/vars.h>
#include <ekg/windows.h>
#include <ekg/themes.h>
#include <ekg/xmalloc.h>

#include "bindings.h"
#include "ecurses.h"
#include "old.h"
#include "contacts.h"
#include "mouse.h"

static int ncurses_plugin_destroy();

plugin_t ncurses_plugin = {
	name: "ncurses",
	pclass: PLUGIN_UI,
	destroy: ncurses_plugin_destroy
};

static int ncurses_beep(void *data, va_list ap)
{
	beep();
	
	return 0;
}

static int dd_contacts(const char *name)
{
	return (config_contacts);
}

static void ncurses_statusbar_timer(int destroy, void *data)
{
	update_statusbar(1);
}

static int ncurses_statusbar_query(void *data, va_list ap)
{
	update_statusbar(1);
	return 0;
}

static int ncurses_ui_is_initialized(void *data, va_list ap)
{
        int *tmp = va_arg(ap, int *);
	
	if (ncurses_initialized)
		*tmp = 1;
	else
		*tmp = 0;	

	return 0;
}


static int ncurses_ui_window_switch(void *data, va_list ap)
{
	window_t **w = va_arg(ap, window_t **);
	ncurses_window_t *n = (*w)->private;

        list_destroy(sorted_all_cache, 1);
        sorted_all_cache = NULL;

	if (n->redraw)
		ncurses_redraw(*w);

	touchwin(n->window);

	update_statusbar(0);

	ncurses_commit();

	return 0;
}

int ncurses_ui_window_print(void *data, va_list ap)
{
	window_t **W = va_arg(ap, window_t **);
	window_t *w = *W;
	fstring_t **Line = va_arg(ap, fstring_t **);
	fstring_t *line = *Line;
	ncurses_window_t *n = w->private;
	int bottom = 0, prev_count = n->lines_count, count = 0;

	if (w->id == 0) {
		fstring_free(line);
		return 0;
	}
	
	if (n->start == n->lines_count - w->height || (n->start == 0 && n->lines_count <= w->height))
		bottom = 1;
	
	count = ncurses_backlog_add(w, line);

	if (n->overflow) {
		n->overflow -= count;

		if (n->overflow < 0) {
			bottom = 1;
			n->overflow = 0;
		}
	}

	if (bottom)
		n->start = n->lines_count - w->height;
	else {
		if (n->backlog_size == config_backlog_size)
			n->start -= count - (n->lines_count - prev_count);
	}

	if (n->start < 0)
		n->start = 0;

	if (n->start < n->lines_count - w->height)
		w->more = 1;

	if (!w->floating) {
		ncurses_redraw(w);
		if (!window_lock_get(w)) // && w == window_current) it should be tested
			ncurses_commit();
	}
	
	return 0;
}

static int ncurses_ui_window_new(void *data, va_list ap)
{
	window_t **w = va_arg(ap, window_t **);

	ncurses_window_new(*w);

	return 0;
}

static int ncurses_ui_window_kill(void *data, va_list ap)
{
	window_t **w = va_arg(ap, window_t **);

	ncurses_window_kill(*w);

	return 0;
}

static int ncurses_ui_window_act_changed(void *data, va_list ap)
{
	update_statusbar(1);

	return 0;
}

static int ncurses_ui_window_target_changed(void *data, va_list ap)
{
	window_t **W = va_arg(ap, window_t **), *w = *W;
	ncurses_window_t *n = w->private;
	char *tmp;

	xfree(n->prompt);

	tmp = format_string(format_find((w->target) ? "ncurses_prompt_query" : "ncurses_prompt_none"), w->target);
	n->prompt = tmp; 
	n->prompt_len = xstrlen(tmp);

	update_statusbar(1);

	return 0;
}

static int ncurses_ui_window_refresh(void *data, va_list ap)
{
	ncurses_refresh();
	ncurses_commit();

	return 0;
}

static int ncurses_ui_window_clear(void *data, va_list ap)
{
	window_t **w = va_arg(ap, window_t **);

	ncurses_clear(*w, 0);
	ncurses_commit();

	return 0;
}

static int ncurses_userlist_changed(void *data, va_list ap)
{
	char **p1 = va_arg(ap, char**);
	char **p2 = va_arg(ap, char**);
        window_t *w = NULL;
	list_t l;

        for (l = windows; l; l = l->next) {
       		window_t *w = l->data;
                ncurses_window_t *n = w->private;

                if (!w->target || xstrcasecmp(w->target, *p1))
                	continue;

                xfree(w->target);
                w->target = xstrdup(*p2);
                
		xfree(n->prompt);
                n->prompt = format_string(format_find("ncurses_prompt_query"), w->target);
                n->prompt_len = xstrlen(n->prompt);
        }

	list_destroy(sorted_all_cache, 1);
	sorted_all_cache = NULL;

	ncurses_contacts_update(NULL);
	if ((w = window_find("__contacts")))
		ncurses_redraw(w);
	ncurses_commit();
	return 0;
}

static int ncurses_variable_changed(void *data, va_list ap)
{
	char **__name = va_arg(ap, char**), *name = *__name;

        if (!xstrcasecmp(name, "sort_windows") && config_sort_windows) {
	        list_t l;
                int id = 2;

                for (l = windows; l; l = l->next) {
	                window_t *w = l->data;
                               
			if (w->floating)
                        	continue;

                        if (w->id > 1)
	                        w->id = id++;
                }
        } else if (!xstrcasecmp(name, "timestamp") || !xstrcasecmp(name, "margin_size")) {
       		list_t l;

                for (l = windows; l; l = l->next) {
	                window_t *w = l->data;

	                ncurses_backlog_split(w, 1, 0);
                }

                ncurses_resize();
        }

        ncurses_contacts_update(NULL);
        update_statusbar(1);

	return 0;
}

static int ncurses_conference_renamed(void *data, va_list ap)
{
	char **__oldname = va_arg(ap, char**), **__newname = va_arg(ap, char**);
	char *oldname = *__oldname, *newname = *__newname;
        list_t l;

        for (l = windows; l; l = l->next) {
	        window_t *w = l->data;
                ncurses_window_t *n = w->private;

	        if (w->target && !xstrcasecmp(w->target, oldname)) {
        	        xfree(w->target);
                        xfree(n->prompt);
                        w->target = xstrdup(newname);
                        n->prompt = format_string(format_find("ncurses_prompt_query"), newname);
                        n->prompt_len = xstrlen(n->prompt);
                }
	}

        ncurses_contacts_update(NULL);
        update_statusbar(1);

        return 0;
}

/*
 * changed_aspell()
 *
 * wywo�ywane po zmianie warto�ci zmiennej ,,aspell'' lub ,,aspell_lang'' lub ,,aspell_encoding''.
 */
void ncurses_changed_aspell(const char *var)
{
#ifdef WITH_ASPELL
        /* probujemy zainicjowac jeszcze raz aspell'a */
        ncurses_spellcheck_init();
#endif
}


static int ncurses_binding_query(void *data, va_list ap)
{
        char *p1 = va_arg(ap, char*), *p2 = va_arg(ap, char*), *p3 = va_arg(ap, char*);
        int quiet = va_arg(ap, int);

        if (match_arg(p1, 'a', "add", 2)) {
	        if (!p2 || !p3)
        	        printq("not_enough_params", "bind");
                else
                        ncurses_binding_add(p2, p3, 0, quiet);
        } else if (match_arg(p1, 'd', "delete", 2)) {
        	if (!p2)
                	printq("not_enough_params", "bind");
                else
                        ncurses_binding_delete(p2, quiet);
        } else if (match_arg(p1, 'L', "list-default", 5)) {
        	binding_list(quiet, p2, 1);
        } else {
        	if (match_arg(p1, 'l', "list", 2))
                	binding_list(quiet, p2, 0);
                else
                        binding_list(quiet, p1, 0);
        }

        ncurses_contacts_update(NULL);
        update_statusbar(1);

        return 0;
}

void ncurses_setvar_default() 
{
	config_contacts_size = 9;         /* szeroko�� okna kontakt�w */
	config_contacts = 2;              /* czy ma by� okno kontakt�w */

	xfree(config_contacts_options);
	xfree(config_contacts_groups);

	config_contacts_options = NULL;   /* opcje listy kontakt�w */
	config_contacts_groups = NULL;    /* grupy listy kontakt�w */

	config_backlog_size = 1000;         /* maksymalny rozmiar backloga */
	config_display_transparent = 1;     /* czy chcemy przezroczyste t�o? */
	config_statusbar_size = 1;
	config_header_size = 0;
	config_enter_scrolls = 0;
	config_margin_size = 15;

#ifdef WITH_ASPELL
        xfree(config_aspell_lang);
        xfree(config_aspell_encoding);

        config_aspell_lang = xstrdup("pl");
        config_aspell_encoding = xstrdup("iso8859-2");
#endif
}

/*
 * ncurses_display_transparent_changed ()
 *
 * called when var display_transparent is changed 
 */
void ncurses_display_transparent_changed(const char *var)
{
	int background;

        if (config_display_transparent) {
                background = COLOR_DEFAULT;
                use_default_colors();
        } else {
                background = COLOR_BLACK;
		assume_default_colors(COLOR_WHITE, COLOR_BLACK);
	}
        init_pair(7, COLOR_BLACK, background); 
        init_pair(1, COLOR_RED, background);
        init_pair(2, COLOR_GREEN, background);
        init_pair(3, COLOR_YELLOW, background);
        init_pair(4, COLOR_BLUE, background);
        init_pair(5, COLOR_MAGENTA, background);
        init_pair(6, COLOR_CYAN, background);

        endwin();
        refresh();
        /* it will call what's needed */
	header_statusbar_resize();
        changed_backlog_size("backlog_size");

}

int ncurses_plugin_init()
{
	list_t l;

	plugin_register(&ncurses_plugin);
	
	ncurses_setvar_default();

	query_connect(&ncurses_plugin, "set-vars-default", ncurses_setvar_default, NULL);	
	query_connect(&ncurses_plugin, "ui-beep", ncurses_beep, NULL);
	query_connect(&ncurses_plugin, "ui-is-initialized", ncurses_ui_is_initialized, NULL);
	query_connect(&ncurses_plugin, "ui-window-switch", ncurses_ui_window_switch, NULL);
	query_connect(&ncurses_plugin, "ui-window-print", ncurses_ui_window_print, NULL);
	query_connect(&ncurses_plugin, "ui-window-new", ncurses_ui_window_new, NULL);
	query_connect(&ncurses_plugin, "ui-window-kill", ncurses_ui_window_kill, NULL);
	query_connect(&ncurses_plugin, "ui-window-target-changed", ncurses_ui_window_target_changed, NULL);
	query_connect(&ncurses_plugin, "ui-window-act-changed", ncurses_ui_window_act_changed, NULL);
	query_connect(&ncurses_plugin, "ui-window-refresh", ncurses_ui_window_refresh, NULL);
	query_connect(&ncurses_plugin, "ui-window-clear", ncurses_ui_window_clear, NULL);
	query_connect(&ncurses_plugin, "session-added", ncurses_statusbar_query, NULL);
	query_connect(&ncurses_plugin, "session-removed", ncurses_statusbar_query, NULL);
	query_connect(&ncurses_plugin, "session-changed", ncurses_contacts_changed, NULL);
	query_connect(&ncurses_plugin, "userlist-changed", ncurses_userlist_changed, NULL);
	query_connect(&ncurses_plugin, "userlist-added", ncurses_userlist_changed, NULL);
	query_connect(&ncurses_plugin, "userlist-removed", ncurses_userlist_changed, NULL);
	query_connect(&ncurses_plugin, "binding-command", ncurses_binding_query, NULL);
	query_connect(&ncurses_plugin, "binding-default", ncurses_binding_default, NULL);
	query_connect(&ncurses_plugin, "variable-changed", ncurses_variable_changed, NULL);
	query_connect(&ncurses_plugin, "conference-renamed", ncurses_conference_renamed, NULL);

        query_connect(&ncurses_plugin, "metacontact-added", ncurses_contacts_changed, NULL);
        query_connect(&ncurses_plugin, "metacontact-removed", ncurses_contacts_changed, NULL);
        query_connect(&ncurses_plugin, "metacontact-item-added", ncurses_contacts_changed, NULL);
        query_connect(&ncurses_plugin, "metacontact-item-removed", ncurses_contacts_changed, NULL);

#ifdef WITH_ASPELL
	variable_add(&ncurses_plugin, "aspell", VAR_BOOL, 1, &config_aspell, ncurses_changed_aspell, NULL, NULL);
        variable_add(&ncurses_plugin, "aspell_lang", VAR_STR, 1, &config_aspell_lang, ncurses_changed_aspell, NULL, NULL);
        variable_add(&ncurses_plugin, "aspell_encoding", VAR_STR, 1, &config_aspell_encoding, ncurses_changed_aspell, NULL, NULL);
#endif
	variable_add(&ncurses_plugin, "backlog_size", VAR_INT, 1, &config_backlog_size, changed_backlog_size, NULL, NULL);
	variable_add(&ncurses_plugin, "contacts", VAR_INT, 1, &config_contacts, ncurses_contacts_changed, NULL, NULL);
	variable_add(&ncurses_plugin, "contacts_groups", VAR_STR, 1, &config_contacts_groups, ncurses_contacts_changed, NULL, dd_contacts);
	variable_add(&ncurses_plugin, "contacts_options", VAR_STR, 1, &config_contacts_options, ncurses_contacts_changed, NULL, dd_contacts);
	variable_add(&ncurses_plugin, "contacts_size", VAR_INT, 1, &config_contacts_size, ncurses_contacts_changed, NULL, dd_contacts);
	variable_add(&ncurses_plugin, "display_crap",  VAR_BOOL, 1, &config_display_crap, NULL, NULL, NULL);
	variable_add(&ncurses_plugin, "display_transparent", VAR_BOOL, 1, &config_display_transparent, ncurses_display_transparent_changed, NULL, NULL);
	variable_add(&ncurses_plugin, "enter_scrolls", VAR_BOOL, 1, &config_enter_scrolls, NULL, NULL, NULL);
	variable_add(&ncurses_plugin, "header_size", VAR_INT, 1, &config_header_size, header_statusbar_resize, NULL, NULL);
        variable_add(&ncurses_plugin, "margin_size", VAR_INT, 1, &config_margin_size, NULL, NULL, NULL);
	variable_add(&ncurses_plugin, "statusbar_size", VAR_INT, 1, &config_statusbar_size, header_statusbar_resize, NULL, NULL);
	
	have_winch_pipe = 0;
#ifdef SIGWINCH
	if (pipe(winch_pipe) == 0) {
		have_winch_pipe = 1;
		watch_add(&ncurses_plugin,
				winch_pipe[0],
				WATCH_READ,
				1,			/* persistent */
				ncurses_watch_winch,	/* handler */
				NULL);			/* data */
	}
#endif
	watch_add(&ncurses_plugin, 0, WATCH_READ, 1, ncurses_watch_stdin, NULL);
	timer_add(&ncurses_plugin, "ncurses:clock", 1, 1, ncurses_statusbar_timer, NULL);

	ncurses_screen_width = getenv("COLUMNS") ? atoi(getenv("COLUMNS")) : 80;
	ncurses_screen_height = getenv("LINES") ? atoi(getenv("LINES")) : 24;

	ncurses_init();
	
	header_statusbar_resize("foo");

	for (l = windows; l; l = l->next)
		ncurses_window_new(l->data);

	ncurses_initialized = 1;

	if (!no_mouse)
		ncurses_enable_mouse(); 

	return 0;
}

static int ncurses_plugin_destroy()
{
	ncurses_plugin_destroyed = 1;
	ncurses_initialized = 0;

	ncurses_disable_mouse(); 

	watch_remove(&ncurses_plugin, 0, WATCH_READ);
	if (have_winch_pipe)
		watch_remove(&ncurses_plugin, winch_pipe[0], WATCH_READ);

	timer_remove(&ncurses_plugin, "ncurses:clock");

	if (sorted_all_cache) {
		list_destroy(sorted_all_cache, 1);
		sorted_all_cache = NULL;
	}

	ncurses_deinit();

	plugin_unregister(&ncurses_plugin);

	return 0;
}

