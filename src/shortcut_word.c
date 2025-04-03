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

#include "jump_to_a_word.h"
#include "line_options.h"
#include "selection.h"
#include "shortcut_common.h"
#include "util.h"
#include "values.h"

/**
 * @brief Places spaces over the word to hide it if that setting is enabled.
 *
 * @param ShortcutJump *sj: The plugin object
 * @param GArray *words: The word array
 * @param GString *buffer: The buffer of the text on screen
 * @param gint first_position: The first position on the screen
 */
static GString *shortcut_hide_word(ShortcutJump *sj, GArray *words, GString *buffer, gint first_position) {
    if (sj->config_settings->hide_word_shortcut_jump) {
        for (gint i = 0; i < words->len; i++) {
            Word word = g_array_index(words, Word, i);
            gint starting = word.starting - first_position;

            for (gint j = 0; j < word.word->len; j++) {
                buffer->str[starting + j] = ' ';
            }
        }
    }

    return buffer;
}

/**
 * @brief Assigns every word on the screen to the words struct, inits line feed and marker arrays, sets
 * configuration for indicators, and activates key press and click signals.
 *
 *  @param ShortcutJump *sj: The plugin object
 */
void shortcut_word_init(ShortcutJump *sj) {
    if (sj->current_mode != JM_NONE) {
        return;
    }

    sj->current_mode = JM_SHORTCUT;

    set_sj_scintilla_object(sj);
    set_selection_info(sj);
    init_sj_values(sj);
    define_indicators(sj->sci, sj);

    gint lfs_added = 0;

    gint prev_line = (sj->in_selection && sj->config_settings->search_from_selection
                          ? scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, sj->first_position, 0)
                          : sj->first_line_on_screen) -
                     1;

    for (gint i = sj->first_position; i < sj->last_position; i++) {
        if (sj->words->len == get_max_words(sj)) {
            break;
        }

        gint start = scintilla_send_message(sj->sci, SCI_WORDSTARTPOSITION, i, TRUE);
        gint end = scintilla_send_message(sj->sci, SCI_WORDENDPOSITION, i, TRUE);

        if (start == end || start < sj->first_position || end > sj->last_position) {
            continue;
        }

        Word word;

        word.word = g_string_new(sci_get_contents_range(sj->sci, start, end));
        word.starting = start + lfs_added;
        word.starting_doc = start;
        word.is_hidden_neighbor = FALSE;
        word.bytes = shortcut_utf8_char_length(word.word->str[0]);
        word.shortcut = shortcut_make_tag(sj, sj->words->len);
        word.line = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, start, 0);
        word.padding = shortcut_set_word_padding(sj, word.word->len);

        if (i + 1 == sj->last_position && word.shortcut->len == 2) {
            g_string_insert_c(sj->buffer, sj->last_position, '\n');
        }

        gchar line_ending_char = scintilla_send_message(sj->sci, SCI_GETCHARAT, end, TRUE);

        if (line_ending_char == '\n' && word.word->len == 1 && word.shortcut->len == 2) {
            g_string_insert_c(sj->buffer, lfs_added + end - sj->first_position, '\n');

            gint line = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, i, 0);

            if (line != prev_line) {
                for (gint i = prev_line; i < line; i++) {
                    g_array_append_val(sj->lf_positions, lfs_added);
                }

                prev_line = line;
            }

            lfs_added++;
        }

        g_array_append_val(sj->words, word);
        i += word.word->len;
    }

    for (gint i = prev_line; i < sj->last_line_on_screen; i++) {
        g_array_append_val(sj->lf_positions, lfs_added);
    }

    sj->buffer = shortcut_mask_bytes(sj->words, sj->buffer, sj->first_position);
    sj->buffer = shortcut_hide_word(sj, sj->words, sj->buffer, sj->first_position);
    sj->buffer = shortcut_set_tags_in_buffer(sj->words, sj->buffer, sj->first_position);

    set_after_shortcut_placement(sj);

    for (gint i = 0; i < sj->words->len; i++) {
        Word word = g_array_index(sj->words, Word, i);

        set_indicator_for_range(sj->sci, INDICATOR_TAG, word.starting + word.padding, word.shortcut->len);
        set_indicator_for_range(sj->sci, INDICATOR_TEXT, word.starting + word.padding, word.shortcut->len);
    }

    set_key_press_action(sj, on_key_press_shortcut);
    set_click_action(sj, on_click_event_shortcut);

    ui_set_statusbar(TRUE, _("%i word%s in view"), sj->words->len, sj->words->len == 1 ? "" : "s");
}

/**
 * @brief Provides a menu callback for performing a shortcut jump.
 *
 * @param GtkMenuItem *menu_item: (unused)
 * @param gpointer user_data: The plugin data
 */
void shortcut_cb(GtkMenuItem *menu_item, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;

    if (sj->current_mode == JM_NONE) {
        shortcut_word_init(sj);
    }
}

/**
 * @brief Provides a keybinding callback for performing a shortcut jump.
 *
 * @param GtkMenuItem *kb: (unused)
 * @param guint key_id: (unused)
 * @param gpointer user_data: The plugin data
 *
 * @return gboolean: TRUE
 */
gboolean shortcut_kb(GeanyKeyBinding *kb, guint key_id, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;

    if (sj->current_mode == JM_NONE) {
        shortcut_word_init(sj);
        return TRUE;
    }

    return FALSE;
}
