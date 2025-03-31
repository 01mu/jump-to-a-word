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
 * @brief Begins jump to line mode.
 *
 * @param ShortcutJump *sj: The plugin object
 */
void line_init(ShortcutJump *sj) {
    if (sj->current_mode != JM_NONE) {
        return;
    }

    sj->current_mode = JM_LINE;

    set_sj_scintilla_object(sj);
    set_selection_info(sj);
    init_sj_values(sj);
    define_indicators(sj->sci, sj);

    gint lfs_added = 0;

    gint prev_line = sj->first_line_on_screen;
    gint indent_width = get_indent_width() - 1;

    for (gint current_line = sj->first_line_on_screen; current_line <= sj->last_line_on_screen; current_line++) {
        if (sj->words->len == get_max_words(sj)) {
            break;
        }

        gint pos = scintilla_send_message(sj->sci, SCI_POSITIONFROMLINE, current_line, TRUE);

        if (pos >= sj->last_position) {
            break;
        }

        Word word;

        word.word = g_string_new(sci_get_contents_range(sj->sci, pos, pos + 1));
        word.starting = pos + lfs_added;
        word.starting_doc = pos;
        word.is_hidden_neighbor = FALSE;
        word.bytes = shortcut_utf8_char_length(word.word->str[0]);
        word.shortcut = shortcut_make_tag(sj, sj->words->len);
        word.line = current_line;
        word.padding = 0;

        gchar c = scintilla_send_message(sj->sci, SCI_GETCHARAT, pos, TRUE);

        if (c == '\t') {
            for (gint i = 0; i < indent_width; i++) {
                g_string_insert_c(sj->buffer, lfs_added + pos - sj->first_position, ' ');
            }

            lfs_added += indent_width;
        }

        if (word.shortcut->len == 1) {
            gchar first_char_on_line = scintilla_send_message(sj->sci, SCI_GETCHARAT, pos, TRUE);

            if (first_char_on_line == '\n') {
                g_string_insert_c(sj->buffer, lfs_added++ + pos - sj->first_position, ' ');
            }
        }

        if (word.shortcut->len == 2) {
            gchar first_char_on_line = scintilla_send_message(sj->sci, SCI_GETCHARAT, pos, TRUE);
            gchar next_char = scintilla_send_message(sj->sci, SCI_GETCHARAT, pos + 1, TRUE);

            gchar line_of_next_char = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, pos + 1, TRUE);

            if (current_line != line_of_next_char && first_char_on_line == '\n') {
                g_string_insert_c(sj->buffer, lfs_added++ + pos - sj->first_position, ' ');
                g_string_insert_c(sj->buffer, lfs_added++ + pos - sj->first_position, ' ');
            }

            if (next_char == '\n') {
                g_string_insert_c(sj->buffer, lfs_added++ + pos - sj->first_position, ' ');
            }
        }

        g_array_append_val(sj->lf_positions, lfs_added);
        g_array_append_val(sj->words, word);
    }

    for (gint i = prev_line; i < sj->last_line_on_screen; i++) {
        g_array_append_val(sj->lf_positions, lfs_added);
    }

    sj->buffer = shortcut_mask_bytes(sj->words, sj->buffer, sj->first_position);
    sj->buffer = shortcut_set_tags_in_buffer(sj->words, sj->buffer, sj->first_position);

    set_after_shortcut_placement(sj);

    for (gint i = 0; i < sj->words->len; i++) {
        Word word = g_array_index(sj->words, Word, i);

        set_indicator_for_range(sj->sci, INDICATOR_TAG, word.starting + word.padding, word.shortcut->len);
        set_indicator_for_range(sj->sci, INDICATOR_TEXT, word.starting + word.padding, word.shortcut->len);
    }

    set_key_press_action(sj, on_key_press_shortcut);
    set_click_action(sj, on_click_event_shortcut);

    ui_set_statusbar(TRUE, _("%i line%s in view"), sj->words->len, sj->words->len == 1 ? "" : "s");
}

/**
 * @brief Provides a menu callback for jumping to a line.
 *
 * @param GtkMenuItem *menu_item: (unused)
 * @param gpointer user_data: The plugin data
 */
void jump_to_line_cb(GtkMenuItem *menu_item, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;

    if (sj->current_mode == JM_NONE) {
        line_init(sj);
    }
}

/**
 * @brief Provides a keybinding callback for jumping to a line.
 *
 * @param GtkMenuItem *kb: (unused)
 * @param guint key_id: (unused)
 * @param gpointer user_data: The plugin data
 *
 * @return gboolean: TRUE
 */
gboolean jump_to_line_kb(GeanyKeyBinding *kb, guint key_id, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;

    if (sj->current_mode == JM_NONE) {
        line_init(sj);
    }

    return TRUE;
}
