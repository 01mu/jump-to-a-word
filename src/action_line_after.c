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

void shortcut_line_handle_after_action(ShortcutJump *sj, gint target) {
    gboolean line_range_jumped = FALSE;
    LineAfter la = sj->config_settings->line_after;

    if (la == LA_DO_NOTHING || la == LA_JUMP_TO_WORD_SHORTCUT || la == LA_JUMP_TO_SUBSTRING_SEARCH ||
        la == LA_JUMP_TO_WORD_SEARCH || la == LA_JUMP_TO_CHARACTER_SHORTCUT) {
        gint pos_target = scintilla_send_message(sj->sci, SCI_POSITIONFROMLINE, target, TRUE);
        scintilla_send_message(sj->sci, SCI_GOTOPOS, pos_target, 0);
    }

    if (la == LA_SELECT_LINE) {
        gint pos = scintilla_send_message(sj->sci, SCI_POSITIONFROMLINE, target, TRUE);
        gint line_length = scintilla_send_message(sj->sci, SCI_LINELENGTH, target, TRUE);

        scintilla_send_message(sj->sci, SCI_SETSEL, pos, pos + line_length);
    }

    if (la == LA_SELECT_TO_LINE) {
        gint current_line = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, sj->current_cursor_pos, 0);

        gint pos_current = scintilla_send_message(sj->sci, SCI_POSITIONFROMLINE, current_line, TRUE);
        gint pos_target = scintilla_send_message(sj->sci, SCI_POSITIONFROMLINE, target, TRUE);
        gint current_line_length = scintilla_send_message(sj->sci, SCI_LINELENGTH, current_line, TRUE);
        gint target_line_length = scintilla_send_message(sj->sci, SCI_LINELENGTH, target, TRUE);

        if (pos_current > pos_target) {
            scintilla_send_message(sj->sci, SCI_SETSEL, pos_current + current_line_length, pos_target);
        } else {
            scintilla_send_message(sj->sci, SCI_SETSEL, pos_current, pos_target + target_line_length);
        }
    }

    if (la == LA_SELECT_LINE_RANGE && sj->range_is_set) {
        scintilla_send_message(sj->sci, SCI_MARKERDELETE, sj->range_first_pos, 0);

        gint first_line = sj->range_first_pos < target ? sj->range_first_pos : target;
        gint second_line = (sj->range_first_pos >= target ? sj->range_first_pos : target);
        gint pos_first = scintilla_send_message(sj->sci, SCI_POSITIONFROMLINE, first_line, TRUE);
        gint pos_second = scintilla_send_message(sj->sci, SCI_POSITIONFROMLINE, second_line, TRUE);
        gint second_line_length = scintilla_send_message(sj->sci, SCI_LINELENGTH, second_line, TRUE);

        if (sj->range_first_pos < target) {
            scintilla_send_message(sj->sci, SCI_SETSEL, pos_first, pos_second + second_line_length);
        } else {
            scintilla_send_message(sj->sci, SCI_SETSEL, pos_second + second_line_length, pos_first);
        }

        line_range_jumped = TRUE;
    }

    if (la == LA_SELECT_LINE_RANGE && !sj->range_is_set) {
        scintilla_send_message(sj->sci, SCI_MARKERDEFINE, 0, SC_MARK_SHORTARROW);
        scintilla_send_message(sj->sci, SCI_MARKERADD, target, 0);
        scintilla_send_message(sj->sci, SCI_GOTOPOS, sj->current_cursor_pos, 0);
        sj->range_first_pos = target;
        g_string_erase(sj->search_query, 0, sj->search_query->len);
        sj->range_is_set = TRUE;
    }

    if (line_range_jumped) {
        sj->range_is_set = FALSE;
    }
}
