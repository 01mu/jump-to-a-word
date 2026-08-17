#include "jump_to_a_word.h"

void multicursor_add_text_from_selection(ShortcutJump *sj, gint start, gint end) {
    Word multicursor_word;

    multicursor_word.word = g_string_new(sci_get_contents_range(sj->sci, start, end));
    multicursor_word.line = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, start, 0);
    multicursor_word.starting = start;
    multicursor_word.starting_doc = start;
    multicursor_word.valid_search = TRUE;

    gint new_word_start = multicursor_word.starting_doc;
    gint new_word_end = multicursor_word.starting_doc + multicursor_word.word->len;

    for (gint i = 0; i < sj->multicursor_words->len; i++) {
        Word *word = &g_array_index(sj->multicursor_words, Word, i);

        gint old_word_start = word->starting_doc;
        gint old_word_end = word->starting_doc + word->word->len;

        if (!word->valid_search) {
            continue;
        }

        if (new_word_start == old_word_start && new_word_end == old_word_end) {
            scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_MULTICURSOR, 0);
            scintilla_send_message(sj->sci, SCI_INDICATORCLEARRANGE, word->starting_doc, word->word->len);
            word->valid_search = FALSE;
            return;
        }

        gboolean old_enclosed_by_new = old_word_start >= new_word_start && old_word_end <= new_word_end;
        gboolean new_start_enclosed_by_old = new_word_start > old_word_start && new_word_start < old_word_end;
        gboolean new_end_enclosed_by_old = new_word_end > old_word_start && new_word_end < old_word_end;

        if (old_enclosed_by_new || new_start_enclosed_by_old || new_end_enclosed_by_old) {
            scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_MULTICURSOR, 0);
            scintilla_send_message(sj->sci, SCI_INDICATORCLEARRANGE, word->starting_doc, word->word->len);
            scintilla_send_message(sj->sci, SCI_INDICATORFILLRANGE, multicursor_word.starting_doc,
                                   multicursor_word.word->len);
            scintilla_send_message(sj->sci, SCI_INDICATORCLEARRANGE, word->starting_doc, word->word->len);
            word->valid_search = FALSE;
        }
    }

    scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_MULTICURSOR, 0);
    scintilla_send_message(sj->sci, SCI_INDICATORFILLRANGE, multicursor_word.starting_doc, multicursor_word.word->len);

    if (multicursor_word.starting_doc <= sj->multicursor_first_pos) {
        sj->multicursor_first_pos = multicursor_word.starting_doc;
    }

    if (new_word_end >= sj->multicursor_last_pos) {
        sj->multicursor_last_pos = new_word_end;
    }

    g_array_append_val(sj->multicursor_words, multicursor_word);
}

void multicursor_add_text_from_search(ShortcutJump *sj, Word word) {
    Word multicursor_word;

    multicursor_word.word = g_string_new(word.word->str);
    multicursor_word.line = word.line;
    multicursor_word.starting = word.starting;
    multicursor_word.starting_doc = word.starting_doc;
    multicursor_word.valid_search = TRUE;

    gint new_word_start = multicursor_word.starting_doc;
    gint new_word_end = multicursor_word.starting_doc + multicursor_word.word->len;

    for (gint i = 0; i < sj->multicursor_words->len; i++) {
        Word *word = &g_array_index(sj->multicursor_words, Word, i);

        gint old_word_start = word->starting_doc;
        gint old_word_end = word->starting_doc + word->word->len;

        if (!word->valid_search) {
            continue;
        }

        if (new_word_start == old_word_start) {
            scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_MULTICURSOR, 0);
            scintilla_send_message(sj->sci, SCI_INDICATORCLEARRANGE, word->starting_doc, word->word->len);
            word->valid_search = FALSE;
            return;
        }

        gboolean old_enclosed_by_new = old_word_start >= new_word_start && old_word_end <= new_word_end;
        gboolean new_start_enclosed_by_old = new_word_start > old_word_start && new_word_start < old_word_end;
        gboolean new_end_enclosed_by_old = new_word_end > old_word_start && new_word_end < old_word_end;

        if (old_enclosed_by_new || new_start_enclosed_by_old || new_end_enclosed_by_old) {
            scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_MULTICURSOR, 0);
            scintilla_send_message(sj->sci, SCI_INDICATORCLEARRANGE, word->starting_doc, word->word->len);
            scintilla_send_message(sj->sci, SCI_INDICATORFILLRANGE, multicursor_word.starting_doc,
                                   multicursor_word.word->len);
            word->valid_search = FALSE;
        }
    }

    if (multicursor_word.starting_doc <= sj->multicursor_first_pos) {
        sj->multicursor_first_pos = multicursor_word.starting_doc;
    }

    if (new_word_end >= sj->multicursor_last_pos) {
        sj->multicursor_last_pos = new_word_end;
    }

    g_array_append_val(sj->multicursor_words, multicursor_word);
}
