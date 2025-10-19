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
#include "search_substring.h"
#include "search_word.h"
#include "selection.h"
#include "shortcut_char.h"
#include "shortcut_common.h"
#include "util.h"
#include "values.h"

/**
 * @brief Handles key press event during a search jump after starting a replace action.
 *
 * @param GtkWidget *widget: (unused)
 * @param GdkEventKey *event: Keypress event
 * @param gpointer user_data: The plugin data
 *
 * @return gboolean: FALSE if uncontrolled for key press
 */
static gboolean on_key_press_search_replace(GtkWidget *widget, GdkEventKey *event, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;
    gunichar keychar = gdk_keyval_to_unicode(event->keyval);

    return replace_handle_input(sj, event, keychar);
}

/**
 * @brief Begins the replacement for a selected character.
 *
 * @param ShortcutJump *sj: The plugin object
 * @param gboolean instant_replace: If instant replace mode is enabled
 */
static void replace_shortcut_char_init(ShortcutJump *sj, gboolean instant_replace) {
    if (sj->words->len == 0) {
        ui_set_statusbar(TRUE, _("No characters to replace"));
        search_cancel(sj);
        return;
    }

    scintilla_send_message(sj->sci, SCI_SETREADONLY, 0, 0);
    scintilla_send_message(sj->sci, SCI_ENDUNDOACTION, 0, 0);
    scintilla_send_message(sj->sci, SCI_UNDO, 0, 0);

    scintilla_send_message(sj->sci, SCI_BEGINUNDOACTION, 0, 0);

    set_to_first_visible_line(sj);

    scintilla_send_message(sj->sci, SCI_GOTOPOS, sj->current_cursor_pos, 0);

    annotation_display_replace_char(sj);
    search_clear_indicators(sj->sci, sj->words);

    if (instant_replace) {
        scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_TAG, 0);

        for (gint i = 0; i < sj->words->len; i++) {
            Word word = g_array_index(sj->words, Word, i);
            scintilla_send_message(sj->sci, SCI_INDICATORCLEARRANGE, word.starting_doc, 1);

            if (word.valid_search) {
                scintilla_send_message(sj->sci, SCI_INDICATORFILLRANGE, word.starting_doc, 1);
            }
        }
    }

    sj->current_mode = JM_SHORTCUT_CHAR_REPLACING;
}

/**
 * @brief Sets the indicators for a string or word being replaced.
 *
 * @param ShortcutJump *sj: The plugin object
 * @param gboolean instant_replace: If instant replace mode is enabled
 */
static void set_replace_indicators(ShortcutJump *sj, gboolean instant_replace) {
    scintilla_send_message(sj->sci, SCI_BEGINUNDOACTION, 0, 0);
    scintilla_send_message(sj->sci, SCI_GOTOPOS, sj->current_cursor_pos, 0);
    annotation_clear(sj->sci, sj->eol_message_line);
    search_clear_indicators(sj->sci, sj->words);

    if (instant_replace) {
        scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_TAG, 0);

        for (gint i = 0; i < sj->words->len; i++) {
            Word word = g_array_index(sj->words, Word, i);
            scintilla_send_message(sj->sci, SCI_INDICATORCLEARRANGE, word.starting, word.word->len);

            if (word.valid_search) {
                scintilla_send_message(sj->sci, SCI_INDICATORFILLRANGE, word.starting, word.word->len);
            }
        }
    }
}

/**
 * @brief Begins the replacement for a substring.
 *
 * @param ShortcutJump *sj: The plugin object
 * @param gboolean instant_replace: If instant replace mode is enabled
 */
static void replace_substring_init(ShortcutJump *sj, gboolean instant_replace) {
    if (sj->search_results_count == 0) {
        ui_set_statusbar(TRUE, _("No substrings to replace"));
        search_cancel(sj);
        return;
    }

    set_replace_indicators(sj, instant_replace);
    annotation_display_replace_substring(sj);
    sj->current_mode = JM_REPLACE_SUBSTRING;

    disconnect_key_press_action(sj);
    connect_key_press_action(sj, on_key_press_search_replace);
}

/**
 * @brief Begins the replacement for a search word. Disconnects the previous key action handler that was acceping
 * characters for the search query and initiates the replacement handler.
 *
 * @param ShortcutJump *sj: The plugin object
 * @param gboolean instant_replace: If instant replace mode is enabled
 */
static void replace_word_init(ShortcutJump *sj, gboolean instant_replace) {
    if (sj->search_results_count == 0) {
        ui_set_statusbar(TRUE, _("No words to replace"));
        search_cancel(sj);
        return;
    }

    set_replace_indicators(sj, instant_replace);
    annotation_display_replace(sj);
    sj->current_mode = JM_REPLACE_SEARCH;

    if (sj->config_settings->replace_action == RA_INSERT_END) {
        for (gint i = 0; i < sj->words->len; i++) {
            Word *word = &g_array_index(sj->words, Word, i);

            if (word->valid_search) {
                word->replace_pos += word->word->len;
            }
        }
    }

    disconnect_key_press_action(sj);
    connect_key_press_action(sj, on_key_press_search_replace);
}

/**
 * @brief Begins instant replace mode if we do not have any text selected. The replacement that occurs is based on
 * whether we have a character, a word, or a substring selected.
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

        shortcut_char_init(sj, TRUE, to_replace);
        replace_shortcut_char_init(sj, TRUE);
        return;
    }

    if (!sj->in_selection) {
        search_init(sj, TRUE);
        replace_word_init(sj, TRUE);
        return;
    }

    substring_init(sj, TRUE);
    replace_substring_init(sj, TRUE);
}

/**
 * @brief Begins replacement for specific modes.
 *
 * @param ShortcutJump *sj: The plugin object
 */
static void replace(ShortcutJump *sj) {
    if (sj->current_mode == JM_SEARCH) {
        replace_word_init(sj, FALSE);
    } else if (sj->current_mode == JM_SUBSTRING) {
        replace_substring_init(sj, FALSE);
    } else if (sj->current_mode == JM_SHORTCUT_CHAR_REPLACING) {
        shortcut_char_replacing_cancel(sj);
        sj->current_mode = JM_NONE;
    } else if (sj->current_mode == JM_SHORTCUT_CHAR_WAITING) {
        shortcut_char_waiting_cancel(sj);
        sj->current_mode = JM_NONE;
    } else if (sj->current_mode == JM_REPLACE_SEARCH || sj->current_mode == JM_REPLACE_SUBSTRING) {
        search_replace_cancel(sj);
        sj->current_mode = JM_NONE;
    } else if (sj->current_mode == JM_SHORTCUT_CHAR_JUMPING) {
        replace_shortcut_char_init(sj, FALSE);
    } else if (sj->current_mode == JM_NONE) {
        replace_instant_init(sj);
    }
}

/**
 * @brief Provides a menu callback for entering word search replacement mode.
 *
 * @param GtkMenuItem *menu_item: (unused)
 * @param gpointer user_data: The plugin data
 */
void replace_search_cb(GtkMenuItem *menu_item, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;

    replace(sj);
}
/**
 * @brief Provides a keybinding callback for entering word search replacement mode.
 *
 * @param GtkMenuItem *kb: (unused)
 * @param guint key_id: (unused)
 * @param gpointer user_data: The plugin data
 *
 * @return gboolean: TRUE
 */
gboolean replace_search_kb(GeanyKeyBinding *kb, guint key_id, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;

    replace(sj);
    return TRUE;
}
