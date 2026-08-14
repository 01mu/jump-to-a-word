#include "annotation.h"
#include "jump_to_a_word.h"
#include "multicursor.h"
#include "util.h"
#include "values.h"

void duplicate_end(ShortcutJump *sj) {
    for (gint i = 0; i < sj->words->len; i++) {
        Word word = g_array_index(sj->words, Word, i);
        g_string_free(word.word, TRUE);
    }

    free_sj_values(sj);
    sj->current_mode = JM_NONE;
}

void duplicate_cancel(ShortcutJump *sj) {
    ui_set_statusbar(TRUE, _("Selection duplication canceled."));

    scintilla_send_message(sj->sci, SCI_ENDUNDOACTION, 0, 0);

    annotation_clear(sj->sci, sj->eol_message_line);

    if (sj->current_mode == JM_SUBSTRING || sj->current_mode == JM_SEARCH) {
        disconnect_key_press_action(sj);
        disconnect_click_action(sj);
    }

    duplicate_end(sj);
}

static void duplicate_complete(ShortcutJump *sj) {
    ui_set_statusbar(TRUE, _("Selection duplication completed."));

    scintilla_send_message(sj->sci, SCI_ENDUNDOACTION, 0, 0);

    annotation_clear(sj->sci, sj->eol_message_line);

    duplicate_end(sj);
}

void multicursor_duplicate_cancel(ShortcutJump *sj) {
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

static void multicursor_duplicate_complete(ShortcutJump *sj) {
    scintilla_send_message(sj->sci, SCI_ENDUNDOACTION, 0, 0);

    annotation_clear(sj->sci, sj->eol_message_line);

    toggle_multicursor_menu(sj, FALSE);

    multicursor_end(sj);
    ui_set_statusbar(TRUE, _("Multicursor string duplication completed."));
}

void duplicate_string(ShortcutJump *sj) {
    if (sj->current_mode == JM_SUBSTRING || sj->current_mode == JM_SEARCH) {
        disconnect_key_press_action(sj);
        disconnect_click_action(sj);
    }

    sj->current_mode = JM_DUPLICATE;

    scintilla_send_message(sj->sci, SCI_BEGINUNDOACTION, 0, 0);

    gint chars_added = 0;

    for (gint i = 0; i < sj->words->len; i++) {
        Word word = g_array_index(sj->words, Word, i);

        if (word.valid_search) {
            gint start = sj->first_position + word.replace_pos + chars_added;
            gint len = word.word->len;

            scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_TAG, 0);
            scintilla_send_message(sj->sci, SCI_INDICATORCLEARRANGE, start, len);
            scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_HIGHLIGHT, 0);
            scintilla_send_message(sj->sci, SCI_INDICATORCLEARRANGE, start, len);
            scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_TEXT, 0);
            scintilla_send_message(sj->sci, SCI_INDICATORCLEARRANGE, start, len);

            scintilla_send_message(sj->sci, SCI_INSERTTEXT, word.starting_doc + word.word->len + chars_added,
                                   (sptr_t)word.word->str);

            chars_added += word.word->len;
        }
    }

    duplicate_complete(sj);
}

void duplicate_string_for_multicursor(ShortcutJump *sj) {
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

    multicursor_duplicate_complete(sj);
}
