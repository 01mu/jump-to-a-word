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
#include "multicursor.h"

void transpose_string(ShortcutJump *sj) {
    sj->current_mode = JM_TRANSPOSE_MULTICURSOR;

    gint valid_count = 0;

    for (gint i = 0; i < sj->multicursor_words->len; i++) {
        Word word = g_array_index(sj->multicursor_words, Word, i);
        valid_count += word.valid_search ? 1 : 0;
    }

    if (valid_count != 2) {
        multicursor_transpose_cancel(sj);
        ui_set_statusbar(TRUE, _("Select 2 strings to transpose."));
        return;
    }

    gint first_valid = -1;
    gint second_valid = -1;

    for (gint i = 0; i < sj->multicursor_words->len; i++) {
        Word word = g_array_index(sj->multicursor_words, Word, i);

        if (word.valid_search && first_valid != -1) {
            second_valid = i;
            break;
        }

        if (word.valid_search) {
            first_valid = i;
        }
    }

    Word word = g_array_index(sj->multicursor_words, Word, first_valid);
    Word next_word = g_array_index(sj->multicursor_words, Word, second_valid);

    if (word.starting_doc > next_word.starting_doc) {
        word = g_array_index(sj->multicursor_words, Word, second_valid);
        next_word = g_array_index(sj->multicursor_words, Word, first_valid);
    }

    gint shift = next_word.starting_doc - word.word->len;

    scintilla_send_message(sj->sci, SCI_SETREADONLY, 0, 0);
    scintilla_send_message(sj->sci, SCI_BEGINUNDOACTION, 0, 0);
    scintilla_send_message(sj->sci, SCI_DELETERANGE, word.starting_doc, word.word->len);
    scintilla_send_message(sj->sci, SCI_DELETERANGE, shift, next_word.word->len);
    scintilla_send_message(sj->sci, SCI_INSERTTEXT, word.starting_doc, (sptr_t)next_word.word->str);
    scintilla_send_message(sj->sci, SCI_INSERTTEXT, shift + next_word.word->len, (sptr_t)word.word->str);

    multicursor_transpose_complete(sj);
}
