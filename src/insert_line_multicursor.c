#include "annotation.h"
#include "insert_line_common.h"
#include "jump_to_a_word.h"
#include "multicursor.h"
#include "paste.h"
#include "replace_instant.h"
#include "util.h"

void multicursor_line_insert_end(ShortcutJump *sj) {
    for (gint i = 0; i < sj->words->len; i++) {
        Word word = g_array_index(sj->words, Word, i);
        g_string_free(word.word, TRUE);
    }

    g_array_free(sj->words, TRUE);
    g_string_free(sj->cache, TRUE);
    g_string_free(sj->replace_cache, TRUE);
    g_free(sj->clipboard_text);

    sj->current_mode = JM_NONE;
    sj->multicursor_mode = MC_DISABLED;
}

void multicursor_line_insert_cancel(ShortcutJump *sj) {
    ui_set_statusbar(TRUE, _("Multicursor line insertion canceled."));

    line_insert_done_common(sj);

    toggle_multicursor_menu(sj, FALSE);

    multicursor_line_insert_end(sj);
    reset_cached_replace_action(sj);
    multicursor_end(sj);
}

void multicursor_line_insert_complete(ShortcutJump *sj) {
    ui_set_statusbar(TRUE, _("Multicursor line insertion completed (%i change%s made)."), sj->search_results_count,
                     sj->search_results_count == 1 ? "" : "s");

    line_insert_done_common(sj);

    toggle_multicursor_menu(sj, FALSE);

    multicursor_line_insert_end(sj);
    reset_cached_replace_action(sj);
    multicursor_end(sj);
}

static gboolean on_click_event_multicursor_line_insert(GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;

    if (mouse_movement_performed(sj, event)) {
        sj->current_cursor_pos = scintilla_send_message(sj->sci, SCI_GETCURRENTPOS, 0, 0);
        scintilla_send_message(sj->sci, SCI_SETCURRENTPOS, sj->current_cursor_pos, 0);
        multicursor_line_insert_cancel(sj);
        return TRUE;
    }

    return FALSE;
}

void line_insert_from_multicursor_init(ShortcutJump *sj) {
    move_to_end_of_line(sj);

    scintilla_send_message(sj->sci, SCI_SETREADONLY, 0, 0);
    scintilla_send_message(sj->sci, SCI_BEGINUNDOACTION, 0, 0);

    GArray *anchors = sj->multicursor_words;
    gint valid_count = 0;

    for (gint i = 0; i < anchors->len; i++) {
        Word word = g_array_index(anchors, Word, i);
        valid_count += word.valid_search ? 1 : 0;
    }

    GArray *dummy_lines = g_array_new(TRUE, FALSE, sizeof(Word));

    sj->words = dummy_lines;

    sj->cache = g_string_new("");
    sj->replace_cache = g_string_new("");
    sj->clipboard_text = g_strdup("");

    connect_key_press_action(sj, on_key_press_search_replace);
    connect_click_action(sj, on_click_event_multicursor_line_insert);

    if (valid_count == 0) {
        multicursor_line_insert_cancel(sj);
        return;
    }

    sj->search_word_pos = -1;
    sj->search_word_pos_first = -1;
    sj->search_word_pos_last = -1;
    sj->search_change_made = FALSE;
    sj->replace_len = 0;
    sj->replace_instant = FALSE;
    sj->waiting_after_single_instance = FALSE;
    sj->search_results_count = 0;

    GArray *unique_lines = g_array_new(FALSE, FALSE, sizeof(LST));

    line_insert_common(sj, unique_lines, dummy_lines, anchors);
    g_array_free(unique_lines, TRUE);

    gint pos = scintilla_send_message(sj->sci, SCI_GETCURRENTPOS, 0, 0);
    gint line = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, pos, 0);

    sj->multicusor_eol_message_line = line;
    sj->current_cursor_pos = pos;

    sj->current_mode = JM_INSERTING_LINE_MULTICURSOR;
    annotation_display_inserting_line_multicursor(sj);

    paste_get_clipboard_text(sj);
}
