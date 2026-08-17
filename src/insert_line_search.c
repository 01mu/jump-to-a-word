#include "annotation.h"
#include "insert_line_common.h"
#include "jump_to_a_word.h"
#include "paste.h"
#include "replace_instant.h"
#include "util.h"
#include "values.h"

void line_insert_end(ShortcutJump *sj) {
    for (gint i = 0; i < sj->searched_words_for_line_insert->len; i++) {
        Word word = g_array_index(sj->searched_words_for_line_insert, Word, i);
        g_string_free(word.word, TRUE);
    }

    for (gint i = 0; i < sj->words->len; i++) {
        Word word = g_array_index(sj->words, Word, i);
        g_string_free(word.word, TRUE);
    }

    free_sj_values(sj);
    sj->current_mode = JM_NONE;
}

void line_insert_cancel(ShortcutJump *sj) {
    ui_set_statusbar(TRUE, _("Line insertion canceled."));

    line_insert_done_common(sj);
    reset_cached_replace_action(sj);
    line_insert_end(sj);
}

void line_insert_complete(ShortcutJump *sj) {
    ui_set_statusbar(TRUE, _("Line insertion completed (%i change%s made)."), sj->search_results_count,
                     sj->search_results_count == 1 ? "" : "s");

    line_insert_done_common(sj);
    reset_cached_replace_action(sj);
    line_insert_end(sj);
}

static gboolean on_click_event_line_insert(GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;

    if (mouse_movement_performed(sj, event)) {
        sj->current_cursor_pos = scintilla_send_message(sj->sci, SCI_GETCURRENTPOS, 0, 0);
        scintilla_send_message(sj->sci, SCI_SETCURRENTPOS, sj->current_cursor_pos, 0);
        line_insert_cancel(sj);
        return TRUE;
    }

    return FALSE;
}

void line_insert_from_search_init(ShortcutJump *sj) {
    move_to_end_of_line(sj);

    scintilla_send_message(sj->sci, SCI_SETREADONLY, 0, 0);
    scintilla_send_message(sj->sci, SCI_BEGINUNDOACTION, 0, 0);

    GArray *anchors = sj->words;

    gint valid_count = 0;

    for (gint i = 0; i < anchors->len; i++) {
        Word word = g_array_index(anchors, Word, i);
        valid_count += word.valid_search ? 1 : 0;
    }

    GArray *dummy_lines = g_array_new(TRUE, FALSE, sizeof(Word));

    sj->searched_words_for_line_insert = sj->words;
    sj->words = dummy_lines;

    if (sj->current_mode == JM_SUBSTRING || sj->current_mode == JM_SEARCH) {
        disconnect_key_press_action(sj);
        disconnect_click_action(sj);
    }

    connect_key_press_action(sj, on_key_press_search_replace);
    connect_click_action(sj, on_click_event_line_insert);

    sj->search_results_count = 0;

    if (valid_count == 0) {
        line_insert_cancel(sj);
        return;
    }

    sj->multicursor_first_pos = scintilla_send_message(sj->sci, SCI_GETLENGTH, 0, 0);
    sj->multicursor_last_pos = 0;

    g_string_free(sj->cache, TRUE);
    g_string_free(sj->replace_cache, TRUE);

    sj->cache = g_string_new("");
    sj->replace_cache = g_string_new("");

    GArray *unique_lines = g_array_new(FALSE, FALSE, sizeof(LST));

    line_insert_common(sj, unique_lines, dummy_lines, anchors);
    g_array_free(unique_lines, TRUE);

    sj->current_mode = JM_INSERTING_LINE;
    annotation_display_inserting_line_from_search(sj);

    paste_get_clipboard_text(sj);
}
