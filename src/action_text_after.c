/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 */

#include <plugindata.h>

#include "jump_to_a_word.h"

gboolean handle_text_after_action(ShortcutJump *sj, gint pos, gint word_length, gint line) {
    gboolean text_range_jumped = FALSE;

    if (sj->config_settings->text_after == TX_DO_NOTHING) {
        scintilla_send_message(sj->sci, SCI_GOTOPOS, pos, 0);
    }

    if (sj->config_settings->text_after == TX_SELECT_TEXT) {
        scintilla_send_message(sj->sci, SCI_SETSEL, pos, pos + word_length);
    }

    gboolean select_when_shortcut_char = sj->config_settings->select_when_shortcut_char;
    gboolean mode_shortcut_char_jumping = sj->current_mode == JM_SHORTCUT_CHAR_JUMPING;
    gboolean char_jump_enabled = select_when_shortcut_char && mode_shortcut_char_jumping;

    if (sj->config_settings->text_after == TX_SELECT_TO_TEXT || char_jump_enabled) {
        if (sj->current_cursor_pos > pos) {
            scintilla_send_message(sj->sci, SCI_SETSEL, sj->current_cursor_pos, pos);
        } else {
            scintilla_send_message(sj->sci, SCI_SETSEL, sj->current_cursor_pos, pos + word_length);
        }
    }

    if (sj->config_settings->text_after == TX_SELECT_TEXT_RANGE && sj->range_is_set) {
        if (pos > sj->range_first_pos) {
            scintilla_send_message(sj->sci, SCI_SETSEL, sj->range_first_pos, pos + word_length);
        } else {
            scintilla_send_message(sj->sci, SCI_SETSEL, sj->range_first_pos + sj->range_word_length, pos);
        }

        text_range_jumped = TRUE;
    }

    if (sj->config_settings->text_after == TX_SELECT_TEXT_RANGE && !sj->range_is_set) {
        scintilla_send_message(sj->sci, SCI_MARKERDEFINE, 0, SC_MARK_SHORTARROW);
        scintilla_send_message(sj->sci, SCI_MARKERADD, line, 0);
        sj->range_first_pos = pos;
        sj->range_word_length = word_length;
        g_string_erase(sj->search_query, 0, sj->search_query->len);
        sj->range_is_set = TRUE;
    }

    if (text_range_jumped) {
        sj->range_is_set = FALSE;
        return TRUE;
    }

    return FALSE;
}
