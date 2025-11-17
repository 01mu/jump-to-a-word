/*
   Jump to a Word - Move the cursor to a word in Geany
   Copyright (C) 2025 01mu <github.com/01mu>

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include <plugindata.h>

#include "annotation.h"
#include "insert_line.h"
#include "jump_to_a_word.h"
#include "multicursor.h"
#include "replace_handle_input.h"
#include "search_substring.h"
#include "search_word.h"
#include "selection.h"
#include "shortcut_char.h"
#include "util.h"
#include "values.h"

gboolean on_key_press_search_replace(GtkWidget *widget, GdkEventKey *event, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;
    gunichar keychar = gdk_keyval_to_unicode(event->keyval);
    annotation_clear(sj->sci, sj->eol_message_line);
    annotation_clear(sj->sci, sj->multicusor_eol_message_line);

    void (*complete_func)(ShortcutJump *) = NULL;
    void (*cancel_func)(ShortcutJump *) = NULL;

    if (sj->current_mode == JM_REPLACE_MULTICURSOR) {
        complete_func = multicursor_replace_complete;
    } else if (sj->current_mode == JM_REPLACE_SEARCH) {
        complete_func = search_word_replace_complete;
    } else if (sj->current_mode == JM_REPLACE_SUBSTRING) {
        complete_func = search_substring_replace_complete;
    } else if (sj->current_mode == JM_INSERTING_LINE) {
        complete_func = line_insert_complete;
    } else if (sj->current_mode == JM_INSERTING_LINE_MULTICURSOR) {
        complete_func = multicursor_line_insert_complete;
    } else if (sj->current_mode == JM_SHORTCUT_CHAR_REPLACING) {
        complete_func = shortcut_char_replacing_complete;
    }

    if (sj->current_mode == JM_REPLACE_MULTICURSOR) {
        cancel_func = multicursor_replace_cancel;
    } else if (sj->current_mode == JM_REPLACE_SEARCH) {
        cancel_func = search_word_replace_cancel;
    } else if (sj->current_mode == JM_REPLACE_SUBSTRING) {
        cancel_func = search_substring_replace_cancel;
    } else if (sj->current_mode == JM_INSERTING_LINE) {
        cancel_func = line_insert_cancel;
    } else if (sj->current_mode == JM_INSERTING_LINE_MULTICURSOR) {
        cancel_func = multicursor_line_insert_cancel;
    } else if (sj->current_mode == JM_SHORTCUT_CHAR_REPLACING) {
        cancel_func = shortcut_char_replacing_cancel;
    }

    return replace_handle_input(sj, event, keychar, complete_func, cancel_func);
}

void multicursor_replace(ShortcutJump *sj) {
    gint valid_count = 0;

    for (gint i = 0; i < sj->multicursor_words->len; i++) {
        Word word = g_array_index(sj->multicursor_words, Word, i);
        valid_count += word.valid_search ? 1 : 0;
    }

    if (valid_count == 0) {
        ui_set_statusbar(TRUE, _("No multicursor strings to replace."));
        return;
    }

    scintilla_send_message(sj->sci, SCI_BEGINUNDOACTION, 0, 0);

    gint first_line_on_screen = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, sj->multicursor_first_pos, 0);
    gint last_line_on_screen = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, sj->multicursor_last_pos, 0);
    gint lines_on_screen = last_line_on_screen - first_line_on_screen;

    sj->first_line_on_screen = first_line_on_screen;
    sj->lines_on_screen = lines_on_screen;
    sj->last_line_on_screen = last_line_on_screen;
    sj->first_position = scintilla_send_message(sj->sci, SCI_POSITIONFROMLINE, first_line_on_screen, 0);
    sj->last_position = scintilla_send_message(sj->sci, SCI_GETLINEENDPOSITION, last_line_on_screen, 0);

    gchar *screen_lines = sci_get_contents_range(sj->sci, sj->first_position, sj->last_position);

    g_array_sort(sj->multicursor_words, sort_words_by_starting_doc);
    sj->words = sj->multicursor_words;

    sj->search_results_count = 0;

    for (gint i = 0; i < sj->words->len; i++) {
        Word *word = &g_array_index(sj->words, Word, i);

        if (word->valid_search) {
            word->replace_pos = word->starting_doc - sj->first_position;
            scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_TAG, 0);
            scintilla_send_message(sj->sci, SCI_INDICATORFILLRANGE, word->starting_doc, word->word->len);
            scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_TEXT, 0);
            scintilla_send_message(sj->sci, SCI_INDICATORFILLRANGE, word->starting_doc, word->word->len);

            if (sj->config_settings->replace_action == RA_INSERT_END) {
                word->replace_pos += word->word->len;
            }

            sj->search_results_count++;
        }
    }

    sj->search_word_pos = -1;
    sj->search_word_pos_first = -1;
    sj->search_word_pos_last = -1;
    sj->search_change_made = FALSE;
    sj->cursor_in_word = FALSE;
    sj->delete_added_bracket = FALSE;
    sj->replace_len = 0;
    sj->replace_instant = FALSE;

    sj->replace_query = g_string_new("");

    sj->cache = g_string_new(screen_lines);
    sj->buffer = g_string_new(screen_lines);
    sj->replace_cache = g_string_new(screen_lines);

    gint pos = scintilla_send_message(sj->sci, SCI_GETCURRENTPOS, 0, 0);
    gint line = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, pos, 0);
    sj->multicusor_eol_message_line = line;
    sj->current_cursor_pos = pos;

    sj->current_mode = JM_REPLACE_MULTICURSOR;
    annotation_display_replace_multicursor(sj);
    connect_key_press_action(sj, on_key_press_search_replace);
    connect_click_action(sj, on_click_event_multicursor_replace);
}

static void replace_shortcut_char_init(ShortcutJump *sj) {
    if (sj->words->len == 0) {
        ui_set_statusbar(TRUE, _("No characters to replace."));
        shortcut_char_replacing_cancel(sj);
        return;
    }

    scintilla_send_message(sj->sci, SCI_BEGINUNDOACTION, 0, 0);
    scintilla_send_message(sj->sci, SCI_GOTOPOS, sj->current_cursor_pos, 0);
    annotation_display_replace_char(sj);
    sj->current_mode = JM_SHORTCUT_CHAR_REPLACING;

    for (gint i = 0; i < sj->words->len; i++) {
        Word word = g_array_index(sj->words, Word, i);
        if (!word.valid_search) {
            continue;
        }
        scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_TAG, 0);
        scintilla_send_message(sj->sci, SCI_INDICATORFILLRANGE, word.starting_doc, 1);
        scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_TEXT, 0);
        scintilla_send_message(sj->sci, SCI_INDICATORFILLRANGE, word.starting_doc, 1);
    }
}

void replace_substring_init(ShortcutJump *sj) {
    if (sj->search_results_count == 0) {
        ui_set_statusbar(TRUE, _("No substrings to replace."));
        search_substring_jump_cancel(sj);
        return;
    }

    scintilla_send_message(sj->sci, SCI_BEGINUNDOACTION, 0, 0);
    scintilla_send_message(sj->sci, SCI_GOTOPOS, sj->current_cursor_pos, 0);
    annotation_display_replace_substring(sj);
    sj->current_mode = JM_REPLACE_SUBSTRING;

    if (sj->config_settings->replace_action == RA_INSERT_END) {
        for (gint i = 0; i < sj->words->len; i++) {
            Word *word = &g_array_index(sj->words, Word, i);
            if (!word->valid_search) {
                continue;
            }
            word->replace_pos += sj->search_query->len;
        }
    }

    disconnect_key_press_action(sj);
    connect_key_press_action(sj, on_key_press_search_replace);
}

void replace_word_init(ShortcutJump *sj) {
    if (sj->search_results_count == 0) {
        ui_set_statusbar(TRUE, _("No words to replace."));
        search_word_jump_cancel(sj);
        return;
    }

    scintilla_send_message(sj->sci, SCI_BEGINUNDOACTION, 0, 0);
    scintilla_send_message(sj->sci, SCI_GOTOPOS, sj->current_cursor_pos, 0);
    annotation_display_replace(sj);
    sj->current_mode = JM_REPLACE_SEARCH;

    if (sj->config_settings->replace_action == RA_INSERT_END) {
        for (gint i = 0; i < sj->words->len; i++) {
            Word *word = &g_array_index(sj->words, Word, i);
            if (word->valid_search) {
                word->replace_pos += word->word->len;
            }
        }
    }

    disconnect_key_press_action(sj);
    connect_key_press_action(sj, on_key_press_search_replace);
}

void replace_instant_init(ShortcutJump *sj) {
    set_sj_scintilla_object(sj);
    set_selection_info(sj);
    sj->current_cursor_pos = scintilla_send_message(sj->sci, SCI_GETCURRENTPOS, 0, 0);
    sj->replace_instant = TRUE;
    define_indicators(sj->sci, sj);

    if (sj->in_selection) {
        if (sj->selection_is_a_char) {
            gchar query = scintilla_send_message(sj->sci, SCI_GETCHARAT, sj->selection_start, 0);
            shortcut_char_init_with_query(sj, query);
            replace_shortcut_char_init(sj);
        } else {
            serach_substring_init(sj);
            replace_substring_init(sj);
        }
    } else {
        search_word_init(sj, TRUE);
        replace_word_init(sj);
    }
}
