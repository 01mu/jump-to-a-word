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
#include "search_substring.h"
#include "search_word.h"
#include "util.h"
#include "values.h"

static void line_insert_clear_replace_indicators(ShortcutJump *sj) {
    for (gint i = 0; i < sj->words->len; i++) {
        gint lines_removed = 0;
        Word word = g_array_index(sj->words, Word, i);
        gint start_pos = scintilla_send_message(sj->sci, SCI_POSITIONFROMLINE, word.line - lines_removed, 0);
        gint clear_len = word.word->len + sj->replace_len;
        if (sj->replace_len == 0) {
            clear_len = word.word->len;
        }
        scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_TAG, 0);
        scintilla_send_message(sj->sci, SCI_INDICATORCLEARRANGE, start_pos, clear_len);
        lines_removed++;
    }
}

static void line_insert_delete_blank_lines(ShortcutJump *sj) {
    if (!sj->search_change_made) {
        gint lines_removed = 0;
        for (gint i = 0; i < sj->words->len; i++) {
            Word word = g_array_index(sj->words, Word, i);
            gint pos = scintilla_send_message(sj->sci, SCI_POSITIONFROMLINE, word.line - lines_removed, 0);
            scintilla_send_message(sj->sci, SCI_DELETERANGE, pos, word.word->len);
            lines_removed++;
        }
    }
}

static void line_insert_remove_added_new_lines(ShortcutJump *sj) {
    gint chars_in_doc = scintilla_send_message(sj->sci, SCI_GETLENGTH, 0, 0);
    scintilla_send_message(sj->sci, SCI_DELETERANGE, chars_in_doc - 1, 1);
}

void line_insert_set_query(ShortcutJump *sj) {
    if (sj->in_selection) {
        sj->in_selection = FALSE;
        init_sj_values(sj);
        search_substring_set_query(sj);
        search_substring_get_substrings(sj);
    } else {
        init_sj_values(sj);
        search_word_get_words(sj);
        search_word_set_query(sj, TRUE);
    }
}

void line_insert_end(ShortcutJump *sj) {
    for (gint i = 0; i < sj->searched_words_for_line_insert->len; i++) {
        Word word = g_array_index(sj->searched_words_for_line_insert, Word, i);
        g_string_free(word.word, TRUE);
    }

    for (gint i = 0; i < sj->words->len; i++) {
        Word word = g_array_index(sj->words, Word, i);
        g_string_free(word.word, TRUE);
    }

    g_string_free(sj->replace_query, TRUE);

    g_array_free(sj->searched_words_for_line_insert, TRUE);
    g_array_free(sj->words, TRUE);
    g_string_free(sj->cache, TRUE);
    g_string_free(sj->buffer, TRUE);
    g_string_free(sj->replace_cache, TRUE);
    sj->current_mode = JM_NONE;
}

void line_insert_cancel(ShortcutJump *sj) {
    ui_set_statusbar(TRUE, _("Line insertion canceled."));
    scintilla_send_message(sj->sci, SCI_SETREADONLY, 0, 0);
    line_insert_clear_replace_indicators(sj);
    line_insert_delete_blank_lines(sj);
    line_insert_remove_added_new_lines(sj);
    scintilla_send_message(sj->sci, SCI_ENDUNDOACTION, 0, 0);
    annotation_clear(sj->sci, sj->eol_message_line);
    disconnect_key_press_action(sj);
    disconnect_click_action(sj);
    line_insert_end(sj);
}

void line_insert_complete(ShortcutJump *sj) {
    ui_set_statusbar(TRUE, _("Line insertion completed (%i change%s made)."), sj->search_results_count,
                     sj->search_results_count == 1 ? "" : "s");
    scintilla_send_message(sj->sci, SCI_SETREADONLY, 0, 0);
    line_insert_clear_replace_indicators(sj);
    line_insert_delete_blank_lines(sj);
    line_insert_remove_added_new_lines(sj);
    scintilla_send_message(sj->sci, SCI_ENDUNDOACTION, 0, 0);
    annotation_clear(sj->sci, sj->eol_message_line);
    disconnect_key_press_action(sj);
    disconnect_click_action(sj);
    line_insert_end(sj);
}

void multicursor_line_insert_end(ShortcutJump *sj) {
    for (gint i = 0; i < sj->words->len; i++) {
        Word word = g_array_index(sj->words, Word, i);
        g_string_free(word.word, TRUE);
    }

    g_array_free(sj->words, TRUE);
    g_string_free(sj->cache, TRUE);
    g_string_free(sj->buffer, TRUE);
    g_string_free(sj->replace_cache, TRUE);
    sj->current_mode = JM_NONE;
    sj->multicursor_mode = MC_DISABLED;
}

void multicursor_line_insert_cancel(ShortcutJump *sj) {
    ui_set_statusbar(TRUE, _("Multicursor line insertion canceled."));
    scintilla_send_message(sj->sci, SCI_SETREADONLY, 0, 0);
    line_insert_clear_replace_indicators(sj);
    line_insert_delete_blank_lines(sj);
    line_insert_remove_added_new_lines(sj);
    scintilla_send_message(sj->sci, SCI_ENDUNDOACTION, 0, 0);
    annotation_clear(sj->sci, sj->eol_message_line);
    disconnect_key_press_action(sj);
    disconnect_click_action(sj);
    multicursor_line_insert_end(sj);
    multicursor_end(sj);
}

void multicursor_line_insert_complete(ShortcutJump *sj) {
    ui_set_statusbar(TRUE, _("Multicursor line insertion completed (%i change%s made)."), sj->search_results_count,
                     sj->search_results_count == 1 ? "" : "s");
    scintilla_send_message(sj->sci, SCI_SETREADONLY, 0, 0);
    line_insert_clear_replace_indicators(sj);
    line_insert_delete_blank_lines(sj);
    line_insert_remove_added_new_lines(sj);
    scintilla_send_message(sj->sci, SCI_ENDUNDOACTION, 0, 0);
    annotation_clear(sj->sci, sj->eol_message_line);
    disconnect_key_press_action(sj);
    disconnect_click_action(sj);
    multicursor_line_insert_end(sj);
    multicursor_end(sj);
}

typedef struct {
    gint line;
    GString *spaces_and_tabs;
} LST;

static GArray *line_insert_get_unique(ShortcutJump *sj, GArray *lines) {
    gint previous_line = -1;

    for (gint i = 0; i < sj->multicursor_words->len; i++) {
        Word word = g_array_index(sj->multicursor_words, Word, i);

        if (word.valid_search && word.line > previous_line) {
            LST lst;

            gint line;

            if (sj->config_settings->replace_action == RA_INSERT_PREVIOUS_LINE) {
                line = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, word.starting_doc, 0);
            } else {
                line = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, word.starting_doc + word.word->len, 0);
            }

            gint start = scintilla_send_message(sj->sci, SCI_POSITIONFROMLINE, line, 0);
            gint end = scintilla_send_message(sj->sci, SCI_GETLINEENDPOSITION, line, 0);
            GString *l = g_string_new(sci_get_contents_range(sj->sci, start, end));
            GString *s = g_string_new("");
            for (gint i = 0; i < l->len; i++) {
                if (l->str[i] == ' ' || l->str[i] == '\t') {
                    g_string_append_c(s, l->str[i]);
                } else {
                    break;
                }
            }
            g_free(l);

            previous_line = line;

            lst.line = previous_line;
            lst.spaces_and_tabs = s;

            g_array_append_val(lines, lst);
        }
    }

    return lines;
}

static GArray *set_words_from_lines(ShortcutJump *sj, GArray *lines, GArray *lines_to_insert) {
    gint lines_added = 0;

    // TODO only insert new line when the last line in the document is in view
    gint chars_in_doc = scintilla_send_message(sj->sci, SCI_GETLENGTH, 0, 0);
    scintilla_send_message(sj->sci, SCI_INSERTTEXT, chars_in_doc, (sptr_t) "\n");

    for (gint i = 0; i < lines->len; i++) {
        LST line = g_array_index(lines, LST, i);
        gint pos;

        if (sj->config_settings->replace_action == RA_INSERT_PREVIOUS_LINE) {
            pos = scintilla_send_message(sj->sci, SCI_POSITIONFROMLINE, line.line + lines_added, 0);
        } else {
            pos = scintilla_send_message(sj->sci, SCI_GETLINEENDPOSITION, line.line + lines_added, 0) + 1;
        }

        GString *insert = g_string_new("");
        g_string_append_len(insert, line.spaces_and_tabs->str, line.spaces_and_tabs->len);
        g_string_append_len(insert, "\n", 1);
        scintilla_send_message(sj->sci, SCI_INSERTTEXT, pos, (sptr_t)insert->str);

        Word multicursor_word;
        multicursor_word.word = g_string_new(insert->str);
        multicursor_word.line = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, pos, 0);
        multicursor_word.starting = pos + insert->len - 1;
        multicursor_word.starting_doc = pos + insert->len - 1;
        multicursor_word.valid_search = TRUE;

        if (multicursor_word.starting_doc <= sj->multicursor_first_pos) {
            sj->multicursor_first_pos = multicursor_word.starting_doc;
        }

        if (pos + insert->len - 1 >= sj->multicursor_last_pos) {
            sj->multicursor_last_pos = pos + insert->len - 1;
        }

        g_array_append_val(lines_to_insert, multicursor_word);
        lines_added++;
    }

    return lines_to_insert;
}

static gboolean on_click_event_multicursor_line_insert(GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;
    if (mouse_movement_performed(sj, event)) {
        sj->current_cursor_pos = scintilla_send_message(sj->sci, SCI_GETCURRENTPOS, 0, 0);
        scintilla_send_message(sj->sci, SCI_SETCURRENTPOS, sj->current_cursor_pos, 0);
        multicursor_line_insert_cancel(sj);
        return TRUE;
    }
    return FALSE;
}

void line_insert_from_multicursor(ShortcutJump *sj) {
    gint valid_count = 0;

    for (gint i = 0; i < sj->multicursor_words->len; i++) {
        Word word = g_array_index(sj->multicursor_words, Word, i);
        valid_count += word.valid_search ? 1 : 0;
    }

    if (valid_count == 0) {
        ui_set_statusbar(TRUE, _("No multicursor strings selected."));
        return;
    }

    for (gint i = 0; i < sj->multicursor_words->len; i++) {
        Word word = g_array_index(sj->multicursor_words, Word, i);
        scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_MULTICURSOR, 0);
        scintilla_send_message(sj->sci, SCI_INDICATORCLEARRANGE, word.starting, word.word->len);
    }

    scintilla_send_message(sj->sci, SCI_SETREADONLY, 0, 0);
    scintilla_send_message(sj->sci, SCI_BEGINUNDOACTION, 0, 0);

    g_array_sort(sj->multicursor_words, sort_words_by_starting_doc);

    GArray *lines = g_array_new(FALSE, FALSE, sizeof(LST));
    GArray *lines_to_insert = g_array_new(TRUE, FALSE, sizeof(Word));

    lines = line_insert_get_unique(sj, lines);
    lines_to_insert = set_words_from_lines(sj, lines, lines_to_insert);

    g_array_free(lines, TRUE);

    sj->words = lines_to_insert;

    gint first_line_on_screen = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, sj->multicursor_first_pos, 0);
    gint last_line_on_screen = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, sj->multicursor_last_pos, 0);
    gint lines_on_screen = last_line_on_screen - first_line_on_screen;

    sj->first_line_on_screen = first_line_on_screen;
    sj->lines_on_screen = lines_on_screen;
    sj->last_line_on_screen = last_line_on_screen;
    sj->first_position = scintilla_send_message(sj->sci, SCI_POSITIONFROMLINE, first_line_on_screen, 0);
    sj->last_position = scintilla_send_message(sj->sci, SCI_GETLINEENDPOSITION, last_line_on_screen, 0);

    gchar *screen_lines = sci_get_contents_range(sj->sci, sj->first_position, sj->last_position);

    sj->replace_query = g_string_new("");

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

    sj->current_mode = JM_INSERTING_LINE_MULTICURSOR;
    annotation_display_inserting_line_multicursor(sj);
    connect_key_press_action(sj, on_key_press_search_replace);
    connect_click_action(sj, on_click_event_multicursor_line_insert);
}

static gboolean on_click_event_line_insert(GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;
    if (mouse_movement_performed(sj, event)) {
        sj->current_cursor_pos = scintilla_send_message(sj->sci, SCI_GETCURRENTPOS, 0, 0);
        scintilla_send_message(sj->sci, SCI_SETCURRENTPOS, sj->current_cursor_pos, 0);
        line_insert_cancel(sj);
        return TRUE;
    }
    return FALSE;
}

void line_insert_from_search(ShortcutJump *sj) {
    sj->multicursor_first_pos = scintilla_send_message(sj->sci, SCI_GETLENGTH, 0, 0);
    sj->multicursor_last_pos = 0;

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
        ui_set_statusbar(TRUE, _("No strings selected."));
        return;
    }

    for (gint i = 0; i < sj->multicursor_words->len; i++) {
        Word word = g_array_index(sj->multicursor_words, Word, i);
        scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_TAG, 0);
        scintilla_send_message(sj->sci, SCI_INDICATORCLEARRANGE, word.starting, word.word->len);
        scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_HIGHLIGHT, 0);
        scintilla_send_message(sj->sci, SCI_INDICATORCLEARRANGE, word.starting, word.word->len);
        scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_TEXT, 0);
        scintilla_send_message(sj->sci, SCI_INDICATORCLEARRANGE, word.starting, word.word->len);
    }

    scintilla_send_message(sj->sci, SCI_SETREADONLY, 0, 0);
    scintilla_send_message(sj->sci, SCI_BEGINUNDOACTION, 0, 0);

    g_array_sort(sj->multicursor_words, sort_words_by_starting_doc);

    GArray *lines = g_array_new(FALSE, FALSE, sizeof(LST));
    GArray *lines_to_insert = g_array_new(TRUE, FALSE, sizeof(Word));

    lines = line_insert_get_unique(sj, lines);
    lines_to_insert = set_words_from_lines(sj, lines, lines_to_insert);

    g_array_free(lines, TRUE);

    sj->searched_words_for_line_insert = sj->words;
    sj->words = lines_to_insert;

    gint first_line_on_screen = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, sj->multicursor_first_pos, 0);
    gint last_line_on_screen = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, sj->multicursor_last_pos, 0);
    gint lines_on_screen = last_line_on_screen - first_line_on_screen;

    sj->first_line_on_screen = first_line_on_screen;
    sj->lines_on_screen = lines_on_screen;
    sj->last_line_on_screen = last_line_on_screen;
    sj->first_position = scintilla_send_message(sj->sci, SCI_POSITIONFROMLINE, first_line_on_screen, 0);
    sj->last_position = scintilla_send_message(sj->sci, SCI_GETLINEENDPOSITION, last_line_on_screen, 0);

    gchar *screen_lines = sci_get_contents_range(sj->sci, sj->first_position, sj->last_position);

    sj->replace_query = g_string_new("");

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
    annotation_display_inserting_line_from_search(sj);
    connect_key_press_action(sj, on_key_press_search_replace);
    connect_click_action(sj, on_click_event_line_insert);
}
