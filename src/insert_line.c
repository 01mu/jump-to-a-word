/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 */

#include <plugindata.h>

#include "annotation.h"
#include "jump_to_a_word.h"
#include "multicursor.h"
#include "replace_instant.h"
#include "search_common.h"
#include "util.h"
#include "values.h"

static void get_lines(ShortcutJump *sj, GArray *lines) {
    gint previous_line = -1;

    for (gint i = 0; i < sj->multicursor_words->len; i++) {
        Word word = g_array_index(sj->multicursor_words, Word, i);

        if (word.valid_search && word.line > previous_line) {
            gint line;

            if (sj->config_settings->replace_action == RA_INSERT_PREVIOUS_LINE) {
                line = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, word.starting_doc, 0);
            } else {
                line = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, word.starting_doc + word.word->len, 0);
            }

            previous_line = line;
            g_array_append_val(lines, previous_line);
        }
    }
}

static void set_words_from_lines(ShortcutJump *sj, GArray *lines) {
    gint lines_added = 0;

    gint chars_in_doc = scintilla_send_message(sj->sci, SCI_GETLENGTH, 0, 0);
    gint last_char = scintilla_send_message(sj->sci, SCI_GETCHARAT, chars_in_doc - 1, 0);

    if (sj->last_position <= chars_in_doc && last_char != '\n') {
        scintilla_send_message(sj->sci, SCI_INSERTTEXT, chars_in_doc, (sptr_t) "\n");
        sj->newline_was_added_for_next_line_insert = TRUE;
    }

    for (gint i = 0; i < lines->len; i++) {
        gint line = g_array_index(lines, gint, i);
        gint pos;

        if (sj->config_settings->replace_action == RA_INSERT_PREVIOUS_LINE) {
            pos = scintilla_send_message(sj->sci, SCI_POSITIONFROMLINE, line + lines_added, 0);
        } else {
            pos = scintilla_send_message(sj->sci, SCI_GETLINEENDPOSITION, line + lines_added, 0) + 1;
        }

        scintilla_send_message(sj->sci, SCI_INSERTTEXT, pos, (sptr_t) "\n");

        Word multicursor_word;
        multicursor_word.word = g_string_new(sci_get_contents_range(sj->sci, pos, pos + 1));
        multicursor_word.line = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, pos, 0);
        multicursor_word.starting = pos;
        multicursor_word.starting_doc = pos;
        multicursor_word.valid_search = TRUE;

        if (multicursor_word.starting_doc <= sj->multicursor_first_pos) {
            sj->multicursor_first_pos = multicursor_word.starting_doc;
        }

        if (pos + 1 >= sj->multicursor_last_pos) {
            sj->multicursor_last_pos = pos + 1;
        }

        g_array_append_val(sj->multicursor_lines, multicursor_word);
        lines_added++;
    }
}

void multicursor_line_insert(ShortcutJump *sj) {
    gint valid_count = 0;

    for (gint i = 0; i < sj->multicursor_words->len; i++) {
        Word word = g_array_index(sj->multicursor_words, Word, i);
        valid_count += word.valid_search ? 1 : 0;
    }

    if (valid_count == 0) {
        multicursor_end(sj);
        ui_set_statusbar(TRUE, _("No multicursor lines selected."));
        return;
    }

    scintilla_send_message(sj->sci, SCI_SETREADONLY, 0, 0);

    sj->multicursor_lines = g_array_new(TRUE, FALSE, sizeof(Word));
    g_array_sort(sj->multicursor_words, sort_words_by_starting_doc);
    GArray *lines = g_array_new(FALSE, FALSE, sizeof(gint));
    get_lines(sj, lines);
    scintilla_send_message(sj->sci, SCI_BEGINUNDOACTION, 0, 0);
    set_words_from_lines(sj, lines);
    sj->words = sj->multicursor_lines;

    gint first_line_on_screen = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, sj->multicursor_first_pos, 0);
    gint last_line_on_screen = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, sj->multicursor_last_pos, 0);
    gint lines_on_screen = last_line_on_screen - first_line_on_screen;

    sj->first_line_on_screen = first_line_on_screen;
    sj->lines_on_screen = lines_on_screen;
    sj->last_line_on_screen = last_line_on_screen;
    sj->first_position = scintilla_send_message(sj->sci, SCI_POSITIONFROMLINE, first_line_on_screen, 0);
    sj->last_position = scintilla_send_message(sj->sci, SCI_GETLINEENDPOSITION, last_line_on_screen, 0);

    gchar *screen_lines = sci_get_contents_range(sj->sci, sj->first_position, sj->last_position);

    sj->cache = g_string_new(screen_lines);
    sj->buffer = g_string_new(screen_lines);
    sj->replace_cache = g_string_new(screen_lines);

    sj->search_results_count = 0;

    for (gint i = 0; i < sj->words->len; i++) {
        Word *word = &g_array_index(sj->words, Word, i);

        if (word->valid_search) {
            word->replace_pos = word->starting_doc - sj->first_position;
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

    gint pos = scintilla_send_message(sj->sci, SCI_GETCURRENTPOS, 0, 0);
    gint line = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, pos, 0);
    sj->multicusor_eol_message_line = line;
    sj->current_cursor_pos = pos;

    sj->current_mode = JM_MULTICURSOR_REPLACING;
    sj->multicursor_enabled = MC_REPLACING;
    annotation_display_inserting_line_multicursor(sj);
    connect_key_press_action(sj, on_key_press_search_replace);
    connect_click_action(sj, on_click_event_multicursor);
}

static gboolean on_click_event_line_replacement(GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;

    if (mouse_movement_performed(sj, event)) {
        if (sj->current_mode == JM_INSERTING_LINE) {
            sj->current_cursor_pos = save_cursor_position(sj);
            scintilla_send_message(sj->sci, SCI_SETCURRENTPOS, sj->current_cursor_pos, 0);
            search_replace_cancel(sj);
            return TRUE;
        }
    }

    return FALSE;
}

void line_insert_from_search(ShortcutJump *sj) {
    sj->multicursor_words = sj->words;

    g_string_free(sj->cache, TRUE);
    g_string_free(sj->buffer, TRUE);
    g_string_free(sj->replace_cache, TRUE);

    gint valid_count = 0;

    for (gint i = 0; i < sj->multicursor_words->len; i++) {
        Word word = g_array_index(sj->multicursor_words, Word, i);
        valid_count += word.valid_search ? 1 : 0;
    }

    if (valid_count == 0) {
        multicursor_end(sj);
        ui_set_statusbar(TRUE, _("No strings selected."));
        return;
    }

    sj->multicursor_lines = g_array_new(TRUE, FALSE, sizeof(Word));
    g_array_sort(sj->multicursor_words, sort_words_by_starting_doc);
    GArray *lines = g_array_new(FALSE, FALSE, sizeof(gint));
    get_lines(sj, lines);
    scintilla_send_message(sj->sci, SCI_SETREADONLY, 0, 0);
    scintilla_send_message(sj->sci, SCI_BEGINUNDOACTION, 0, 0);
    set_words_from_lines(sj, lines);
    sj->multicursor_words = sj->words;
    sj->words = sj->multicursor_lines;

    gint first_line_on_screen = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, sj->multicursor_first_pos, 0);
    gint last_line_on_screen = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, sj->multicursor_last_pos, 0);
    gint lines_on_screen = last_line_on_screen - first_line_on_screen;

    sj->first_line_on_screen = first_line_on_screen;
    sj->lines_on_screen = lines_on_screen;
    sj->last_line_on_screen = last_line_on_screen;
    sj->first_position = scintilla_send_message(sj->sci, SCI_POSITIONFROMLINE, first_line_on_screen, 0);
    sj->last_position = scintilla_send_message(sj->sci, SCI_GETLINEENDPOSITION, last_line_on_screen, 0);

    gchar *screen_lines = sci_get_contents_range(sj->sci, sj->first_position, sj->last_position);

    sj->cache = g_string_new(screen_lines);
    sj->buffer = g_string_new(screen_lines);
    sj->replace_cache = g_string_new(screen_lines);

    sj->search_results_count = 0;

    for (gint i = 0; i < sj->words->len; i++) {
        Word *word = &g_array_index(sj->words, Word, i);

        if (word->valid_search) {
            word->replace_pos = word->starting_doc - sj->first_position;
            sj->search_results_count++;
        }
    }

    sj->current_mode = JM_INSERTING_LINE;
    connect_key_press_action(sj, on_key_press_search_replace);
    scintilla_send_message(sj->sci, SCI_SETREADONLY, 1, 0);
    connect_click_action(sj, on_click_event_line_replacement);
}
