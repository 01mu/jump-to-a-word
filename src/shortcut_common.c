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

#include <math.h>
#include <plugindata.h>

#include "annotation.h"
#include "jump_to_a_word.h"
#include "multicursor.h"
#include "search_substring.h"
#include "search_word.h"
#include "shortcut_char.h"
#include "shortcut_line.h"
#include "shortcut_word.h"
#include "util.h"

/**
 * @brief Sets the first visible line after writing the buffer with shortcuts to the screen. This is needed because the
 * view may be off by one from its previous position after the text is inserted.
 *
 * @param ShortcutJump *sj: The plugin object
 */
void shortcut_set_to_first_visible_line(ShortcutJump *sj) {
    if (!sj->in_selection || sj->selection_is_a_word || sj->selection_is_a_char) {
        scintilla_send_message(sj->sci, SCI_SETFIRSTVISIBLELINE, sj->first_line_on_screen, 0);
    }
}

/**
 * @brief Returns the maximum number of shortcuts that can exist on screen. There can be at most 702 if we are including
 * single char paterns as tags (A to Z and AA to ZZ) and 676 otherwise (AA to ZZ only).
 *
 * @param ShortcutJump *sj: The plugin object
 *
 * @return gint: The number of shortcuts
 */
gint shortcut_get_max_words(ShortcutJump *sj) {
    if (sj->config_settings->shortcuts_include_single_char) {
        return 720;
    } else {
        return 676;
    }
}

/**
 * @brief Masks bad bytes (see shortcut_utf8_char_length).
 *
 * @param GArray *words: The words array
 * @param GString *buffer: The string buffer containing shortcuts above text
 * @param gint first_position: The first position on the screen (used as offset)
 *
 * @return GString *: The masked buffer
 */
GString *shortcut_mask_bytes(GArray *words, GString *buffer, gint first_position) {
    for (gint i = 0; i < words->len; i++) {
        Word word = g_array_index(words, Word, i);
        gint starting = word.starting - first_position;

        for (gint j = 0; j < word.bytes; j++) {
            buffer->str[starting + j + word.padding] = ' ';
        }
    }

    return buffer;
}

/**
 * @brief Set the shortcut tags in place in the buffer string and ignores repeated chars during a shortcut char jump.
 *
 * @param GArray *words: The words array
 * @param GString *buffer: The string buffer containing shortcuts above text
 * @param gint first_position: The first position on the screen (used as offset)
 *
 * @return GString *: The buffer with the shortcut tags added
 */
GString *shortcut_set_tags_in_buffer(GArray *words, GString *buffer, gint first_position) {
    for (gint i = 0; i < words->len; i++) {
        Word word = g_array_index(words, Word, i);
        gint starting = word.starting - first_position;

        if (!word.is_hidden_neighbor) {
            for (gint j = 0; j < word.shortcut->len; j++) {
                buffer->str[starting + j + word.padding] = word.shortcut->str[j];
            }
        }
    }

    return buffer;
}

/**
 * @brief Generates the shortcut tag used by a word when performing a shortcut jump.
 *
 * @param ShortcutJump *sj: The plugin object
 * @param gint position: The index of the word in the words array
 *
 * @return GString *: A pointer to the tag
 */
GString *shortcut_make_tag(ShortcutJump *sj, gint position) {
    if (!sj->config_settings->shortcuts_include_single_char) {
        position += 26;
    }

    if (position < 0) {
        return NULL;
    }

    gint word_length = 1;
    gint temp = position;

    while (temp >= 26) {
        temp = (temp / 26) - 1;
        word_length++;
    }

    GString *result = g_string_new("");

    if (!result) {
        return NULL;
    }

    for (gint i = word_length - 1; i >= 0; i--) {
        gchar ch = (sj->config_settings->shortcut_all_caps ? 'A' : 'a') + (position % 26);
        g_string_prepend_c(result, ch);
        position = (position / 26) - 1;
    }

    return result;
}

/**
 * @brief Frees memory allocated during shortcut search, resets values associated with the search process, and
 * blocks the key press and click signals.
 *
 * @param ShortcutJump *sj: The plugin object
 * @param gboolean was_canceled: Whether the shortcut jump was canceled. This is needed for the perform after action.
 */
void shortcut_end(ShortcutJump *sj, gboolean was_canceled) {
    for (gint i = 0; i < sj->words->len; i++) {
        Word word = g_array_index(sj->words, Word, i);

        g_string_free(word.word, TRUE);

        if (!word.is_hidden_neighbor) {
            g_string_free(word.shortcut, TRUE);
        }
    }

    gboolean performing_line_after = sj->current_mode == JM_LINE ? TRUE : FALSE;

    margin_markers_reset(sj);
    sj->current_mode = JM_NONE;
    sj->search_results_count = 0;
    g_string_free(sj->cache, TRUE);
    g_string_free(sj->buffer, TRUE);
    g_string_free(sj->replace_cache, TRUE);
    g_string_free(sj->search_query, TRUE);
    g_array_free(sj->markers, TRUE);
    g_array_free(sj->words, TRUE);
    g_array_free(sj->lf_positions, TRUE);
    disconnect_key_press_action(sj);
    disconnect_click_action(sj);

    if (sj->multicursor_enabled) {
        scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_MULTICURSOR, 0);

        for (gint i = 0; i < sj->multicursor_words->len; i++) {
            Word word = g_array_index(sj->multicursor_words, Word, i);
            if (word.valid_search) {
                scintilla_send_message(sj->sci, SCI_INDICATORFILLRANGE, word.starting_doc, word.word->len);
            }
        }
    }

    if (performing_line_after && !was_canceled) {
        if (sj->config_settings->line_after == LA_JUMP_TO_CHARACTER_SHORTCUT) {
            shortcut_char_init(sj, FALSE, '0');
        } else if (sj->config_settings->line_after == LA_JUMP_TO_WORD_SHORTCUT) {
            shortcut_word_init(sj);
        } else if (sj->config_settings->line_after == LA_JUMP_TO_WORD_SEARCH) {
            search_init(sj, FALSE);
        } else if (sj->config_settings->line_after == LA_JUMP_TO_SUBSTRING_SEARCH) {
            substring_init(sj, FALSE);
        }
    }
}

/**
 * @brief Completes a shortcut jump by moving the cursor to the word's location, replaces the buffer with the
 * original cached text, enables undo collection, and moves the marker or select the word if those settings are
 * enabled.
 *
 * @param ShortcutJump *sj: The plugin object
 * @param gint pos: The position of the start of the word in the document
 * @param gint word_length: The length of the word
 * @param gint line: The line the word is on (used when moving the marker)
 */
void shortcut_complete(ShortcutJump *sj, gint pos, gint word_length, gint line) {
    if (sj->current_mode == JM_SHORTCUT) {
        ui_set_statusbar(TRUE, _("Word shortcut jump completed."));
    } else if (sj->current_mode == JM_SHORTCUT_CHAR_JUMPING) {
        ui_set_statusbar(TRUE, _("Character shortcut jump completed."));
    }

    scintilla_send_message(sj->sci, SCI_SETREADONLY, 0, 0);
    scintilla_send_message(sj->sci, SCI_DELETERANGE, sj->first_position, sj->buffer->len);
    scintilla_send_message(sj->sci, SCI_INSERTTEXT, sj->first_position, (sptr_t)sj->cache->str);
    scintilla_send_message(sj->sci, SCI_ENDUNDOACTION, 0, 0);
    scintilla_send_message(sj->sci, SCI_UNDO, 0, 0);
    sj->previous_cursor_pos = sj->current_cursor_pos;

    if (sj->config_settings->move_marker_to_line) {
        GeanyDocument *doc = document_get_current();

        if (!doc->is_valid) {
            exit(1);
        } else {
            navqueue_goto_line(doc, doc, line + 1);
        }
    }

    if (sj->current_mode == JM_LINE) {
        shortcut_line_handle_jump_action(sj, line);
    }

    gboolean clear_previous_marker = FALSE;

    if (sj->multicursor_enabled == MC_DISABLED &&
        (sj->current_mode == JM_SHORTCUT || sj->current_mode == JM_SHORTCUT_CHAR_JUMPING)) {
        clear_previous_marker = handle_text_after_action(sj, pos, word_length, line);
    }

    if (sj->multicursor_enabled == MC_ACCEPTING && sj->current_mode != JM_LINE) {
        scintilla_send_message(sj->sci, SCI_GOTOPOS, sj->current_cursor_pos, 0);
    }

    shortcut_set_to_first_visible_line(sj);
    annotation_clear(sj->sci, sj->eol_message_line);
    shortcut_end(sj, FALSE);

    if (clear_previous_marker) {
        gint line = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, sj->range_first_pos, 0);
        scintilla_send_message(sj->sci, SCI_MARKERDELETE, line, -1);
    }
}

/*
 * @brief Inserts the original text, ends the shortcut jump without moving the cursor to a new location, and enables
 * undo collection.
 *
 * @param ScintillaObject *sci: The Scintilla object
 */
void shortcut_word_cancel(ShortcutJump *sj) {
    ui_set_statusbar(TRUE, _("Word shortcut jump canceled."));
    scintilla_send_message(sj->sci, SCI_SETREADONLY, 0, 0);
    scintilla_send_message(sj->sci, SCI_DELETERANGE, sj->first_position, sj->buffer->len);
    scintilla_send_message(sj->sci, SCI_INSERTTEXT, sj->first_position, (sptr_t)sj->cache->str);
    scintilla_send_message(sj->sci, SCI_ENDUNDOACTION, 0, 0);
    scintilla_send_message(sj->sci, SCI_GOTOPOS, sj->current_cursor_pos, 0);
    shortcut_set_to_first_visible_line(sj);
    annotation_clear(sj->sci, sj->eol_message_line);
    sj->range_is_set = FALSE;
    shortcut_end(sj, TRUE);
}

/**
 * @brief Returns the number of marked words in the words array during a shortcut jump.
 *
 * @param ScintillaObject *sci: The Scintilla object
 * @param GArray *words: Array of words
 *
 * @return gint: The number of marked words
 */
gint shortcut_get_search_results_count(ScintillaObject *sci, GArray *words) {
    gint search_results_count = 0;

    for (gint i = 0; i < words->len; i++) {
        Word *word = &g_array_index(words, Word, i);

        if (word->shortcut_marked) {
            search_results_count += 1;
        }
    }

    return search_results_count;
}

/**
 * @brief Returns the index of the shortcut that matches the search query during a shortcut jump.
 *
 * @param ScintillaObject *sci: The Scintilla object
 * @param GArray *words: Array of words
 *
 * @return gint: The index of the valid word
 */
gint shortcut_get_highlighted_pos(ScintillaObject *sci, GArray *words) {
    for (gint i = 0; i < words->len; i++) {
        Word word = g_array_index(words, Word, i);

        if (word.valid_search) {
            return i;
        }
    }

    return -1;
}

/**
 * @brief Resets indicators and info from the previous search, begins a new search, highlights the words whose
 * shortcut pattern partially matches the search query, and marks the word whose pattern fully matches as valid.
 *
 * @param ScintillaObject *sci: The Scintilla object
 * @param GArray *words: Array of words
 * @param GString *search_query: The needle
 *
 * @return GArray *: The pointer to the original array
 */
GArray *shortcut_mark_indicators(ScintillaObject *sci, GArray *words, GString *search_query) {
    for (gint i = 0; i < words->len; i++) {
        Word *word = &g_array_index(words, Word, i);

        word->shortcut_marked = FALSE;
        word->valid_search = FALSE;
        scintilla_send_message(sci, SCI_INDICATORCLEARRANGE, word->starting, 2);
    }

    for (gint i = 0; i < words->len; i++) {
        Word *word = &g_array_index(words, Word, i);

        if (word->is_hidden_neighbor) {
            continue;
        }

        if (g_str_has_prefix(word->shortcut->str, search_query->str) && search_query->len > 0) {
            word->shortcut_marked = TRUE;
        }

        if (g_strcmp0(word->shortcut->str, search_query->str) == 0) {
            word->valid_search = TRUE;
        }
    }

    return words;
}

/**
 * @brief Returns the length of a char in bytes. This is needed because we want to mask extra bytes when writing the
 * shortcut over a char; ￭ is 0xEF 0xBF 0xAD so instead of "A0xBF0xAD" (A is the shorcut) being displayed in the
 * buffer we get "A  ".
 *
 * @param gchar c: The character
 *
 * @return gint: The number of bytes
 */
gint shortcut_utf8_char_length(gchar c) {
    if ((c & 0x80) == 0) {
        return 1;
    }

    if ((c & 0xE0) == 0xC0) {
        return 2;
    }

    if ((c & 0xF0) == 0xE0) {
        return 3;
    }

    if ((c & 0xF8) == 0xF0) {
        return 4;
    }

    return -1;
}

/**
 * @brief Sets the padding needed for centering shortcuts on words if that option is enabled.
 *
 * @param: gint word_length: The length of the word
 *
 * @return gint: The amount of padding to be applied to the start of the word
 */
gint shortcut_set_padding(ShortcutJump *sj, gint word_length) {
    if (sj->config_settings->center_shortcut) {
        return word_length >= 3 ? floor((float)word_length / 2) : 0;
    }

    return 0;
}

/**
 * @brief Sets the indicators for every shortcut.
 *
 * @param ShortcutJump *sj: The plugin object
 *
 *
 */
static void shortcut_set_indicators(ShortcutJump *sj) {
    for (gint i = 0; i < sj->words->len; i++) {
        Word word = g_array_index(sj->words, Word, i);
        gint start = word.starting + word.padding;

        if (word.shortcut_marked) {
            scintilla_send_message(sj->sci, SCI_INDICATORFILLRANGE, start, sj->search_query->len);
        }
    }
};

/**
 * @brief Handles key presses for a shortcut jump: jumps to selected word with Return, removes the last char from
 * the search query with Backspace, and updates indicator higlights.
 *
 * @param GdkEventKey *event: struct containing the key event
 * @param gpointer user_data: The plugin data
 *
 * @return gint: FALSE if no controlled for key press action was found
 */

gint shortcut_on_key_press_action(GdkEventKey *event, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;
    gunichar keychar = gdk_keyval_to_unicode(event->keyval);

    scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_HIGHLIGHT, 0);
    sj->words = shortcut_mark_indicators(sj->sci, sj->words, sj->search_query);
    sj->search_results_count = shortcut_get_search_results_count(sj->sci, sj->words);
    sj->shortcut_single_pos = shortcut_get_highlighted_pos(sj->sci, sj->words);

    if (keychar >= 96 && keychar <= 122 && sj->config_settings->shortcut_all_caps) {
        keychar -= 32;
    }

    if (event->keyval == GDK_KEY_BackSpace) {
        if (sj->search_query->len == 0) {
            shortcut_word_cancel(sj);
            return TRUE;
        }

        g_string_truncate(sj->search_query, sj->search_query->len - 1);
        sj->words = shortcut_mark_indicators(sj->sci, sj->words, sj->search_query);
        shortcut_set_indicators(sj);
        return TRUE;
    }

    if (event->keyval == GDK_KEY_Return) {
        Word word;

        if (sj->shortcut_single_pos == -1 && sj->current_mode != JM_LINE) {
            shortcut_word_cancel(sj);
            return TRUE;
        }

        if (strcmp(sj->search_query->str, "") == 0 && sj->current_mode == JM_LINE) {
            gint current_pos = scintilla_send_message(sj->sci, SCI_GETCURRENTPOS, 0, 0);
            gint current_line = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, current_pos, 0);
            gint lfs = get_lfs(sj, current_line);

            current_line = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, sj->current_cursor_pos + lfs, 0);
            if (current_line - sj->first_line_on_screen >= sj->words->len) {
                shortcut_word_cancel(sj);
                return TRUE;
            }
            word = g_array_index(sj->words, Word, current_line - sj->first_line_on_screen);
        } else {
            if (sj->shortcut_single_pos >= sj->words->len) {
                shortcut_word_cancel(sj);
                return TRUE;
            }
            word = g_array_index(sj->words, Word, sj->shortcut_single_pos);
        }

        if (sj->multicursor_enabled == MC_ACCEPTING && sj->current_mode != JM_LINE) {
            multicursor_add_word(sj, word);
        }

        shortcut_complete(sj, word.starting_doc, word.word->len, word.line);
        return TRUE;
    }

    if (keychar != 0 && g_unichar_isalnum(keychar)) {
        g_string_append_c(sj->search_query, keychar);

        sj->words = shortcut_mark_indicators(sj->sci, sj->words, sj->search_query);
        sj->search_results_count = shortcut_get_search_results_count(sj->sci, sj->words);
        sj->shortcut_single_pos = shortcut_get_highlighted_pos(sj->sci, sj->words);

        if (sj->search_results_count == 0) {
            shortcut_word_cancel(sj);
            return TRUE;
        }

        shortcut_set_indicators(sj);

        if (sj->search_results_count == 1 && !sj->config_settings->wait_for_enter) {
            Word word = g_array_index(sj->words, Word, sj->shortcut_single_pos);

            if (sj->multicursor_enabled == MC_ACCEPTING && sj->current_mode != JM_LINE) {
                multicursor_add_word(sj, word);
            }

            shortcut_complete(sj, word.starting_doc, word.word->len, word.line);
        }

        return TRUE;
    }

    shortcut_word_cancel(sj);
    return FALSE;
}

/**
 * @brief Clears the range on screen and updates it with the text containing shortcuts and sets the cursor to the
 * correct position if line ending characters are displaced.
 *
 * @@param ShortcutJump *sj: The plugin object
 */
void shortcut_set_after_placement(ShortcutJump *sj) {
    gint current_line = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, sj->current_cursor_pos, 0);
    gint lfs_added = get_lfs(sj, current_line);

    shortcut_set_to_first_visible_line(sj);
    scintilla_send_message(sj->sci, SCI_BEGINUNDOACTION, 0, 0);
    scintilla_send_message(sj->sci, SCI_DELETERANGE, sj->first_position, sj->last_position - sj->first_position);
    scintilla_send_message(sj->sci, SCI_INSERTTEXT, sj->first_position, (sptr_t)sj->buffer->str);
    scintilla_send_message(sj->sci, SCI_SETREADONLY, 1, 0);
    scintilla_send_message(sj->sci, SCI_GOTOPOS, sj->current_cursor_pos + lfs_added, 0);
}

/**
 * @brief Handles key press event during shortcut jump.
 *
 * @param GtkWidget *widget: (unused)
 * @param GdkEventKey *event: Keypress event
 * @param gpointer user_data: The plugin data
 *
 * @return gboolean: FALSE if uncontrolled for key press
 */
gboolean shortcut_on_key_press(GtkWidget *widget, GdkEventKey *event, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;

    if (sj->current_mode == JM_SHORTCUT || sj->current_mode == JM_LINE) {
        return shortcut_on_key_press_action(event, sj);
    }

    return FALSE;
}

/**
 * @brief Handles click event for shortcut jump.
 *
 * @param GtkWidget *widget: (unused)
 * @param GdkEventButton *event: Click event
 * @param gpointer user_data: The plugin data
 *
 * @return gboolean: FALSE if uncontrolled for click or wrong mode
 */
gboolean shortcut_on_click_event(GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;

    if (mouse_movement_performed(sj, event) && (sj->current_mode == JM_SHORTCUT || sj->current_mode == JM_LINE)) {
        sj->current_cursor_pos = save_cursor_position(sj);
        sj->current_cursor_pos = set_cursor_position_with_lfs(sj);
        shortcut_word_cancel(sj);
        return TRUE;
    }

    return FALSE;
}
