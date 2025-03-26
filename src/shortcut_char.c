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
#include "selection.h"
#include "shortcut_common.h"
#include "util.h"
#include "values.h"

void shortcut_char_replacing_cancel(ShortcutJump *sj);

/**
 * @brief Handles click event for shortcut char jump. If the search char has not been initialized we run the search
 * so we can free memory and reset values used during the search.
 *
 * @param GtkWidget *widget: (unused)
 * @param GdkEventButton *event: Click event
 * @param gpointer user_data: The plugin data
 *
 * @return gboolean: False if uncontrolled for click or wrong mode
 */
gboolean on_click_event_shortcut_char(GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;

    if (mouse_movement_performed(sj, event)) {
        if (sj->current_mode == JM_SHORTCUT_CHAR_WAITING) {
            shortcut_char_waiting_cancel(sj);
            annotation_clear(sj->sci, sj->eol_message_line);
        }

        if (sj->current_mode == JM_SHORTCUT_CHAR_JUMPING) {
            sj->current_cursor_pos = save_cursor_position(sj);
            sj->current_cursor_pos = set_cursor_position_with_lfs(sj);
            shortcut_cancel(sj);
        }

        if (sj->current_mode == JM_SHORTCUT_CHAR_REPLACING) {
            sj->current_cursor_pos = scintilla_send_message(sj->sci, SCI_GETCURRENTPOS, 0, 0);

            gint current_line = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, sj->current_cursor_pos, 0);
            gint lfs_at_current_line = g_array_index(sj->lf_positions, gint, current_line - sj->first_line_on_screen);

            sj->current_cursor_pos -= lfs_at_current_line;

            shortcut_char_replacing_cancel(sj);
        }

        return TRUE;
    }

    return FALSE;
}

/**
 * @brief Begins shortcut jump to character mode.
 *
 * @param ShortcutJump *sj: The plugin object
 */
void shortcut_char_init(ShortcutJump *sj) {
    sj->current_mode = JM_SHORTCUT_CHAR_WAITING;

    set_sj_scintilla_object(sj);
    set_selection_info(sj);
    init_sj_values(sj);
    define_indicators(sj->sci, sj);

    if (sj->in_selection && sj->selection_is_a_char && sj->config_settings->use_selected_word_or_char) {
        gchar search_char = scintilla_send_message(sj->sci, SCI_GETCHARAT, sj->selection_start, sj->selection_end);

        shortcut_char_get_chars(sj, search_char);

        sj->buffer = shortcut_mask_bytes(sj->words, sj->buffer, sj->first_position);
        sj->buffer = shortcut_set_tags_in_buffer(sj->words, sj->buffer, sj->first_position);

        sj->current_cursor_pos = save_cursor_position(sj);
        set_after_shortcut_placement(sj);
        set_shortcut_indicators(sj);

        ui_set_statusbar(TRUE, _("%i char%s in view"), sj->words->len, sj->words->len == 1 ? "" : "s");

        sj->current_mode = JM_SHORTCUT_CHAR_JUMPING;
    }

    if (!sj->in_selection || (sj->in_selection && !sj->selection_is_a_char)) {
        annotation_display_shortcut_char(sj);
    }

    set_key_press_action(sj, on_key_press_shortcut_char);
    set_click_action(sj, on_click_event_shortcut_char);
}

/**
 * @brief
 *
 *  @param ShortcutJump *sj: The plugin object
 */
void shortcut_char_replacing_cancel(ShortcutJump *sj) {
    annotation_clear(sj->sci, sj->eol_message_line);
    shortcut_end(sj, FALSE);
    ui_set_statusbar(TRUE, _("Shortcut replacement canceled"));
}

/**
 * @brief Provides a menu callback for performing a shortcut char jump.
 *
 * @param GtkMenuItem *menuitem: (unused)
 * @param gpointer user_data: The plugin data
 */
void shortcut_char_cb(GtkMenuItem *menuitem, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;

    if (sj->current_mode == JM_NONE) {
        shortcut_char_init(sj);
    }
}

/**
 * @brief Provides a keybinding callback for performing a shortcut char jump.
 *
 * @param GtkMenuItem *kb: (unused)
 * @param guint key_id: (unused)
 * @param gpointer user_data: The plugin data
 *
 * @return gboolean: True
 */
gboolean shortcut_char_kb(GeanyKeyBinding *kb, guint key_id, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;

    if (sj->current_mode == JM_NONE) {
        shortcut_char_init(sj);
        return TRUE;
    }

    return TRUE;
}
