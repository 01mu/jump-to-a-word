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
#include "jump_to_a_word.h"
#include "replace_handle_input.h"
#include "search_common.h"
#include "selection.h"
#include "util.h"
#include "values.h"

static void mark_text(ShortcutJump *sj) {
    for (gint i = 0; i < sj->words->len; i++) {
        Word word = g_array_index(sj->words, Word, i);

        clear_indicator_for_range(sj->sci, INDICATOR_TAG, word.starting, word.word->len);
        clear_indicator_for_range(sj->sci, INDICATOR_HIGHLIGHT, word.starting, word.word->len);
        clear_indicator_for_range(sj->sci, INDICATOR_TEXT, word.starting, word.word->len);

        g_string_free(word.word, TRUE);
    }

    g_array_set_size(sj->words, 0);
    sj->search_results_count = 0;

    if (sj->config_settings->search_case_sensitive) {
        gchar *b = sj->buffer->str;
        gchar *z = g_strstr_len(b, -1, sj->search_query->str);

        while (z) {
            gint i = z - sj->buffer->str;

            Word data;
            gint start = sj->first_position + i;
            gint end = sj->first_position + i + sj->search_query->len;
            data.word = g_string_new(sci_get_contents_range(sj->sci, start, end));
            data.starting = start;
            data.replace_pos = i;
            data.line = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, start, 0);
            data.valid_search = TRUE;
            g_array_append_val(sj->words, data);

            sj->search_results_count += 1;
            b = z + sj->search_query->len;
            z = g_strstr_len(b, -1, sj->search_query->str);
        }
    }

    if (!sj->config_settings->search_case_sensitive) {
        gchar *buffer_lower = g_ascii_strdown(sj->buffer->str, -1);
        gchar *query_lower = g_ascii_strdown(sj->search_query->str, -1);
        const gchar *b = buffer_lower;

        while ((b = g_strstr_len(b, -1, query_lower))) {
            gint i = b - buffer_lower;

            Word data;
            gint start = sj->first_position + i;
            gint end = sj->first_position + i + sj->search_query->len;
            data.word = g_string_new(sci_get_contents_range(sj->sci, start, end));
            data.starting = start;
            data.replace_pos = i;
            data.line = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, start, 0);
            data.valid_search = TRUE;
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
            set_indicator_for_range(sj->sci, INDICATOR_TAG, word.starting, sj->search_query->len);
            set_indicator_for_range(sj->sci, INDICATOR_TEXT, word.starting, sj->search_query->len);
        }
    }

    gint search_word_pos = get_search_word_pos(sj);

    sj->search_word_pos_first = get_search_word_pos_first(sj);
    sj->search_word_pos = search_word_pos == -1 ? sj->search_word_pos_first : search_word_pos;

    if (sj->search_results_count > 0) {
        Word word = g_array_index(sj->words, Word, sj->search_word_pos);

        set_indicator_for_range(sj->sci, INDICATOR_HIGHLIGHT, word.starting, word.word->len);
    }

    sj->search_word_pos_last = get_search_word_pos_last(sj);
}

static gboolean on_key_press_substring(GtkWidget *widget, GdkEventKey *event, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;
    gunichar keychar = gdk_keyval_to_unicode(event->keyval);

    if (sj->delete_added_bracket) {
        scintilla_send_message(sj->sci, SCI_DELETERANGE, sj->current_cursor_pos, 1);
        sj->current_cursor_pos = scintilla_send_message(sj->sci, SCI_GETCURRENTPOS, 0, 0);
        scintilla_send_message(sj->sci, SCI_SETCURRENTPOS, sj->current_cursor_pos, 0);
        sj->delete_added_bracket = FALSE;
    }

    gboolean is_other_char =
        strchr("[]\\;'.,/-=_+{`_+|}:<>?\"~)(*&^% $#@!)", (gchar)gdk_keyval_to_unicode(event->keyval)) ||
        (event->keyval >= GDK_KEY_0 && event->keyval <= GDK_KEY_9);

    if (sj->current_mode == JM_REPLACE_SUBSTRING) {
        return replace_handle_input(sj, event, keychar);
    }

    if (event->keyval == GDK_KEY_Return) {
        if (sj->search_word_pos != -1) {
            search_complete(sj);
            return TRUE;
        }
    }

    if (event->keyval == GDK_KEY_BackSpace) {
        if (sj->search_query->len == 0) {
            search_cancel(sj);
            return TRUE;
        }

        g_string_truncate(sj->search_query, sj->search_query->len - 1);

        if (sj->search_query->len > 0) {
            mark_text(sj);
        } else {
            sj->search_results_count = 0;
            search_clear_indicators(sj->sci, sj->words);
        }

        annotation_display_substring(sj);
        return TRUE;
    }

    if (keychar != 0 && (g_unichar_isalpha(keychar) || is_other_char)) {
        g_string_append_c(sj->search_query, keychar);

        mark_text(sj);
        annotation_display_substring(sj);

        if (sj->search_results_count == 1 && !sj->config_settings->wait_for_enter) {
            search_complete(sj);
        }

        return TRUE;
    }

    scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, 1, 1);

    for (gint i = 0; i < sj->words->len; i++) {
        Word word = g_array_index(sj->words, Word, i);
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

    if (event->keyval == GDK_KEY_Shift_L || event->keyval == GDK_KEY_Shift_R || event->keyval == GDK_KEY_Caps_Lock ||
        event->keyval == GDK_KEY_Control_L) {
        return TRUE;
    }

    search_cancel(sj);
    return FALSE;
}

static void search_set_initial_query(ShortcutJump *sj, gboolean instant_replace) {
    if (instant_replace || sj->in_selection) {
        g_string_append(sj->search_query, sci_get_contents_range(sj->sci, sj->selection_start, sj->selection_end));
        mark_text(sj);
    }
}

void substring_init(ShortcutJump *sj, gboolean instant_replace) {
    if (sj->current_mode != JM_NONE) {
        return;
    }

    sj->current_mode = JM_SUBSTRING;

    set_sj_scintilla_object(sj);

    if (!instant_replace) {
        set_selection_info(sj);
    }

    init_sj_values(sj);
    define_indicators(sj->sci, sj);

    if (sj->in_selection && instant_replace) {
        search_set_initial_query(sj, instant_replace);
    }

    set_key_press_action(sj, on_key_press_substring);
    set_click_action(sj, on_click_event_search);

    annotation_display_substring(sj);
}

/**
 * @brief Provides a menu callback for performing a substring jump.
 *
 * @param GtkMenuItem *menuitem: (unused)
 * @param gpointer user_data: The plugin data
 */
void substring_cb(GtkMenuItem *menuitem, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;

    if (sj->current_mode == JM_NONE) {
        substring_init(sj, FALSE);
    }
}

/**
 * @brief Provides a keybinding callback for performing a substirng jump.
 *
 * @param GtkMenuItem *kb: (unused)
 * @param guint key_id: (unused)
 * @param gpointer user_data: The plugin data
 *
 * @return gboolean: True
 */
gboolean substring_kb(GeanyKeyBinding *kb, guint key_id, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;

    if (sj->current_mode == JM_NONE) {
        substring_init(sj, FALSE);
        return TRUE;
    }

    return TRUE;
}
