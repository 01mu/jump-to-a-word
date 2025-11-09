#include <plugindata.h>

#include "jump_to_a_word.h"
#include "multicursor.h"

void transpose_string(ShortcutJump *sj) {
    gint valid_count = 0;
    for (gint i = 0; i < sj->multicursor_words->len; i++) {
        Word word = g_array_index(sj->multicursor_words, Word, i);
        valid_count += word.valid_search ? 1 : 0;
    }
    if (valid_count != 2) {
        multicursor_end(sj);
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

    multicursor_complete(sj);
}
