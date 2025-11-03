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
#include "util.h"

/**
 * @brief Returns whether the char being searched for in the buffer matches the smart case condition whereby a lowercase
 * char matches both a lower and uppercase haystack char and an uppercase one only matches an uppercase chcaracter.
 *
 * @param char haystack_character: The character in the buffer
 * @param char haystack_character: The character in the search query
 *
 * @return gboolean: TRUE if valid
 */
gboolean valid_smart_case(char haystack_char, char needle_char) {
    gboolean g1 = g_unichar_islower(haystack_char) && g_unichar_islower(needle_char) && needle_char == haystack_char;

    gboolean g2 = g_unichar_isupper(haystack_char) && g_unichar_islower(needle_char) &&
                  g_ascii_tolower(haystack_char) == needle_char;

    gboolean g3 = g_unichar_isupper(haystack_char) && g_unichar_isupper(needle_char) && needle_char == haystack_char;

    return g1 || g2 || g3 || haystack_char == needle_char;
}

/**
 * @brief Returns the index of the word or text closest to the cursor when performing a search.
 *
 * @param ShortcutJump *sj: The plugin object
 *
 * @return gint: The index
 */
gint get_search_word_pos(ShortcutJump *sj) {
    gint closest_to_left = 0;
    gint closest_to_right = 0;

    gint closest_to_left_idx = -1;
    gint closest_to_right_idx = -1;

    gint left_len = 0;

    for (gint i = 0; i < sj->words->len; i++) {
        Word word = g_array_index(sj->words, Word, i);

        if (word.valid_search && word.starting >= sj->current_cursor_pos) {
            closest_to_right = word.starting;
            closest_to_right_idx = i;
            break;
        }
    }

    for (gint i = sj->words->len - 1; i >= 0; i--) {
        Word word = g_array_index(sj->words, Word, i);

        if (word.valid_search && word.starting < sj->current_cursor_pos) {
            closest_to_left = word.starting;
            closest_to_left_idx = i;
            left_len = word.word->len;
            break;
        }
    }

    if (abs(sj->current_cursor_pos - closest_to_left - left_len) < abs(sj->current_cursor_pos - closest_to_right)) {
        return closest_to_left_idx;
    } else {
        return closest_to_right_idx;
    }
}

/**
 * @brief Returs the index of the first valid word or text in the array after a search used for search wrapping.
 *
 * @param ShortcutJump *sj: The plugin object
 *
 * @return gint: The first index
 */

gint get_search_word_pos_first(ShortcutJump *sj) {
    for (gint i = 0; i < sj->words->len; i++) {
        Word word = g_array_index(sj->words, Word, i);

        if (word.valid_search) {
            return i;
        }
    }

    return 0;
}

/**
 * @brief Returs the index of the last valid word or text in the array after a search used for search wrapping.
 *
 * @param ShortcutJump *sj: The plugin object
 *
 * @return gint: The last index
 */
gint get_search_word_pos_last(ShortcutJump *sj) {
    for (gint i = sj->words->len - 1; i >= 0; i--) {
        Word word = g_array_index(sj->words, Word, i);

        if (word.valid_search) {
            return i;
        }
    }

    return 0;
}

/**
 * @brief Highlights the currently selected word or text during a search after pressing the right arrow key.
 *
 * @param ShortcutJump *sj: The plugin object
 *
 * @return gboolean: TRUE if there is text to the right, FALSE otherwise (we are beyond the last index)
 */
gboolean set_search_word_pos_right_key(ShortcutJump *sj) {
    for (gint i = 0; i < sj->words->len; i++) {
        Word word = g_array_index(sj->words, Word, i);

        if (sj->search_word_pos == sj->search_word_pos_last && word.valid_search && sj->config_settings->wrap_search) {
            sj->search_word_pos = sj->search_word_pos_first;
            scintilla_send_message(sj->sci, SCI_INDICATORFILLRANGE, word.starting, word.word->len);
            return TRUE;
        }

        if (i > sj->search_word_pos && word.valid_search) {
            sj->search_word_pos = i;
            scintilla_send_message(sj->sci, SCI_INDICATORFILLRANGE, word.starting, word.word->len);
            return TRUE;
        }
    }

    return FALSE;
}

/**
 * @brief Highlights the currently selected word or text during a search after pressing the left arrow key.
 *
 * @param ShortcutJump *sj: The plugin object
 *
 * @return gboolean: TRUE if there is text to the left, FALSE otherwise (we are beyond the first index)
 */
gboolean set_search_word_pos_left_key(ShortcutJump *sj) {
    for (gint i = sj->words->len - 1; i >= 0; i--) {
        Word word = g_array_index(sj->words, Word, i);

        if (sj->search_word_pos == sj->search_word_pos_first && word.valid_search && sj->config_settings->wrap_search) {
            sj->search_word_pos = i;
            scintilla_send_message(sj->sci, SCI_INDICATORFILLRANGE, word.starting, word.word->len);
            return TRUE;
        }

        if (i < sj->search_word_pos && word.valid_search) {
            sj->search_word_pos = i;
            scintilla_send_message(sj->sci, SCI_INDICATORFILLRANGE, word.starting, word.word->len);
            return TRUE;
        }
    }

    return FALSE;
}

/**
 * @brief Frees memory allocated during word search, resets values associated with the search process, clears indicators
 * and annotation messages, blocks the key press and click signals, and resets values used during replacement. If we are
 * ending the search after having replaced a word we input the replacement cache and perform additional operations.
 *
 * @param ShortcutJump *sj: The plugin object
 */
void search_end(ShortcutJump *sj) {
    reset_indicators(sj);

    scintilla_send_message(sj->sci, SCI_INDICSETSTYLE, INDICATOR_TEXT, sj->config_settings->text_color_store_style);
    scintilla_send_message(sj->sci, SCI_INDICSETALPHA, INDICATOR_TEXT, sj->config_settings->text_color_store_outline);
    scintilla_send_message(sj->sci, SCI_INDICSETFORE, INDICATOR_TEXT, sj->config_settings->text_color_store_fore);

    scintilla_send_message(sj->sci, SCI_SETREADONLY, 0, 0);

    search_clear_indicators(sj->sci, sj->words);

    annotation_clear(sj->sci, sj->eol_message_line);

    for (gint i = 0; i < sj->words->len; i++) {
        Word word = g_array_index(sj->words, Word, i);
        g_string_free(word.word, TRUE);
    }

    scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_TAG, 0);

    if (sj->current_mode == JM_REPLACE_SEARCH || sj->current_mode == JM_REPLACE_SUBSTRING) {
        for (gint i = 0; i < sj->words->len; i++) {
            Word word = g_array_index(sj->words, Word, i);

            if (word.valid_search) {
                scintilla_send_message(sj->sci, SCI_INDICATORCLEARRANGE, word.replace_pos, sj->replace_len + 2);
            }
        }

        scintilla_send_message(sj->sci, SCI_DELETERANGE, sj->first_position, sj->replace_cache->len);
        scintilla_send_message(sj->sci, SCI_INSERTTEXT, sj->first_position, (sptr_t)sj->replace_cache->str);
        scintilla_send_message(sj->sci, SCI_ENDUNDOACTION, 0, 0);

        if (!sj->search_change_made) {
            scintilla_send_message(sj->sci, SCI_UNDO, 0, 0);
        }

        margin_markers_reset(sj);
        g_array_free(sj->markers, TRUE);

        sj->cursor_in_word = FALSE;
        sj->replace_len = 0;
        sj->search_change_made = FALSE;

        if (!sj->in_selection) {
            scintilla_send_message(sj->sci, SCI_SETFIRSTVISIBLELINE, sj->first_line_on_screen, 0);
        }

        scintilla_send_message(sj->sci, SCI_GOTOPOS, sj->current_cursor_pos, 0);
    }

    sj->search_word_pos = -1;
    sj->search_word_pos_first = -1;
    sj->search_word_pos_last = -1;
    sj->search_results_count = 0;

    sj->current_mode = JM_NONE;

    annotation_clear(sj->sci, sj->eol_message_line);

    g_string_free(sj->search_query, TRUE);
    g_string_free(sj->eol_message, TRUE);
    g_array_free(sj->words, TRUE);
    g_string_free(sj->replace_cache, TRUE);

    disconnect_key_press_action(sj);
    disconnect_click_action(sj);
}

/**
 * @brief Cancels the word search or substring search replacement.
 *
 * @param ShortcutJump *sj: The plugin object
 */
void search_replace_cancel(ShortcutJump *sj) {
    gchar *type = sj->current_mode == JM_REPLACE_SEARCH ? "Word" : "Substring";
    ui_set_statusbar(TRUE, _("%s replacement canceled."), type);
    search_end(sj);
}

/**
 * @brief Ends the word search or substring search replacement.
 *
 * @param ShortcutJump *sj: The plugin object
 */
void search_replace_complete(ShortcutJump *sj) {
    gchar *type = sj->current_mode == JM_REPLACE_SEARCH ? "Word" : "Substring";
    ui_set_statusbar(TRUE, _("%s replacement completed (%i change%s made)."), type, sj->search_results_count,
                     sj->search_results_count == 1 ? "" : "s");
    search_end(sj);
}

void search_complete(ShortcutJump *sj) {
    ui_set_statusbar(TRUE, _("%s search completed."), sj->current_mode == JM_SEARCH ? "Word" : "Substring");

    if (sj->multicursor_enabled) {
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

    if (sj->multicursor_enabled == MC_DISABLED && (sj->current_mode == JM_SEARCH || sj->current_mode == JM_SUBSTRING)) {
        clear_previous_marker = handle_text_after_action(sj, pos, word_length, line);
    }

    if (sj->multicursor_enabled == MC_ACCEPTING) {
        scintilla_send_message(sj->sci, SCI_GOTOPOS, sj->current_cursor_pos, 0);
    }

    search_end(sj);

    if (clear_previous_marker) {
        gint line = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, sj->range_first_pos, 0);
        scintilla_send_message(sj->sci, SCI_MARKERDELETE, line, -1);
    }
}

/**
 * @brief Ends the word search without moving the cursor to a new location.
 *
 * @param ShortcutJump *sj: The plugin object
 */
void search_cancel(ShortcutJump *sj) {
    ui_set_statusbar(TRUE, _("%s search canceled."), sj->current_mode == JM_SEARCH ? "Word" : "Substring");
    search_end(sj);

    if (sj->range_is_set) {
        gint line = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, sj->range_first_pos, 0);
        scintilla_send_message(sj->sci, SCI_MARKERDELETE, line, -1);
        sj->range_is_set = FALSE;
    }
}

/**
 * @brief Handles key press event for shortcut jump.
 *
 * @param GtkWidget *widget: (unused)
 * @param GdkEventButton *event: Click event
 * @param gpointer user_data: The plugin data
 *
 * @return gboolean: FALSE if uncontrolled for click or wrong mode
 */
gboolean on_click_event_search(GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;

    if (mouse_movement_performed(sj, event)) {
        if (sj->current_mode == JM_SEARCH || sj->current_mode == JM_REPLACE_SEARCH ||
            sj->current_mode == JM_SUBSTRING || sj->current_mode == JM_REPLACE_SUBSTRING) {
            sj->current_cursor_pos = save_cursor_position(sj);
            scintilla_send_message(sj->sci, SCI_SETCURRENTPOS, sj->current_cursor_pos, 0);
            search_cancel(sj);
            return TRUE;
        }
    }

    return FALSE;
}
