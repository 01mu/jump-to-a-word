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
#include "paste.h"
#include "replace_instant.h"
#include "util.h"

typedef struct {
    gint line;
    GString *spaces_and_tabs;
} LST;

static void line_insert_clear_replace_indicators(ShortcutJump *sj) {
    for (gint i = 0; i < sj->words->len; i++) {
        Word word = g_array_index(sj->words, Word, i);
        scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_TAG, 0);
        scintilla_send_message(sj->sci, SCI_INDICATORCLEARRANGE, sj->first_position + word.replace_pos,
                               sj->replace_len);
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
    if (sj->added_new_line_insert > 0) {
        gint chars_in_doc = scintilla_send_message(sj->sci, SCI_GETLENGTH, 0, 0);

        scintilla_send_message(sj->sci, SCI_DELETERANGE, chars_in_doc - 1, sj->added_new_line_insert);
        sj->added_new_line_insert = 0;
    }
}

static void line_insert_done_common(ShortcutJump *sj) {
    scintilla_send_message(sj->sci, SCI_SETREADONLY, 0, 0);
    line_insert_clear_replace_indicators(sj);
    line_insert_delete_blank_lines(sj);
    line_insert_remove_added_new_lines(sj);
    scintilla_send_message(sj->sci, SCI_ENDUNDOACTION, 0, 0);
    annotation_clear(sj->sci, sj->eol_message_line);
    disconnect_key_press_action(sj);
    disconnect_click_action(sj);
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

void multicursor_line_insert_end(ShortcutJump *sj) {
    for (gint i = 0; i < sj->words->len; i++) {
        Word word = g_array_index(sj->words, Word, i);
        g_string_free(word.word, TRUE);
    }

    g_array_free(sj->words, TRUE);
    g_string_free(sj->cache, TRUE);
    g_string_free(sj->replace_cache, TRUE);

    sj->current_mode = JM_NONE;
    sj->multicursor_mode = MC_DISABLED;
}

void line_insert_cancel(ShortcutJump *sj) {
    ui_set_statusbar(TRUE, _("Line insertion canceled."));

    line_insert_done_common(sj);
    line_insert_end(sj);
}

void line_insert_complete(ShortcutJump *sj) {
    ui_set_statusbar(TRUE, _("Line insertion completed (%i change%s made)."), sj->search_results_count,
                     sj->search_results_count == 1 ? "" : "s");

    line_insert_done_common(sj);
    line_insert_end(sj);
}

void multicursor_line_insert_cancel(ShortcutJump *sj) {
    ui_set_statusbar(TRUE, _("Multicursor line insertion canceled."));

    line_insert_done_common(sj);
    multicursor_line_insert_end(sj);
    multicursor_end(sj);
}

void multicursor_line_insert_complete(ShortcutJump *sj) {
    ui_set_statusbar(TRUE, _("Multicursor line insertion completed (%i change%s made)."), sj->search_results_count,
                     sj->search_results_count == 1 ? "" : "s");

    line_insert_done_common(sj);
    multicursor_line_insert_end(sj);
    multicursor_end(sj);
}

static GArray *line_insert_get_unique_lines(ShortcutJump *sj, GArray *lines, GArray *anchors) {
    gint previous_line = -1;

    for (gint i = 0; i < anchors->len; i++) {
        Word word = g_array_index(anchors, Word, i);

        if (!word.valid_search || !(word.line > previous_line)) {
            continue;
        }

        gint line_number;

        if (sj->config_settings->replace_action == RA_INSERT_PREVIOUS_LINE) {
            line_number = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, word.starting_doc, 0);
        } else {
            line_number = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, word.starting_doc + word.word->len, 0);
        }

        gint line_start_pos = scintilla_send_message(sj->sci, SCI_POSITIONFROMLINE, line_number, 0);
        gint line_end_pos = scintilla_send_message(sj->sci, SCI_GETLINEENDPOSITION, line_number, 0);

        GString *line_content;

        if (line_start_pos < line_end_pos) {
            line_content = g_string_new(sci_get_contents_range(sj->sci, line_start_pos, line_end_pos));
        } else {
            line_content = g_string_new("");
        }

        GString *spaces_and_tabs = g_string_new("");

        gint i = 0;

        while (line_content->str[i] == ' ' || line_content->str[i] == '\t') {
            g_string_append_c(spaces_and_tabs, line_content->str[i]);
            i++;
        }

        g_free(line_content);

        LST lst;

        lst.line = line_number;
        lst.spaces_and_tabs = spaces_and_tabs;

        g_array_append_val(lines, lst);
        previous_line = line_number;
    }

    return lines;
}

static GArray *line_insert_get_dummy_lines(ShortcutJump *sj, GArray *lines, GArray *lines_to_insert) {
    gint lines_added = 0;

    for (gint i = 0; i < lines->len; i++) {
        LST line = g_array_index(lines, LST, i);
        gint dummy_string_pos;

        if (sj->config_settings->replace_action == RA_INSERT_PREVIOUS_LINE) {
            dummy_string_pos = scintilla_send_message(sj->sci, SCI_POSITIONFROMLINE, line.line + lines_added, 0);
        } else {
            dummy_string_pos = scintilla_send_message(sj->sci, SCI_GETLINEENDPOSITION, line.line + lines_added, 0) + 1;
        }

        GString *dummy_string = g_string_new("");

        g_string_append_len(dummy_string, line.spaces_and_tabs->str, line.spaces_and_tabs->len);
        g_string_append_len(dummy_string, "\n", 1);

        scintilla_send_message(sj->sci, SCI_INSERTTEXT, dummy_string_pos, (sptr_t)dummy_string->str);

        Word dummy_word;

        dummy_word.word = g_string_new(dummy_string->str);
        dummy_word.line = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, dummy_string_pos, 0);
        dummy_word.starting = dummy_string_pos + dummy_string->len - 1;
        dummy_word.starting_doc = dummy_string_pos + dummy_string->len - 1;
        dummy_word.valid_search = TRUE;

        if (dummy_word.starting_doc <= sj->multicursor_first_pos) {
            sj->multicursor_first_pos = dummy_word.starting_doc;
        }

        if (dummy_string_pos + dummy_string->len - 1 >= sj->multicursor_last_pos) {
            sj->multicursor_last_pos = dummy_string_pos + dummy_string->len - 1;
        }

        g_array_append_val(lines_to_insert, dummy_word);
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

void line_insert_set_positions(ShortcutJump *sj, GArray *lines_to_insert) {
    gint first_line_on_screen = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, sj->multicursor_first_pos, 0);
    gint last_line_on_screen = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, sj->multicursor_last_pos, 0);
    gint lines_on_screen = last_line_on_screen - first_line_on_screen;

    sj->first_line_on_screen = first_line_on_screen;
    sj->lines_on_screen = lines_on_screen;
    sj->last_line_on_screen = last_line_on_screen;
    sj->first_position = scintilla_send_message(sj->sci, SCI_POSITIONFROMLINE, first_line_on_screen, 0);
    sj->last_position = scintilla_send_message(sj->sci, SCI_GETLINEENDPOSITION, last_line_on_screen, 0);

    sj->replace_query = g_string_new("");

    if (sj->first_position < sj->last_position) {
        gchar *screen_lines = sci_get_contents_range(sj->sci, sj->first_position, sj->last_position);

        g_string_free(sj->cache, TRUE);
        g_string_free(sj->replace_cache, TRUE);

        sj->cache = g_string_new(screen_lines);
        sj->replace_cache = g_string_new(screen_lines);
    }
}

void clear_anchor_annotations(ScintillaObject *sci, GArray *anchors) {
    for (gint i = 0; i < anchors->len; i++) {
        Word word = g_array_index(anchors, Word, i);

        scintilla_send_message(sci, SCI_SETINDICATORCURRENT, INDICATOR_TAG, 0);
        scintilla_send_message(sci, SCI_INDICATORCLEARRANGE, word.starting_doc, word.word->len);
        scintilla_send_message(sci, SCI_SETINDICATORCURRENT, INDICATOR_HIGHLIGHT, 0);
        scintilla_send_message(sci, SCI_INDICATORCLEARRANGE, word.starting_doc, word.word->len);
        scintilla_send_message(sci, SCI_SETINDICATORCURRENT, INDICATOR_TEXT, 0);
        scintilla_send_message(sci, SCI_INDICATORCLEARRANGE, word.starting_doc, word.word->len);
        scintilla_send_message(sci, SCI_SETINDICATORCURRENT, INDICATOR_MULTICURSOR, 0);
        scintilla_send_message(sci, SCI_INDICATORCLEARRANGE, word.starting_doc, word.word->len);
    }
}

void insert_newline_chars(ShortcutJump *sj, GArray *anchors) {
    sj->added_new_line_insert = 0;

    gint chars_in_doc = scintilla_send_message(sj->sci, SCI_GETLENGTH, 0, 0);
    gint last_char_in_doc = scintilla_send_message(sj->sci, SCI_GETCHARAT, chars_in_doc - 1, 0);

    if (sj->last_position == chars_in_doc && last_char_in_doc != '\n') {
        scintilla_send_message(sj->sci, SCI_INSERTTEXT, chars_in_doc, (sptr_t) "\n");
        sj->added_new_line_insert += 1;
    }

    chars_in_doc = scintilla_send_message(sj->sci, SCI_GETLENGTH, 0, 0);

    Word last_word = g_array_index(anchors, Word, anchors->len - 1);
    gint last_word_last_char_pos = last_word.starting_doc + last_word.word->len;

    if (last_word.word->str[last_word.word->len - 1] == '\n' && chars_in_doc == last_word_last_char_pos) {
        scintilla_send_message(sj->sci, SCI_INSERTTEXT, chars_in_doc, (sptr_t) "\n");
        sj->added_new_line_insert += 1;
    }
}

void line_insert_common(ShortcutJump *sj, GArray *unique_lines, GArray *dummy_lines, GArray *anchors) {
    clear_anchor_annotations(sj->sci, anchors);
    g_array_sort(anchors, sort_words_by_starting_doc);
    unique_lines = line_insert_get_unique_lines(sj, unique_lines, anchors);
    insert_newline_chars(sj, anchors);
    dummy_lines = line_insert_get_dummy_lines(sj, unique_lines, dummy_lines);
    line_insert_set_positions(sj, dummy_lines);

    for (gint i = 0; i < unique_lines->len; i++) {
        LST line = g_array_index(unique_lines, LST, i);

        g_free(line.spaces_and_tabs);
    }

    for (gint i = 0; i < dummy_lines->len; i++) {
        Word *word = &g_array_index(dummy_lines, Word, i);

        if (word->valid_search) {
            word->replace_pos = word->starting_doc - sj->first_position;
            sj->search_results_count++;
        }
    }
}

void line_insert_from_multicursor(ShortcutJump *sj) {
    scintilla_send_message(sj->sci, SCI_SETREADONLY, 0, 0);
    scintilla_send_message(sj->sci, SCI_BEGINUNDOACTION, 0, 0);

    GArray *anchors = sj->multicursor_words;
    gint valid_count = 0;

    for (gint i = 0; i < anchors->len; i++) {
        Word word = g_array_index(anchors, Word, i);
        valid_count += word.valid_search ? 1 : 0;
    }

    GArray *dummy_lines = g_array_new(TRUE, FALSE, sizeof(Word));

    sj->words = dummy_lines;

    sj->cache = g_string_new("");
    sj->replace_cache = g_string_new("");

    connect_key_press_action(sj, on_key_press_search_replace);
    connect_click_action(sj, on_click_event_multicursor_line_insert);

    sj->search_results_count = 0;

    if (valid_count == 0) {
        multicursor_line_insert_cancel(sj);
        return;
    }

    GArray *unique_lines = g_array_new(FALSE, FALSE, sizeof(LST));

    line_insert_common(sj, unique_lines, dummy_lines, anchors);
    g_array_free(unique_lines, TRUE);

    gint pos = scintilla_send_message(sj->sci, SCI_GETCURRENTPOS, 0, 0);
    gint line = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, pos, 0);

    sj->multicusor_eol_message_line = line;
    sj->current_cursor_pos = pos;

    sj->current_mode = JM_INSERTING_LINE_MULTICURSOR;
    annotation_display_inserting_line_multicursor(sj);

    paste_get_clipboard_text(sj);
}

void line_insert_from_search(ShortcutJump *sj) {
    scintilla_send_message(sj->sci, SCI_SETREADONLY, 0, 0);
    scintilla_send_message(sj->sci, SCI_BEGINUNDOACTION, 0, 0);

    GArray *anchors = sj->words;

    gint valid_count = 0;

    for (gint i = 0; i < anchors->len; i++) {
        Word word = g_array_index(anchors, Word, i);
        valid_count += word.valid_search ? 1 : 0;
    }

    GArray *dummy_lines = g_array_new(TRUE, FALSE, sizeof(Word));

    sj->searched_words_for_line_insert = sj->words;
    sj->words = dummy_lines;

    if (sj->current_mode == JM_SUBSTRING || sj->current_mode == JM_SEARCH) {
        disconnect_key_press_action(sj);
        disconnect_click_action(sj);
    }

    connect_key_press_action(sj, on_key_press_search_replace);
    connect_click_action(sj, on_click_event_line_insert);

    sj->search_results_count = 0;

    if (valid_count == 0) {
        line_insert_cancel(sj);
        return;
    }

    sj->multicursor_first_pos = scintilla_send_message(sj->sci, SCI_GETLENGTH, 0, 0);
    sj->multicursor_last_pos = 0;

    g_string_free(sj->cache, TRUE);
    g_string_free(sj->replace_cache, TRUE);

    sj->cache = g_string_new("");
    sj->replace_cache = g_string_new("");

    GArray *unique_lines = g_array_new(FALSE, FALSE, sizeof(LST));

    line_insert_common(sj, unique_lines, dummy_lines, anchors);
    g_array_free(unique_lines, TRUE);

    sj->current_mode = JM_INSERTING_LINE;
    annotation_display_inserting_line_from_search(sj);

    paste_get_clipboard_text(sj);
}
