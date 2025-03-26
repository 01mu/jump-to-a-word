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
#include "line_options.h"
#include "search_substring.h"
#include "search_word.h"
#include "selection.h"
#include "shortcut_char.h"
#include "shortcut_common.h"
#include "shortcut_line.h"
#include "shortcut_word.h"
#include "util.h"
#include "values.h"

/**
 * @brief Begins replace search word mode by setting the current mode to JM_SEARCH_MODE. Any key press made afterwards
 * will register as replace inputs in search_on_key_press. This method is runs after search_init has been called, either
 * after a standard search and repalce or an instant replace.
 *
 * @param ShortcutJump *sj: The plugin object
 * @param gboolean instant_replace: If instant replace mode is enabled
 */
static void replace_init(ShortcutJump *sj, gboolean instant_replace) {
    if (sj->current_mode == JM_REPLACE_SEARCH || sj->current_mode == JM_SHORTCUT_CHAR_REPLACING) {
        if (sj->current_mode == JM_REPLACE_SEARCH) {
            search_replace_cancel(sj);
        }

        if (sj->current_mode == JM_SHORTCUT_CHAR_REPLACING) {
            shortcut_char_replacing_cancel(sj);
        }

        sj->current_mode = JM_NONE;
        return;
    }

    if (sj->current_mode == JM_SHORTCUT_CHAR_JUMPING) {
        if (sj->words->len == 0) {
            ui_set_statusbar(TRUE, _("No words to replace"));
            search_cancel(sj);
            return;
        }

        scintilla_send_message(sj->sci, SCI_SETREADONLY, 0, 0);
        scintilla_send_message(sj->sci, SCI_ENDUNDOACTION, 0, 0);
        scintilla_send_message(sj->sci, SCI_UNDO, 0, 0);

        sj->replace_cache = g_string_new(sj->cache->str);

        scintilla_send_message(sj->sci, SCI_BEGINUNDOACTION, 0, 0);

        if (!sj->in_selection) {
            scintilla_send_message(sj->sci, SCI_SETFIRSTVISIBLELINE, sj->first_line_on_screen, 0);
        }

        scintilla_send_message(sj->sci, SCI_GOTOPOS, sj->current_cursor_pos, 0);

        annotation_display_replace_char(sj);
        search_clear_indicators(sj->sci, sj->words);

        sj->current_mode = JM_SHORTCUT_CHAR_REPLACING;
    }

    if (sj->current_mode == JM_SEARCH || sj->current_mode == JM_SUBSTRING) {
        if (sj->search_results_count == 0) {
            ui_set_statusbar(TRUE, _("No words to replace"));
            search_cancel(sj);
            return;
        }

        gchar *s = sci_get_contents_range(sj->sci, sj->first_position, sj->last_position);
        sj->replace_cache = g_string_new(s);

        scintilla_send_message(sj->sci, SCI_BEGINUNDOACTION, 0, 0);

        scintilla_send_message(sj->sci, SCI_GOTOPOS, sj->current_cursor_pos, 0);

        annotation_clear(sj->sci, sj->eol_message_line);

        search_clear_indicators(sj->sci, sj->words);

        if (instant_replace) {
            scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, 0, 0);

            for (gint i = 0; i < sj->words->len; i++) {
                Word *word = &g_array_index(sj->words, Word, i);
                scintilla_send_message(sj->sci, SCI_INDICATORCLEARRANGE, word->starting, word->word->len);

                if (word->valid_search) {
                    scintilla_send_message(sj->sci, SCI_INDICATORFILLRANGE, word->starting, word->word->len);
                }
            }
        }

        if (sj->current_mode == JM_SEARCH) {
            annotation_display_replace(sj);
            sj->current_mode = JM_REPLACE_SEARCH;
        }

        if (sj->current_mode == JM_SUBSTRING) {
            annotation_display_replace_substring(sj);
            sj->current_mode = JM_REPLACE_SUBSTRING;
        }
    }
}

/**
 * @brief Begin instant replace word mode.
 *
 * @param ShortcutJump *sj: The plugin object
 */
static void replace_instant_init(ShortcutJump *sj) {
    set_sj_scintilla_object(sj);
    set_selection_info(sj);

    sj->current_cursor_pos = save_cursor_position(sj);
    sj->replace_instant = TRUE;

    define_indicators(sj->sci, sj);

    if (sj->selection_is_a_char) {
        gchar to_replace = scintilla_send_message(sj->sci, SCI_GETCHARAT, sj->selection_start, 0);

        sj->current_mode = JM_SHORTCUT_CHAR_WAITING;

        scintilla_send_message(sj->sci, SCI_SETEMPTYSELECTION, sj->current_cursor_pos, 0);
        sj->current_cursor_pos = save_cursor_position(sj) - 1;

        set_selection_info(sj);
        init_sj_values(sj);

        shortcut_char_get_chars(sj, to_replace);

        sj->buffer = shortcut_mask_bytes(sj->words, sj->buffer, sj->first_position);
        sj->buffer = shortcut_set_tags_in_buffer(sj->words, sj->buffer, sj->first_position);
        set_after_shortcut_placement(sj);

        ui_set_statusbar(TRUE, _("%i char%s in view"), sj->words->len, sj->words->len == 1 ? "" : "s");

        if (sj->words->len == 0) {
            shortcut_cancel(sj);
            return;
        }

        sj->replace_cache = g_string_new(sj->cache->str);

        sj->current_mode = JM_SHORTCUT_CHAR_JUMPING;

        replace_init(sj, TRUE);
        set_word_indicators(sj);

        set_key_press_action(sj, on_key_press_shortcut_char);
        set_click_action(sj, on_click_event_shortcut_char);
    } else if (sj->selection_is_a_word) {
        search_init(sj, TRUE);
        replace_init(sj, TRUE);
    } else {
        substring_init(sj, TRUE);
        replace_init(sj, TRUE);
    }
}

/**
 * @brief Provides a menu callback for entering word search replacement mode.
 *
 * @param GtkMenuItem *menuitem: (unused)
 * @param gpointer user_data: The plugin data
 */
void replace_search_cb(GtkMenuItem *menuitem, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;

    if (sj->current_mode == JM_SEARCH || sj->current_mode == JM_REPLACE_SEARCH ||
        sj->current_mode == JM_SHORTCUT_CHAR_JUMPING || sj->current_mode == JM_SUBSTRING) {
        replace_init(sj, FALSE);
    }
}

/**
 * @brief Provides a keybinding callback for entering word search replacement mode.
 *
 * @param GtkMenuItem *kb: (unused)
 * @param guint key_id: (unused)
 * @param gpointer user_data: The plugin data
 *
 * @return gboolean: True
 */
gboolean replace_search_kb(GeanyKeyBinding *kb, guint key_id, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;

    if (sj->current_mode == JM_SEARCH || sj->current_mode == JM_REPLACE_SEARCH ||
        sj->current_mode == JM_SHORTCUT_CHAR_JUMPING || sj->current_mode == JM_SUBSTRING) {
        replace_init(sj, FALSE);
    }

    if (sj->current_mode == JM_NONE) {
        replace_instant_init(sj);
    }

    return TRUE;
}
