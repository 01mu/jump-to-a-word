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

#include "annotation.h"
#include "jump_to_a_word.h"
#include "multicursor.h"
#include "util.h"

void multicursor_transpose_cancel(ShortcutJump *sj) {
    multicursor_replace_clear_indicators(sj);
    annotation_clear(sj->sci, sj->eol_message_line);

    for (gint i = 0; i < sj->multicursor_words->len; i++) {
        Word word = g_array_index(sj->multicursor_words, Word, i);

        scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_MULTICURSOR, 0);
        scintilla_send_message(sj->sci, SCI_INDICATORCLEARRANGE, word.starting, word.word->len);
    }

    toggle_multicursor_menu(sj, FALSE);

    multicursor_end(sj);
    ui_set_statusbar(TRUE, _("Multicursor string duplication canceled."));
}

static void duplication_complete(ShortcutJump *sj) {
    scintilla_send_message(sj->sci, SCI_SETREADONLY, 0, 0);
    scintilla_send_message(sj->sci, SCI_ENDUNDOACTION, 0, 0);

    annotation_clear(sj->sci, sj->eol_message_line);

    toggle_multicursor_menu(sj, FALSE);

    multicursor_end(sj);
    ui_set_statusbar(TRUE, _("Multicursor string duplication completed."));
}

void duplicate_string(ShortcutJump *sj) {
    sj->current_mode = JM_DUPLICATE_MULTICURSOR;

    scintilla_send_message(sj->sci, SCI_SETREADONLY, 0, 0);
    scintilla_send_message(sj->sci, SCI_BEGINUNDOACTION, 0, 0);

    g_array_sort(sj->multicursor_words, sort_words_by_starting_doc);

    gint chars_added = 0;

    for (gint i = 0; i < sj->multicursor_words->len; i++) {
        Word word = g_array_index(sj->multicursor_words, Word, i);

        scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_MULTICURSOR, 0);
        scintilla_send_message(sj->sci, SCI_INDICATORCLEARRANGE, word.starting + chars_added, word.word->len);

        scintilla_send_message(sj->sci, SCI_INSERTTEXT, word.starting_doc + word.word->len + chars_added,
                               (sptr_t)word.word->str);

        chars_added += word.word->len;
    }

    duplication_complete(sj);
}
