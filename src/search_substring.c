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

#include "action_text_after.h"
#include "annotation.h"
#include "jump_to_a_word.h"
#include "multicursor.h"
#include "paste.h"
#include "search_common.h"
#include "selection.h"
#include "util.h"
#include "values.h"

static void search_substring_clear_replace_indicators(ShortcutJump *sj) {
    for (gint i = 0; i < sj->words->len; i++) {
        Word word = g_array_index(sj->words, Word, i);
        gint start = sj->first_position + word.replace_pos;
        gint len = sj->replace_len;
        scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_TAG, 0);
        scintilla_send_message(sj->sci, SCI_INDICATORCLEARRANGE, start, len);
        scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_HIGHLIGHT, 0);
        scintilla_send_message(sj->sci, SCI_INDICATORCLEARRANGE, start, len);
        scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_TEXT, 0);
        scintilla_send_message(sj->sci, SCI_INDICATORCLEARRANGE, start, len);
    }
}

static void search_substring_clear_jump_indicators(ShortcutJump *sj) {
    for (gint i = 0; i < sj->words->len; i++) {
        Word word = g_array_index(sj->words, Word, i);
        scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_TAG, 0);
        scintilla_send_message(sj->sci, SCI_INDICATORCLEARRANGE, word.starting, word.word->len);
        scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_HIGHLIGHT, 0);
        scintilla_send_message(sj->sci, SCI_INDICATORCLEARRANGE, word.starting, word.word->len);
        scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_TEXT, 0);
        scintilla_send_message(sj->sci, SCI_INDICATORCLEARRANGE, word.starting, word.word->len);
    }
}

void search_substring_end(ShortcutJump *sj) {
    for (gint i = 0; i < sj->words->len; i++) {
        Word word = g_array_index(sj->words, Word, i);
        g_string_free(word.word, TRUE);
    }

    g_string_free(sj->replace_query, TRUE);

    g_string_free(sj->eol_message, TRUE);
    g_string_free(sj->search_query, TRUE);

    sj->search_results_count = 0;
    sj->search_word_pos = -1;
    sj->search_word_pos_first = -1;
    sj->search_word_pos_last = -1;
    sj->search_change_made = FALSE;
    sj->cursor_in_word = FALSE;
    sj->delete_added_bracket = FALSE;
    sj->replace_len = 0;
    sj->replace_instant = FALSE;

    sj->waiting_after_single_instance = FALSE;

    g_free(sj->clipboard_text);

    g_string_free(sj->cache, TRUE);
    g_string_free(sj->buffer, TRUE);
    g_string_free(sj->replace_cache, TRUE);

    g_array_free(sj->lf_positions, TRUE);
    g_array_free(sj->words, TRUE);
    g_array_free(sj->markers, TRUE);

    sj->current_mode = JM_NONE;
}

void search_substring_replace_complete(ShortcutJump *sj) {
    ui_set_statusbar(TRUE, _("Substring replacement completed (%i change%s made)."), sj->search_results_count,
                     sj->search_results_count == 1 ? "" : "s");
    search_substring_clear_replace_indicators(sj);
    search_substring_clear_jump_indicators(sj);
    scintilla_send_message(sj->sci, SCI_ENDUNDOACTION, 0, 0);
    margin_markers_reset(sj);
    scintilla_send_message(sj->sci, SCI_SETREADONLY, 0, 0);
    annotation_clear(sj->sci, sj->eol_message_line);
    disconnect_key_press_action(sj);
    disconnect_click_action(sj);

    if (sj->has_previous_action) {
        g_string_free(sj->previous_search_query, TRUE);
        g_string_free(sj->previous_replace_query, TRUE);
    }

    sj->previous_search_query = g_string_new(sj->search_query->str);
    sj->previous_replace_query = g_string_new(sj->replace_query->str);
    sj->previous_mode = sj->current_mode;
    sj->has_previous_action = TRUE;

    search_substring_end(sj);
}

void search_substring_replace_cancel(ShortcutJump *sj) {
    search_substring_clear_replace_indicators(sj);
    search_substring_clear_jump_indicators(sj);
    scintilla_send_message(sj->sci, SCI_ENDUNDOACTION, 0, 0);
    margin_markers_reset(sj);
    scintilla_send_message(sj->sci, SCI_SETREADONLY, 0, 0);
    annotation_clear(sj->sci, sj->eol_message_line);
    disconnect_key_press_action(sj);
    disconnect_click_action(sj);
    search_substring_end(sj);
    ui_set_statusbar(TRUE, _("Substring replacement canceled."));
}

static void search_substring_jump_complete(ShortcutJump *sj) {
    search_substring_clear_jump_indicators(sj);

    if (sj->multicursor_mode == MC_ACCEPTING) {
        for (gint i = 0; i < sj->words->len; i++) {
            Word word = g_array_index(sj->words, Word, i);
            if (word.valid_search) {
                scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_MULTICURSOR, 0);
                scintilla_send_message(sj->sci, SCI_INDICATORFILLRANGE, word.starting_doc, word.word->len);
                multicursor_add_word(sj, word);
            }
        }
    }

    Word word = g_array_index(sj->words, Word, sj->search_word_pos);
    gint pos = word.starting;
    gint line = word.line;
    gint word_length = word.word->len;

    sj->previous_cursor_pos = sj->current_cursor_pos;
    scintilla_send_message(sj->sci, SCI_GOTOPOS, word.starting, 0);

    if (sj->config_settings->move_marker_to_line) {
        GeanyDocument *doc = document_get_current();

        if (!doc->is_valid) {
            exit(1);
        } else {
            navqueue_goto_line(doc, doc, word.line + 1);
        }
    }

    gboolean clear_previous_marker = FALSE;

    if (sj->multicursor_mode == MC_DISABLED) {
        clear_previous_marker = handle_text_after_action(sj, pos, word_length, line);
    }

    if (sj->multicursor_mode == MC_ACCEPTING) {
        scintilla_send_message(sj->sci, SCI_GOTOPOS, sj->current_cursor_pos, 0);
    }

    margin_markers_reset(sj);

    if (sj->multicursor_mode != MC_ACCEPTING) {
        scintilla_send_message(sj->sci, SCI_SETREADONLY, 0, 0);
    }

    annotation_clear(sj->sci, sj->eol_message_line);
    disconnect_key_press_action(sj);
    disconnect_click_action(sj);
    search_substring_end(sj);

    if (clear_previous_marker) {
        gint line = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, sj->range_first_pos, 0);
        scintilla_send_message(sj->sci, SCI_MARKERDELETE, line, -1);
    }

    ui_set_statusbar(TRUE, _("Substring search completed."));
}

void search_substring_jump_cancel(ShortcutJump *sj) {
    if (sj->waiting_after_single_instance) {
        return;
    }

    search_substring_clear_jump_indicators(sj);
    margin_markers_reset(sj);
    scintilla_send_message(sj->sci, SCI_SETREADONLY, 0, 0);
    annotation_clear(sj->sci, sj->eol_message_line);
    disconnect_key_press_action(sj);
    disconnect_click_action(sj);
    search_substring_end(sj);
    ui_set_statusbar(TRUE, _("Substring search canceled."));
}

void search_substring_set_query(ShortcutJump *sj) {
    g_string_append(sj->search_query, sci_get_contents_range(sj->sci, sj->selection_start, sj->selection_end));
}

static Word search_substring_make_word(ShortcutJump *sj, gint i) {
    Word data;
    gint start = sj->first_position + i;
    gint end = sj->first_position + i + sj->search_query->len;
    data.word = g_string_new(sci_get_contents_range(sj->sci, start, end));
    data.starting = start;
    data.starting_doc = start;
    data.replace_pos = i;
    data.line = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, start, 0);
    data.valid_search = TRUE;
    data.shortcut = NULL;
    data.padding = 0;
    data.bytes = 0;
    data.shortcut_marked = FALSE;
    data.is_hidden_neighbor = FALSE;
    return data;
}

void search_substring_get_substrings(ShortcutJump *sj) {
    if (sj->delete_added_bracket) {
        scintilla_send_message(sj->sci, SCI_DELETERANGE, sj->current_cursor_pos, 1);
        sj->current_cursor_pos = scintilla_send_message(sj->sci, SCI_GETCURRENTPOS, 0, 0);
        scintilla_send_message(sj->sci, SCI_SETCURRENTPOS, sj->current_cursor_pos, 0);
        sj->delete_added_bracket = FALSE;
    }

    for (gint i = 0; i < sj->words->len; i++) {
        Word word = g_array_index(sj->words, Word, i);
        scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_TAG, 0);
        scintilla_send_message(sj->sci, SCI_INDICATORCLEARRANGE, word.starting, word.word->len);
        scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_HIGHLIGHT, 0);
        scintilla_send_message(sj->sci, SCI_INDICATORCLEARRANGE, word.starting, word.word->len);
        scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_TEXT, 0);
        scintilla_send_message(sj->sci, SCI_INDICATORCLEARRANGE, word.starting, word.word->len);
        g_string_free(word.word, TRUE);
    }

    g_array_set_size(sj->words, 0);
    sj->search_results_count = 0;

    if (sj->config_settings->search_case_sensitive && !sj->config_settings->search_smart_case) {
        gchar *b = sj->buffer->str;
        gchar *z = g_strstr_len(b, -1, sj->search_query->str);

        while (z) {
            gint i = z - sj->buffer->str;
            Word data = search_substring_make_word(sj, i);

            g_array_append_val(sj->words, data);
            sj->search_results_count += 1;
            b = z + sj->search_query->len;
            z = g_strstr_len(b, -1, sj->search_query->str);
        }
    } else if (sj->config_settings->search_case_sensitive && sj->config_settings->search_smart_case) {
        gint i = 0;

        for (gchar *p = sj->buffer->str; *p != '\0'; p++) {
            const gchar *z;
            gint k = 0;
            gchar haystack_char;
            gchar needle_char;

            do {
                const gchar *d = p + k;

                z = sj->search_query->str + k;

                haystack_char = d[0];
                needle_char = z[0];

                k++;
            } while (valid_smart_case(haystack_char, needle_char) ||
                     (!g_unichar_isalpha(needle_char) && needle_char == haystack_char));

            if (k - 1 == sj->search_query->len) {
                Word data = search_substring_make_word(sj, i);

                sj->search_results_count += 1;
                g_array_append_val(sj->words, data);
            }

            i++;
        }
    } else if (!sj->config_settings->search_case_sensitive) {
        gchar *buffer_lower = g_ascii_strdown(sj->buffer->str, -1);
        gchar *query_lower = g_ascii_strdown(sj->search_query->str, -1);

        const gchar *b = buffer_lower;

        while ((b = g_strstr_len(b, -1, query_lower))) {
            gint i = b - buffer_lower;
            Word data = search_substring_make_word(sj, i);

            g_array_append_val(sj->words, data);
            sj->search_results_count += 1;
            b += sj->search_query->len;
        }

        g_free(buffer_lower);
        g_free(query_lower);
    }

    for (gint i = 0; i < sj->words->len; i++) {
        Word word = g_array_index(sj->words, Word, i);
        if (word.valid_search) {
            scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_TAG, 0);
            scintilla_send_message(sj->sci, SCI_INDICATORFILLRANGE, word.starting, word.word->len);
            scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_TEXT, 0);
            scintilla_send_message(sj->sci, SCI_INDICATORFILLRANGE, word.starting, word.word->len);
        }
    }

    gint search_word_pos = get_search_word_pos(sj);
    sj->search_word_pos_first = get_search_word_pos_first(sj);
    sj->search_word_pos = search_word_pos == -1 ? sj->search_word_pos_first : search_word_pos;
    if (sj->search_results_count > 0) {
        Word word = g_array_index(sj->words, Word, sj->search_word_pos);
        scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_HIGHLIGHT, 0);
        scintilla_send_message(sj->sci, SCI_INDICATORFILLRANGE, word.starting, word.word->len);
    }
    sj->search_word_pos_last = get_search_word_pos_last(sj);

    ui_set_statusbar(TRUE, _("%i substring%s in view."), sj->search_results_count,
                     sj->search_results_count == 1 ? "" : "s");
}

static gboolean timer_callback(gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;
    search_substring_jump_complete(sj);
    return G_SOURCE_REMOVE;
}

static gboolean on_key_press_search_substring(GtkWidget *widget, GdkEventKey *event, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;
    gunichar keychar = gdk_keyval_to_unicode(event->keyval);

    gboolean is_other_char =
        strchr("[]\\;'.,/-=_+{`_+|}:<>?\"~)(*&^% $#@!)", (gchar)gdk_keyval_to_unicode(event->keyval)) ||
        (event->keyval >= GDK_KEY_0 && event->keyval <= GDK_KEY_9);

    if (sj->waiting_after_single_instance) {
        return FALSE;
    }

    if (event->keyval == GDK_KEY_Return) {
        if (sj->search_word_pos != -1) {
            search_substring_jump_complete(sj);
            return TRUE;
        }
    }

    if (event->keyval == GDK_KEY_BackSpace) {
        if (sj->search_query->len == 0) {
            search_substring_jump_cancel(sj);
            return TRUE;
        }

        g_string_truncate(sj->search_query, sj->search_query->len - 1);

        if (sj->search_query->len > 0) {
            search_substring_get_substrings(sj);
        } else {
            sj->search_results_count = 0;

            for (gint i = 0; i < sj->words->len; i++) {
                Word word = g_array_index(sj->words, Word, i);
                scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_TAG, 0);
                scintilla_send_message(sj->sci, SCI_INDICATORCLEARRANGE, word.starting, 1);
                scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_HIGHLIGHT, 0);
                scintilla_send_message(sj->sci, SCI_INDICATORCLEARRANGE, word.starting, 1);
                scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_TEXT, 0);
                scintilla_send_message(sj->sci, SCI_INDICATORCLEARRANGE, word.starting, 1);
                scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_MULTICURSOR, 0);
                scintilla_send_message(sj->sci, SCI_INDICATORCLEARRANGE, word.starting, 1);
            }
        }

        annotation_display_substring(sj);
        return TRUE;
    }

    if (keychar != 0 && (g_unichar_isalpha(keychar) || is_other_char)) {
        g_string_append_c(sj->search_query, keychar);

        search_substring_get_substrings(sj);
        annotation_display_substring(sj);

        if (sj->search_results_count == 1 && !sj->config_settings->wait_for_enter) {
            if (sj->multicursor_mode == MC_ACCEPTING) {
                search_substring_jump_complete(sj);
            } else {
                Word word = g_array_index(sj->words, Word, sj->search_word_pos);
                scintilla_send_message(sj->sci, SCI_GOTOPOS, word.starting, 0);
                annotation_clear(sj->sci, sj->eol_message_line);
                sj->waiting_after_single_instance = TRUE;
                scintilla_send_message(sj->sci, SCI_SETREADONLY, 1, 0);
                return g_timeout_add(500, timer_callback, sj);
            }
        }

        return TRUE;
    }

    for (gint i = 0; i < sj->words->len; i++) {
        Word word = g_array_index(sj->words, Word, i);

        scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_HIGHLIGHT, 0);
        scintilla_send_message(sj->sci, SCI_INDICATORCLEARRANGE, word.starting, word.word->len);
    }

    if (event->keyval == GDK_KEY_Left && sj->search_query->len > 0) {
        if (set_search_word_pos_left_key(sj)) {
            return TRUE;
        }
    }

    if (event->keyval == GDK_KEY_Right && sj->search_query->len > 0) {
        if (set_search_word_pos_right_key(sj)) {
            return TRUE;
        }
    }

    if (mod_key_pressed(event)) {
        return TRUE;
    }

    search_substring_jump_cancel(sj);
    return FALSE;
}

void serach_substring_init(ShortcutJump *sj) {
    sj->current_mode = JM_SUBSTRING;
    sj->sci = get_scintilla_object();
    set_selection_info(sj);

    define_indicators(sj->sci, sj->config_settings->tag_color, sj->config_settings->highlight_color,
                      sj->config_settings->text_color);

    if (sj->in_selection) {
        if (!sj->selection_is_a_char && !sj->selection_is_a_word && sj->selection_is_within_a_line) {
            sj->in_selection = FALSE;

            init_sj_values(sj);

            search_substring_set_query(sj);
            search_substring_get_substrings(sj);
        } else {
            init_sj_values(sj);
        }
    } else {
        init_sj_values(sj);
    }

    paste_get_clipboard_text(sj);
    connect_key_press_action(sj, on_key_press_search_substring);
    connect_click_action(sj, on_click_event_search);
    annotation_display_substring(sj);
}

void search_substring_cb(GtkMenuItem *menu_item, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;
    if (sj->current_mode == JM_NONE) {
        serach_substring_init(sj);
    }
}

gboolean search_substring_kb(GeanyKeyBinding *kb, guint key_id, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;
    if (sj->current_mode == JM_NONE) {
        serach_substring_init(sj);
        return TRUE;
    }
    return TRUE;
}
