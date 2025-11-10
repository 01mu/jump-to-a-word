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
#include "multicursor.h"
#include "search_common.h"
#include "selection.h"
#include "util.h"
#include "values.h"

void search_substring_end(ShortcutJump *sj) {
    scintilla_send_message(sj->sci, SCI_SETREADONLY, 0, 0);
    annotation_clear(sj->sci, sj->eol_message_line);

    for (gint i = 0; i < sj->words->len; i++) {
        Word word = g_array_index(sj->words, Word, i);
        scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_TAG, 0);
        scintilla_send_message(sj->sci, SCI_INDICATORCLEARRANGE, word.starting, word.word->len);
        scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_HIGHLIGHT, 0);
        scintilla_send_message(sj->sci, SCI_INDICATORCLEARRANGE, word.starting, word.word->len);
        scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_TEXT, 0);
        scintilla_send_message(sj->sci, SCI_INDICATORCLEARRANGE, word.starting, word.word->len);
    }

    for (gint i = 0; i < sj->words->len; i++) {
        Word word = g_array_index(sj->words, Word, i);
        g_string_free(word.word, TRUE);
    }

    if (sj->search_change_made) {
        scintilla_send_message(sj->sci, SCI_DELETERANGE, sj->first_position, sj->replace_cache->len);
        scintilla_send_message(sj->sci, SCI_INSERTTEXT, sj->first_position, (sptr_t)sj->replace_cache->str);
        scintilla_send_message(sj->sci, SCI_ENDUNDOACTION, 0, 0);
    }

    if (sj->newline_was_added_for_next_line_insert) {
        gint chars_in_doc = scintilla_send_message(sj->sci, SCI_GETLENGTH, 0, 0);
        scintilla_send_message(sj->sci, SCI_DELETERANGE, chars_in_doc - 1, 1);
        sj->newline_was_added_for_next_line_insert = FALSE;
    }

    margin_markers_reset(sj);
    g_array_free(sj->markers, TRUE);
    sj->cursor_in_word = FALSE;
    sj->replace_len = 0;
    sj->search_change_made = FALSE;
    scintilla_send_message(sj->sci, SCI_GOTOPOS, sj->current_cursor_pos, 0);

    sj->search_word_pos = -1;
    sj->search_word_pos_first = -1;
    sj->search_word_pos_last = -1;
    sj->search_results_count = 0;
    sj->current_mode = JM_NONE;
    g_string_free(sj->search_query, TRUE);
    g_string_free(sj->eol_message, TRUE);
    g_array_free(sj->words, TRUE);
    g_string_free(sj->replace_cache, TRUE);
    annotation_clear(sj->sci, sj->eol_message_line);
    disconnect_key_press_action(sj);
    disconnect_click_action(sj);
}

void search_substring_replace_complete(ShortcutJump *sj) {
    ui_set_statusbar(TRUE, _("Substring replacement completed (%i change%s made)."), sj->search_results_count,
                     sj->search_results_count == 1 ? "" : "s");
    search_substring_end(sj);
}

void search_substring_replace_cancel(ShortcutJump *sj) {
    ui_set_statusbar(TRUE, _("Substring replacement canceled."));
    search_substring_end(sj);
}

void search_substring_complete(ShortcutJump *sj) {
    ui_set_statusbar(TRUE, _("Substring search completed."));

    if (sj->multicursor_enabled == MC_ACCEPTING) {
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

    if (sj->multicursor_enabled == MC_DISABLED) {
        clear_previous_marker = handle_text_after_action(sj, pos, word_length, line);
    }

    if (sj->multicursor_enabled == MC_ACCEPTING) {
        scintilla_send_message(sj->sci, SCI_GOTOPOS, sj->current_cursor_pos, 0);
    }

    search_substring_end(sj);

    if (clear_previous_marker) {
        gint line = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, sj->range_first_pos, 0);
        scintilla_send_message(sj->sci, SCI_MARKERDELETE, line, -1);
    }
}

void search_substring_cancel(ShortcutJump *sj) {
    ui_set_statusbar(TRUE, _("Substring search canceled."));
    search_substring_end(sj);
    if (sj->range_is_set) {
        gint line = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, sj->range_first_pos, 0);
        scintilla_send_message(sj->sci, SCI_MARKERDELETE, line, -1);
        sj->range_is_set = FALSE;
    }
}

/**
 * @brief Returns the substring searched for during a substring search.
 *
 * @param ShortcutJump *sj: The plugin object
 * @param gint i: The starting position of the substring within the visible text
 *
 * @return Word: The substring
 */
Word get_substring_for_search(ShortcutJump *sj, gint i) {
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

/**
 * @brief Marks every occurace of the substring, clears the words array, and sets indicators.
 *
 * @param ShortcutJump *sj: The plugin object
 */
void search_get_substrings(ShortcutJump *sj) {
    if (sj->delete_added_bracket) {
        scintilla_send_message(sj->sci, SCI_DELETERANGE, sj->current_cursor_pos, 1);
        sj->current_cursor_pos = scintilla_send_message(sj->sci, SCI_GETCURRENTPOS, 0, 0);
        scintilla_send_message(sj->sci, SCI_SETCURRENTPOS, sj->current_cursor_pos, 0);
        sj->delete_added_bracket = FALSE;
    }

    for (gint i = 0; i < sj->words->len; i++) {
        Word word = g_array_index(sj->words, Word, i);

        clear_indicator_for_range(sj->sci, INDICATOR_TAG, word.starting, word.word->len);
        clear_indicator_for_range(sj->sci, INDICATOR_HIGHLIGHT, word.starting, word.word->len);
        clear_indicator_for_range(sj->sci, INDICATOR_TEXT, word.starting, word.word->len);

        g_string_free(word.word, TRUE);
    }

    g_array_set_size(sj->words, 0);
    sj->search_results_count = 0;

    if (sj->config_settings->search_case_sensitive && !sj->config_settings->search_smart_case) {
        gchar *b = sj->buffer->str;
        gchar *z = g_strstr_len(b, -1, sj->search_query->str);

        while (z) {
            gint i = z - sj->buffer->str;
            Word data = get_substring_for_search(sj, i);

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
                Word data = get_substring_for_search(sj, i);

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
            Word data = get_substring_for_search(sj, i);

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

    ui_set_statusbar(TRUE, _("%i substring%s in view."), sj->search_results_count,
                     sj->search_results_count == 1 ? "" : "s");

    gint search_word_pos = get_search_word_pos(sj);

    sj->search_word_pos_first = get_search_word_pos_first(sj);
    sj->search_word_pos = search_word_pos == -1 ? sj->search_word_pos_first : search_word_pos;

    if (sj->search_results_count > 0) {
        Word word = g_array_index(sj->words, Word, sj->search_word_pos);

        set_indicator_for_range(sj->sci, INDICATOR_HIGHLIGHT, word.starting, word.word->len);
    }

    sj->search_word_pos_last = get_search_word_pos_last(sj);
}

/**
 * @brief Handles key press event during a substring jump.
 *
 * @param GtkWidget *widget: (unused)
 * @param GdkEventKey *event: Keypress event
 * @param gpointer user_data: The plugin data
 *
 * @return gboolean: FALSE if uncontrolled for key press
 */
static gboolean on_key_press_search_substring(GtkWidget *widget, GdkEventKey *event, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;
    gunichar keychar = gdk_keyval_to_unicode(event->keyval);

    gboolean is_other_char =
        strchr("[]\\;'.,/-=_+{`_+|}:<>?\"~)(*&^% $#@!)", (gchar)gdk_keyval_to_unicode(event->keyval)) ||
        (event->keyval >= GDK_KEY_0 && event->keyval <= GDK_KEY_9);

    if (event->keyval == GDK_KEY_Return) {
        if (sj->search_word_pos != -1) {
            search_substring_complete(sj);
            return TRUE;
        }
    }

    if (event->keyval == GDK_KEY_BackSpace) {
        if (sj->search_query->len == 0) {
            search_substring_cancel(sj);
            return TRUE;
        }

        g_string_truncate(sj->search_query, sj->search_query->len - 1);

        if (sj->search_query->len > 0) {
            search_get_substrings(sj);
        } else {
            sj->search_results_count = 0;
            search_clear_indicators(sj->sci, sj->words);
        }

        annotation_display_substring(sj);
        return TRUE;
    }

    if (keychar != 0 && (g_unichar_isalpha(keychar) || is_other_char)) {
        g_string_append_c(sj->search_query, keychar);

        search_get_substrings(sj);
        annotation_display_substring(sj);

        if (sj->search_results_count == 1 && !sj->config_settings->wait_for_enter) {
            search_substring_complete(sj);
        }

        return TRUE;
    }

    scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_HIGHLIGHT, 0);

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

    search_substring_cancel(sj);
    return FALSE;
}

/**
 * @brief Sets the initial query if we are in a selection. The selected query will mark every occurance on the page.
 *
 * @param ShortcutJump *sj: The plugin object
 * @param gboolean instant_replace: If we are instantly replacing
 */
GString *set_search_query(ScintillaObject *sci, gint selection_start, gint selection_end, GString *search_query) {
    return g_string_append(search_query, sci_get_contents_range(sci, selection_start, selection_end));
}

/**
 * @brief Begins the substring jump or replacement, defines indicators, and sets key and mouse movements.
 *
 * @param ShortcutJump *sj: The plugin object
 * @param gboolean instant_replace: If we are instantly replacing
 */
void substring_init(ShortcutJump *sj, gboolean instant_replace) {
    if (sj->current_mode != JM_NONE) {
        return;
    }

    sj->current_mode = JM_SUBSTRING;
    set_sj_scintilla_object(sj);

    if (!instant_replace) {
        set_selection_info(sj);
    }

    if (sj->in_selection) {
        if (!sj->selection_is_a_char && !sj->selection_is_a_word && sj->selection_is_a_line) {
            sj->in_selection = FALSE;
            init_sj_values(sj);
            sj->search_query = set_search_query(sj->sci, sj->selection_start, sj->selection_end, sj->search_query);
            search_get_substrings(sj);
        } else {
            init_sj_values(sj);
        }
    } else {
        init_sj_values(sj);
    }

    define_indicators(sj->sci, sj);

    connect_key_press_action(sj, on_key_press_search_substring);
    connect_click_action(sj, on_click_event_search);
    annotation_display_substring(sj);
}

/**
 * @brief Provides a menu callback for performing a substring jump.
 *
 * @param GtkMenuItem *menu_item: (unused)
 * @param gpointer user_data: The plugin data
 */
void substring_cb(GtkMenuItem *menu_item, gpointer user_data) {
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
 * @return gboolean: TRUE
 */
gboolean substring_kb(GeanyKeyBinding *kb, guint key_id, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;

    if (sj->current_mode == JM_NONE) {
        substring_init(sj, FALSE);
        return TRUE;
    }

    return TRUE;
}
