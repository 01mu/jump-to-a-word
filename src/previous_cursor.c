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
#include "search_word.h"
#include "selection.h"
#include "shortcut_common.h"
#include "shortcut_line.h"
#include "shortcut_word.h"
#include "util.h"
#include "values.h"

/**
 * @brief Jumps to the previous cursor position and sets the previous_cursor_pos variable to the current position.
 *
 * @param ShortcutJump *sj: The plugin object
 */
static void previous_cursor_init(ShortcutJump *sj) {
    gint temp = scintilla_send_message(sj->sci, SCI_GETCURRENTPOS, 0, 0);

    if (sj->previous_cursor_pos == -1) {
        scintilla_send_message(sj->sci, SCI_GOTOPOS, temp, 0);
    } else {
        if (sj->config_settings->move_marker_to_line) {
            GeanyDocument *doc = document_get_current();

            if (!doc->is_valid) {
                exit(1);
            } else {
                gint line = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, sj->previous_cursor_pos, 0);

                navqueue_goto_line(doc, doc, line + 1);
            }
        } else {
            scintilla_send_message(sj->sci, SCI_GOTOPOS, sj->previous_cursor_pos, 0);
        }

        sj->previous_cursor_pos = temp;
    }
}

/**
 * @brief Provides a menu callback for jumping to a previous cursor.
 *
 * @param GtkMenuItem *menuitem: (unused)
 * @param gpointer user_data: The plugin data
 */
void jump_to_previous_cursor_cb(GtkMenuItem *menuitem, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;
    sj->sci = get_scintilla_object();

    if (sj->current_mode == JM_NONE) {
        previous_cursor_init(sj);
    }
}

/**
 * @brief Provides a keybinding callback for jumping to a previous cursor.
 *
 * @param GtkMenuItem *kb: (unused)
 * @param guint key_id: (unused)
 * @param gpointer user_data: The plugin data
 *
 * @return gboolean: True
 */
gboolean jump_to_previous_cursor_kb(GeanyKeyBinding *kb, guint key_id, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;
    sj->sci = get_scintilla_object();

    if (sj->current_mode == JM_NONE) {
        previous_cursor_init(sj);
    }

    return TRUE;
}
