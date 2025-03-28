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

#include "Scintilla.h"
#include "annotation.h"
#include "jump_to_a_word.h"
#include "line_options.h"
#include "replace_handle_input.h"
#include "search_word.h"
#include "selection.h"
#include "shortcut_char.h"
#include "shortcut_word.h"
#include "util.h"

gint get_max_words(ShortcutJump *sj) { return sj->config_settings->shortcuts_include_single_char ? 702 : 676; }

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
 * @brief Set the shortcut tags in place in the buffer string.
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
    search_clear_indicators(sj->sci, sj->words);

    if (sj->current_mode == JM_SHORTCUT_CHAR_REPLACING) {
        for (gint i = 0; i < sj->words->len; i++) {
            Word word = g_array_index(sj->words, Word, i);

            scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, 0, 0);

            if (word.valid_search) {
                scintilla_send_message(sj->sci, SCI_INDICATORCLEARRANGE, word.replace_pos + sj->first_position,
                                       sj->replace_len + 2);
            }
        }

        scintilla_send_message(sj->sci, SCI_SETREADONLY, 0, 0);
        scintilla_send_message(sj->sci, SCI_DELETERANGE, sj->first_position, sj->replace_cache->len);
        scintilla_send_message(sj->sci, SCI_INSERTTEXT, sj->first_position, (sptr_t)sj->replace_cache->str);
        scintilla_send_message(sj->sci, SCI_ENDUNDOACTION, 0, 0);

        g_string_free(sj->replace_cache, TRUE);

        if (!sj->search_change_made) {
            scintilla_send_message(sj->sci, SCI_UNDO, 0, 0);
        }

        sj->cursor_in_word = FALSE;
        sj->replace_len = 0;
        sj->search_change_made = FALSE;

        scintilla_send_message(sj->sci, SCI_GOTOPOS, sj->current_cursor_pos, 0);
    }

    if (!sj->in_selection) {
        scintilla_send_message(sj->sci, SCI_SETFIRSTVISIBLELINE, sj->first_line_on_screen, 0);
    }

    for (gint i = 0; i < sj->words->len; i++) {
        Word word = g_array_index(sj->words, Word, i);

        g_string_free(word.word, TRUE);

        if (sj->current_mode == JM_SHORTCUT_CHAR_REPLACING || sj->current_mode == JM_SHORTCUT_CHAR_JUMPING) {
            if (!word.is_hidden_neighbor) {
                g_string_free(word.shortcut, TRUE);
            }
        } else {
            g_string_free(word.shortcut, TRUE);
        }
    }

    margin_markers_reset(sj);

    gboolean performing_line_after = sj->current_mode == JM_LINE ? TRUE : FALSE;

    sj->current_mode = JM_NONE;

    sj->search_results_count = 0;

    g_string_free(sj->cache, TRUE);
    g_string_free(sj->buffer, TRUE);
    g_string_free(sj->search_query, TRUE);

    g_array_free(sj->markers, TRUE);
    g_array_free(sj->words, TRUE);
    g_array_free(sj->lf_positions, TRUE);

    sj->line_range_set = FALSE;

    block_key_press_action(sj);
    block_click_action(sj);

    if (performing_line_after && !was_canceled) {
        if (sj->config_settings->line_after == LA_JUMP_TO_CHARACTER_SHORTCUT) {
            shortcut_char_init(sj);
        } else if (sj->config_settings->line_after == LA_JUMP_TO_WORD_SHORTCUT) {
            shortcut_word_init(sj);
        } else if (sj->config_settings->line_after == LA_JUMP_TO_WORD_SEARCH) {
            search_init(sj, FALSE);
        }
    }
}

/**
 * @brief Completes a shortcut jump by moving the cursor to the word's location, replaces the buffer with the
 * original cached text, enables undo collection, and moves the marker or select the word if those settings are
 * enabled.
 *
 * @param ScintillaObject *sci: The Scintilla object
 * @param gint pos: The position of the start of the word in the document
 * @param gint word_length: The length of the word
 * @param gint line: The line the word is on (used when moving the marker)
 */
void shortcut_complete(ShortcutJump *sj, gint pos, gint word_length, gint line) {
    for (gint i = 0; i < sj->words->len; i++) {
        Word *word = &g_array_index(sj->words, Word, i);

        scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, 1, 1);
        scintilla_send_message(sj->sci, SCI_INDICATORCLEARRANGE, word->starting, word->shortcut->len);
    }

    if (sj->current_mode == JM_LINE && sj->config_settings->line_after == LA_SELECT_LINE_RANGE && !sj->line_range_set) {
        scintilla_send_message(sj->sci, SCI_MARKERDEFINE, 0, SC_MARK_SHORTARROW);
        scintilla_send_message(sj->sci, SCI_MARKERADD, line, 0);
        sj->line_range_first = line;
        g_string_erase(sj->search_query, 0, sj->search_query->len);
        sj->line_range_set = TRUE;
        return;
    }

    if ((sj->current_mode == JM_SHORTCUT || sj->current_mode == JM_SHORTCUT_CHAR_JUMPING) &&
        sj->config_settings->text_after == TX_SELECT_TEXT_RANGE && !sj->line_range_set) {
        scintilla_send_message(sj->sci, SCI_MARKERDEFINE, 0, SC_MARK_SHORTARROW);
        scintilla_send_message(sj->sci, SCI_MARKERADD, line, 0);
        sj->line_range_first = pos;
        sj->text_range_word_length = word_length;
        g_string_erase(sj->search_query, 0, sj->search_query->len);
        sj->line_range_set = TRUE;
        return;
    }

    scintilla_send_message(sj->sci, SCI_SETREADONLY, 0, 0);
    scintilla_send_message(sj->sci, SCI_DELETERANGE, sj->first_position, sj->buffer->len);
    scintilla_send_message(sj->sci, SCI_INSERTTEXT, sj->first_position, (sptr_t)sj->cache->str);

    scintilla_send_message(sj->sci, SCI_ENDUNDOACTION, 0, 0);
    scintilla_send_message(sj->sci, SCI_UNDO, 0, 0);
    scintilla_send_message(sj->sci, SCI_GOTOPOS, pos, 0);

    sj->previous_cursor_pos = sj->current_cursor_pos;

    if (sj->config_settings->move_marker_to_line) {
        GeanyDocument *doc = document_get_current();

        if (!doc->is_valid) {
            exit(1);
        } else {
            navqueue_goto_line(doc, doc, line + 1);
        }
    }

    if (sj->current_mode == JM_SHORTCUT || sj->current_mode == JM_SHORTCUT_CHAR_JUMPING) {
        if (sj->config_settings->text_after == TX_SELECT_TEXT) {
            scintilla_send_message(sj->sci, SCI_SETSEL, pos, pos + word_length);
        }

        if (sj->config_settings->text_after == TX_SELECT_TO_TEXT) {
            if (sj->current_cursor_pos > pos) {
                scintilla_send_message(sj->sci, SCI_SETSEL, sj->current_cursor_pos, pos);
            } else {
                scintilla_send_message(sj->sci, SCI_SETSEL, sj->current_cursor_pos, pos + word_length);
            }
        }

        if (sj->config_settings->text_after == TX_SELECT_TEXT_RANGE && sj->line_range_set) {
            if (pos > sj->line_range_first) {
                scintilla_send_message(sj->sci, SCI_SETSEL, sj->line_range_first, pos + word_length);
            } else {
                scintilla_send_message(sj->sci, SCI_SETSEL, pos, sj->line_range_first + sj->text_range_word_length);
            }
        }
    }

    if (sj->current_mode == JM_LINE) {
        if (sj->config_settings->line_after == LA_SELECT_LINE) {
            gint pos = scintilla_send_message(sj->sci, SCI_POSITIONFROMLINE, line, TRUE);
            gint line_length = scintilla_send_message(sj->sci, SCI_LINELENGTH, line, TRUE);

            scintilla_send_message(sj->sci, SCI_SETSEL, pos, pos + line_length);
        }

        if (sj->config_settings->line_after == LA_SELECT_TO_LINE) {
            gint current_line = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, sj->current_cursor_pos, 0);

            gint first_line = current_line < line ? current_line : line;
            gint second_line = (current_line >= line ? current_line : line);
            gint pos_first = scintilla_send_message(sj->sci, SCI_POSITIONFROMLINE, first_line, TRUE);
            gint pos_second = scintilla_send_message(sj->sci, SCI_POSITIONFROMLINE, second_line, TRUE);
            gint second_line_length = scintilla_send_message(sj->sci, SCI_LINELENGTH, second_line, TRUE);

            scintilla_send_message(sj->sci, SCI_SETSEL, pos_first, pos_second + second_line_length);
        }

        if (sj->config_settings->line_after == LA_SELECT_LINE_RANGE && sj->line_range_set) {
            gint first_line = sj->line_range_first < line ? sj->line_range_first : line;
            gint second_line = (sj->line_range_first >= line ? sj->line_range_first : line);
            gint pos_first = scintilla_send_message(sj->sci, SCI_POSITIONFROMLINE, first_line, TRUE);
            gint pos_second = scintilla_send_message(sj->sci, SCI_POSITIONFROMLINE, second_line, TRUE);
            gint second_line_length = scintilla_send_message(sj->sci, SCI_LINELENGTH, second_line, TRUE);

            scintilla_send_message(sj->sci, SCI_SETSEL, pos_first, pos_second + second_line_length);
        }
    }

    // scintilla_send_message(sj->sci, SCI_GOTOPOS, sj->current_cursor_pos, 0);

    annotation_clear(sj->sci, sj->eol_message_line);
    shortcut_end(sj, FALSE);
    ui_set_statusbar(TRUE, _("Jump completed"));
}

/**
 * @brief Inserts the original text, ends the shortcut jump without moving the cursor to a new location, and enables
 * undo collection.
 *
 * @param ScintillaObject *sci: The Scintilla object
 */
void shortcut_cancel(ShortcutJump *sj) {
    scintilla_send_message(sj->sci, SCI_SETREADONLY, 0, 0);
    scintilla_send_message(sj->sci, SCI_DELETERANGE, sj->first_position, sj->buffer->len);
    scintilla_send_message(sj->sci, SCI_INSERTTEXT, sj->first_position, (sptr_t)sj->cache->str);
    scintilla_send_message(sj->sci, SCI_ENDUNDOACTION, 0, 0);
    scintilla_send_message(sj->sci, SCI_UNDO, 0, 0);

    scintilla_send_message(sj->sci, SCI_GOTOPOS, sj->current_cursor_pos, 0);

    annotation_clear(sj->sci, sj->eol_message_line);
    shortcut_end(sj, TRUE);
    ui_set_statusbar(TRUE, _("Jump canceled"));
}

/**
 * @brief Display message for completed shortcut jump and free memory.
 *
 * @param ShortcutJump *sj: The plugin object
 */
void shortcut_replace_complete(ShortcutJump *sj) {
    shortcut_end(sj, FALSE);
    ui_set_statusbar(TRUE, _("Shortcut replacement completed"));
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
GArray *shortcut_mark_words(ScintillaObject *sci, GArray *words, GString *search_query) {
    scintilla_send_message(sci, SCI_SETINDICATORCURRENT, 1, 1);

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
 * @brief Sets word padding for shortcuts if that option is enabled.
 *
 * @param: gint word_length: The length of the word
 *
 * @return gint: The amount of padding to be applied to the start of the word
 */
gint shortcut_set_word_padding(ShortcutJump *sj, gint word_length) {
    if (sj->config_settings->center_shortcut) {
        return word_length >= 3 ? floor((float)word_length / 2) : 0;
    }

    return 0;
}

/**
 * @brief Handles key presses for a shortcut jump: jumps to selected word with Return, removes the last char from
 * the search query with Backspace, and updates indicator higlights.
 *
 * @param GdkEventKey *event: struct containing the key event
 * @param gpointer user_data: (unused)
 *
 * @return gint: FALSE if no controlled for key press action was found
 */
static gint shortcut_on_key_press(GdkEventKey *event, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;

    gunichar keychar = gdk_keyval_to_unicode(event->keyval);

    scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, 1, 1);

    if (keychar >= 96 && keychar <= 122) {
        keychar -= (sj->config_settings->shortcut_all_caps ? 32 : 0);
    }

    if (event->keyval == GDK_KEY_BackSpace) {
        if (sj->search_query->len == 0) {
            shortcut_cancel(sj);
            return TRUE;
        }

        g_string_truncate(sj->search_query, sj->search_query->len - 1);

        if (sj->search_query->len >= 0) {
            sj->words = shortcut_mark_words(sj->sci, sj->words, sj->search_query);
            sj->search_results_count = shortcut_get_search_results_count(sj->sci, sj->words);
            sj->shortcut_single_pos = shortcut_get_highlighted_pos(sj->sci, sj->words);

            for (gint i = 0; i < sj->words->len; i++) {
                Word word = g_array_index(sj->words, Word, i);

                if (word.shortcut_marked && sj->config_settings->wait_for_enter) {
                    scintilla_send_message(sj->sci, SCI_INDICATORFILLRANGE, word.starting + word.padding,
                                           sj->search_query->len);
                }
            }
        }

        return TRUE;
    }

    if (event->keyval == GDK_KEY_Return) {
        Word word = g_array_index(sj->words, Word, sj->shortcut_single_pos);

        if (sj->shortcut_single_pos == -1) {
            shortcut_cancel(sj);
        } else {
            shortcut_complete(sj, word.starting_doc, word.word->len, word.line);
        }

        return TRUE;
    }

    if (keychar != 0 && g_unichar_isalnum(keychar)) {
        if (sj->config_settings->shortcuts_include_single_char) {
            if ((sj->search_query->len == 1 && sj->words->len < 26) ||
                (sj->search_query->len == 2 && sj->words->len >= 26)) {
                shortcut_cancel(sj);
                return TRUE;
            }
        }

        g_string_append_c(sj->search_query, keychar);

        sj->words = shortcut_mark_words(sj->sci, sj->words, sj->search_query);
        sj->search_results_count = shortcut_get_search_results_count(sj->sci, sj->words);
        sj->shortcut_single_pos = shortcut_get_highlighted_pos(sj->sci, sj->words);

        if (sj->search_results_count == 0) {
            shortcut_cancel(sj);
            return TRUE;
        }

        for (gint i = 0; i < sj->words->len; i++) {
            Word word = g_array_index(sj->words, Word, i);

            if (word.shortcut_marked && sj->config_settings->wait_for_enter) {
                scintilla_send_message(sj->sci, SCI_INDICATORFILLRANGE, word.starting + word.padding,
                                       sj->search_query->len);
            }

            if (word.shortcut_marked && !sj->config_settings->wait_for_enter) {
                scintilla_send_message(sj->sci, SCI_INDICATORFILLRANGE, word.starting + word.padding,
                                       word.shortcut->len);
            }
        }

        if (!sj->config_settings->shortcuts_include_single_char && sj->search_query->len == 1) {
            return TRUE;
        }

        if (sj->search_results_count == 1 && !sj->config_settings->wait_for_enter) {
            Word word = g_array_index(sj->words, Word, sj->shortcut_single_pos);
            shortcut_complete(sj, word.starting_doc, word.word->len, word.line);
        }

        return TRUE;
    }

    if (event->keyval == GDK_KEY_Shift_L || event->keyval == GDK_KEY_Shift_R || event->keyval == GDK_KEY_Caps_Lock ||
        event->keyval == GDK_KEY_Control_L || event->keyval == GDK_KEY_Control_R) {
        return TRUE;
    }

    shortcut_cancel(sj);
    return FALSE;
}

void set_after_shortcut_placement(ShortcutJump *sj) {
    gint current_line = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, sj->current_cursor_pos, 0);

    if (!sj->in_selection) {
        scintilla_send_message(sj->sci, SCI_SETFIRSTVISIBLELINE, sj->first_line_on_screen, 0);
    }

    scintilla_send_message(sj->sci, SCI_BEGINUNDOACTION, 0, 0);
    scintilla_send_message(sj->sci, SCI_DELETERANGE, sj->first_position, sj->last_position - sj->first_position);
    scintilla_send_message(sj->sci, SCI_INSERTTEXT, sj->first_position, (sptr_t)sj->buffer->str);
    scintilla_send_message(sj->sci, SCI_SETREADONLY, 1, 0);

    gint lfs_added = get_lfs(sj, current_line - 1);
    scintilla_send_message(sj->sci, SCI_GOTOPOS, sj->current_cursor_pos + lfs_added, 0);
}

/**
 * @brief Gets every char that matches the search query and assigns them tags for jumping.
 *
 *  @param ShortcutJump *sj: The plugin object
 *  @param gchar search_char: The char to search for
 */
void shortcut_char_get_chars(ShortcutJump *sj, gchar search_char) {
    if (sj->current_mode != JM_SHORTCUT_CHAR_WAITING) {
        return;
    }

    gint lfs_added = 0;
    gint prev_line = sj->in_selection && sj->config_settings->search_from_selection
                         ? scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, sj->first_position, 0)
                         : sj->first_line_on_screen;
    gint toggle = 0;
    gint added = 0;
    gchar prev_char;

    if (sj->delete_added_bracket) {
        scintilla_send_message(sj->sci, SCI_DELETERANGE, sj->current_cursor_pos, 1);
        sj->current_cursor_pos = scintilla_send_message(sj->sci, SCI_GETCURRENTPOS, 0, 0);
        scintilla_send_message(sj->sci, SCI_SETCURRENTPOS, sj->current_cursor_pos, 0);
    }

    for (gint i = sj->first_position; i < sj->last_position; i++) {
        if (added == get_max_words(sj)) {
            break;
        }

        gchar current = scintilla_send_message(sj->sci, SCI_GETCHARAT, i, TRUE);

        if (current != search_char) {
            continue;
        }

        gchar char_to_the_left = scintilla_send_message(sj->sci, SCI_GETCHARAT, i - 1, TRUE);

        if (char_to_the_left != prev_char) {
            toggle = 1;
        }

        Word word;

        gboolean good = FALSE;

        if (sj->config_settings->shortcuts_include_single_char) {
            if (sj->words->len > 26) {
                good = TRUE;
            }
        }

        if (!sj->config_settings->shortcuts_include_single_char) {
            good = TRUE;
        }

        if (current == search_char && prev_char == current && toggle == 0 && good) {
            toggle ^= 1;

            GString *ch = g_string_new("");
            g_string_insert_c(ch, 0, current);
            word.word = ch;

            word.valid_search = TRUE;
            word.is_hidden_neighbor = TRUE;
            word.starting = i + lfs_added;
            word.starting_doc = i;
            word.bytes = shortcut_utf8_char_length(word.word->str[0]);
            word.line = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, i, 0);
            word.padding = shortcut_set_word_padding(sj, word.word->len);
            word.replace_pos = i - sj->first_position;

            g_array_append_val(sj->words, word);

            continue;
        }

        toggle ^= 1;
        prev_char = current;

        GString *ch = g_string_new("");
        g_string_insert_c(ch, 0, current);
        word.word = ch;

        word.valid_search = TRUE;
        word.is_hidden_neighbor = FALSE;
        word.starting = i + lfs_added;
        word.starting_doc = i;
        word.bytes = shortcut_utf8_char_length(word.word->str[0]);
        word.line = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, i, 0);
        word.padding = shortcut_set_word_padding(sj, word.word->len);
        word.replace_pos = i - sj->first_position;

        word.shortcut = shortcut_make_tag(sj, added++);

        if (i + 1 == sj->last_position && word.shortcut->len == 2) {
            g_string_insert_c(sj->buffer, sj->last_position, ' ');
        }

        gchar line_ending_char = scintilla_send_message(sj->sci, SCI_GETCHARAT, i + 1, TRUE);

        if (line_ending_char == '\n' && word.shortcut->len == 2) {
            g_string_insert_c(sj->buffer, lfs_added + i - sj->first_position, '\n');

            gint line = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, i, 0);

            if (line != prev_line) {
                for (gint i = prev_line; i < line; i++) {
                    g_array_append_val(sj->lf_positions, lfs_added);
                }

                prev_line = line;
            }

            lfs_added += 1;
        }

        g_array_append_val(sj->words, word);
    }

    for (gint i = prev_line; i < sj->last_line_on_screen; i++) {
        g_array_append_val(sj->lf_positions, lfs_added);
    }

    sj->search_results_count = sj->words->len;
}

/**
 * @brief Handles key press event during shortcut jump.
 *
 * @param GtkWidget *widget: (unused)
 * @param GdkEventKey *event: Keypress event
 * @param gpointer user_data: The plugin data
 *
 * @return gboolean: False if uncontrolled for key press
 */
gboolean on_key_press_shortcut(GtkWidget *widget, GdkEventKey *event, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;

    if (sj->current_mode == JM_SHORTCUT || sj->current_mode == JM_LINE) {
        return shortcut_on_key_press(event, sj);
    }

    return FALSE;
}

/**
 * @brief Clears the memory allocated during an incomplete shortcut jump (when a search query was not provided). This
 * is needed because the words array and other variables that are usually allocated during a char search are not.
 * During JM_SHORTCUT_CHAR_JUMPING we call the standard shortcut_cancel.
 *
 * @param ShortcutJump *sj: The plugin object
 */
void shortcut_char_waiting_cancel(ShortcutJump *sj) {
    g_string_free(sj->eol_message, TRUE);
    g_string_free(sj->search_query, TRUE);
    g_string_free(sj->cache, TRUE);
    g_string_free(sj->buffer, TRUE);

    g_array_free(sj->lf_positions, TRUE);
    g_array_free(sj->words, TRUE);

    block_key_press_action(sj);
    block_click_action(sj);

    sj->current_mode = JM_NONE;

    ui_set_statusbar(TRUE, _("Shortcut jump canceled (no option)"));
}

/**
 * @brief Handles key presses for a shortcut char jump: if we are waiting for a search query we set that char to
 * search_char and proceed to find every instance of the char with shortcut_char_get_chars. If we have a completed
 * search query, we are in jumping mode, and we perform the standard tag jump.
 *
 * @param GtkWidget *widget: (unused)
 * @param GdkEventKey *event: Keypress event
 * @param gpointer user_data: The plugin data
 *
 * @return gboolean: False if uncontrolled for key press
 */
gboolean on_key_press_shortcut_char(GtkWidget *widget, GdkEventKey *event, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;
    gunichar search_char = gdk_keyval_to_unicode(event->keyval);

    if (sj->current_mode == JM_SHORTCUT_CHAR_WAITING) {
        if (mod_key_pressed(event)) {
            return TRUE;
        }

        shortcut_char_get_chars(sj, search_char);

        if (sj->words->len == 0) {
            shortcut_char_waiting_cancel(sj);
            annotation_clear(sj->sci, sj->eol_message_line);
            return FALSE;
        }

        sj->buffer = shortcut_mask_bytes(sj->words, sj->buffer, sj->first_position);
        sj->buffer = shortcut_set_tags_in_buffer(sj->words, sj->buffer, sj->first_position);

        set_after_shortcut_placement(sj);
        set_shortcut_indicators(sj);

        annotation_display_char_search(sj);

        ui_set_statusbar(TRUE, _("%i character%s in view"), sj->words->len, sj->words->len == 1 ? "" : "s");

        sj->current_mode = JM_SHORTCUT_CHAR_JUMPING;

        return TRUE;
    }

    if (sj->current_mode == JM_SHORTCUT_CHAR_JUMPING) {
        annotation_clear(sj->sci, sj->eol_message_line);
        return shortcut_on_key_press(event, sj);
    }

    if (sj->current_mode == JM_SHORTCUT_CHAR_REPLACING) {
        annotation_clear(sj->sci, sj->eol_message_line);
        return replace_handle_input(sj, event, search_char);
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
 * @return gboolean: False if uncontrolled for click or wrong mode
 */
gboolean on_click_event_shortcut(GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;

    if (mouse_movement_performed(sj, event)) {
        if (sj->current_mode == JM_SHORTCUT || sj->current_mode == JM_LINE) {
            sj->current_cursor_pos = scintilla_send_message(sj->sci, SCI_GETCURRENTPOS, 0, 0);

            gint current_line = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, sj->current_cursor_pos, 0);
            gint lfs_at_current_line = g_array_index(sj->lf_positions, gint, current_line - sj->first_line_on_screen);

            sj->current_cursor_pos -= lfs_at_current_line;
            shortcut_cancel(sj);
            return TRUE;
        }
    }

    return FALSE;
}
