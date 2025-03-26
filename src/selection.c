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
#include "previous_cursor.h"
#include "search_word.h"
#include "shortcut_common.h"
#include "shortcut_line.h"
#include "shortcut_word.h"
#include "util.h"

/**
 * @brief Checks to see if the selection is a word. If the selection is a w word we use the entire page as a search
 * area instead (see usage). It seems unlikely someone would highlight a single word just to jump to/search for it.
 *
 * @param ScintillaObject *sci: The Scintilla object
 * @param gint selection_start: The start of the selection
 * @param gint selection_end: The end of the selection
 *
 * @return gboolean: Whether the entire selection is a single word
 */
static gboolean selection_is_a_word(ScintillaObject *sci, gint selection_start, gint selection_end) {
    gint start_of_first = scintilla_send_message(sci, SCI_WORDSTARTPOSITION, selection_start, TRUE);
    gint end_of_first = scintilla_send_message(sci, SCI_WORDENDPOSITION, selection_start, TRUE);

    gint start_of_last = scintilla_send_message(sci, SCI_WORDSTARTPOSITION, selection_end, TRUE);
    gint end_of_last = scintilla_send_message(sci, SCI_WORDENDPOSITION, selection_end, TRUE);

    // gboolean iw = scintilla_send_message(sci, SCI_ISRANGEWORD, start_of_first, end_of_first);
    // ui_set_statusbar(TRUE, _("%i"), iw);

    return start_of_first != end_of_last && start_of_first == start_of_last && end_of_first == end_of_last;
}

/**
 * @brief Checks to see if the selection is a line. Used for setting if enabled.
 *
 * @param ScintillaObject *sci: The Scintilla object
 * @param gint selection_start: The start of the selection
 * @param gint selection_end: The end of the selection
 *
 * @return gboolean: Whether the entire selection is a single word
 */
static gboolean selection_is_a_line(ShortcutJump *sj, ScintillaObject *sci, gint selection_start, gint selection_end) {
    selection_end -= 1;

    gint line_of_start = scintilla_send_message(sci, SCI_LINEFROMPOSITION, selection_start, TRUE);
    gint line_of_end = scintilla_send_message(sci, SCI_LINEFROMPOSITION, selection_end, TRUE);

    // gint start_pos = scintilla_send_message(sci, SCI_POSITIONFROMLINE, line_of_start, 0);
    // gint line_length = scintilla_send_message(sci, SCI_LINELENGTH, line_of_start, 0) - 1;
    // gint end_pos = scintilla_send_message(sci, SCI_GETLINEENDPOSITION, line_of_start, 0);

    // if (line_of_start == line_of_end && selection_start == start_pos && selection_end == end_pos &&
    // end_pos == start_pos + line_length) {
    // return TRUE;
    //}

    if (!sj->selection_is_a_word && line_of_start == line_of_end) {
        return TRUE;
    }

    return FALSE;
}

/**
 * @brief Gets the word selection bounds to be used in determining whether we use the selection or the page as a
 * search or replace area.
 *
 * @param ShortcutJump *sj: The plugin object
 */
void set_selection_info(ShortcutJump *sj) {
    gint selection_start = scintilla_send_message(sj->sci, SCI_GETSELECTIONSTART, 0, 0);
    gint selection_end = scintilla_send_message(sj->sci, SCI_GETSELECTIONEND, 0, 0);

    sj->in_selection = selection_start != selection_end && sj->current_mode != JM_LINE;
    sj->selection_start = selection_start;
    sj->selection_end = selection_end;
    sj->selection_is_a_char = selection_end == selection_start + 1;
    sj->selection_is_a_word = selection_is_a_word(sj->sci, selection_start, selection_end);
    sj->selection_is_a_line = selection_is_a_line(sj, sj->sci, selection_start, selection_end);
}
