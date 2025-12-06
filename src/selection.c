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

static gboolean selection_is_a_word(ScintillaObject *sci, gint selection_start, gint selection_end) {
    char word_chars[256];

    scintilla_send_message(sci, SCI_GETWORDCHARS, 0, (sptr_t)word_chars);

    gint start_of_first = scintilla_send_message(sci, SCI_WORDSTARTPOSITION, selection_start, TRUE);
    gint end_of_first = scintilla_send_message(sci, SCI_WORDENDPOSITION, selection_start, TRUE);

    gint start_of_last = scintilla_send_message(sci, SCI_WORDSTARTPOSITION, selection_end, TRUE);
    gint end_of_last = scintilla_send_message(sci, SCI_WORDENDPOSITION, selection_end, TRUE);

    gchar left_bound = scintilla_send_message(sci, SCI_GETCHARAT, selection_start - 1, 0);
    gchar right_bound = scintilla_send_message(sci, SCI_GETCHARAT, selection_end, 0);

    gint len = scintilla_send_message(sci, SCI_GETLENGTH, 0, 0);

    gboolean bound_is_word_char = FALSE;

    for (char *p = word_chars; *p != '\0'; p++) {
        guchar ch = (*p >= 32 && *p <= 126) ? *p : '?';

        if ((start_of_first - 1 <= 0 && left_bound == ch) || (end_of_last < len && right_bound == ch)) {
            bound_is_word_char = TRUE;
        }
    }

    return start_of_first != end_of_last && start_of_first == start_of_last && end_of_first == end_of_last &&
           !bound_is_word_char;
}

static gboolean selection_is_a_line(const ShortcutJump *sj, ScintillaObject *sci, gint selection_start,
                                    gint selection_end) {
    selection_end -= 1;

    gint line_of_start = scintilla_send_message(sci, SCI_LINEFROMPOSITION, selection_start, TRUE);
    gint line_of_end = scintilla_send_message(sci, SCI_LINEFROMPOSITION, selection_end, TRUE);

    if (!sj->selection_is_a_word && line_of_start == line_of_end) {
        return TRUE;
    }

    return FALSE;
}

void set_selection_info(ShortcutJump *sj) {
    gint selection_start = scintilla_send_message(sj->sci, SCI_GETSELECTIONSTART, 0, 0);
    gint selection_end = scintilla_send_message(sj->sci, SCI_GETSELECTIONEND, 0, 0);

    sj->in_selection = selection_start != selection_end && sj->current_mode != JM_LINE;
    sj->selection_start = selection_start;
    sj->selection_end = selection_end;
    sj->selection_is_a_char = selection_end == selection_start + 1;
    sj->selection_is_a_word = selection_is_a_word(sj->sci, selection_start, selection_end);
    sj->selection_is_within_a_line = selection_is_a_line(sj, sj->sci, selection_start, selection_end);
}
