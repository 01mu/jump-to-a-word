#include "multicursor_replace.h"
#include "annotation.h"
#include "jump_to_a_word.h"
#include "multicursor.h"
#include "util.h"

static void multicursor_replace_clear_indicators(ShortcutJump *sj) {
    for (gint i = 0; i < sj->multicursor_words->len; i++) {
        Word word = g_array_index(sj->multicursor_words, Word, i);

        if (word.valid_search) {
            scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_TAG, 0);
            scintilla_send_message(sj->sci, SCI_INDICATORCLEARRANGE, sj->first_position + word.replace_pos,
                                   sj->replace_len == 0 ? word.word->len : sj->replace_len);
            scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_TEXT, 0);
            scintilla_send_message(sj->sci, SCI_INDICATORCLEARRANGE, sj->first_position + word.replace_pos,
                                   sj->replace_len == 0 ? word.word->len : sj->replace_len);
        }
    }
}

static void multicursor_replace_end(ShortcutJump *sj) {
    g_string_free(sj->cache, TRUE);
    g_string_free(sj->buffer, TRUE);
    g_string_free(sj->replace_cache, TRUE);
    g_string_free(sj->replace_query, TRUE);
    g_free(sj->clipboard_text);
    multicursor_end(sj);
}

void multicursor_replace_start(ShortcutJump *sj) {
    gint first_line_on_screen = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, sj->multicursor_first_pos, 0);
    gint last_line_on_screen = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, sj->multicursor_last_pos, 0);
    gint lines_on_screen = last_line_on_screen - first_line_on_screen;

    sj->first_line_on_screen = first_line_on_screen;
    sj->lines_on_screen = lines_on_screen;
    sj->last_line_on_screen = last_line_on_screen;
    sj->first_position = scintilla_send_message(sj->sci, SCI_POSITIONFROMLINE, first_line_on_screen, 0);
    sj->last_position = scintilla_send_message(sj->sci, SCI_GETLINEENDPOSITION, last_line_on_screen, 0);

    gchar *screen_lines;

    if (sj->first_position < sj->last_position) {
        screen_lines = sci_get_contents_range(sj->sci, sj->first_position, sj->last_position);
    } else {
        screen_lines = g_strdup("");
    }

    sj->cache = g_string_new(screen_lines);
    sj->buffer = g_string_new(screen_lines);
    sj->replace_cache = g_string_new(screen_lines);

    g_free(screen_lines);

    sj->replace_query = g_string_new("");
    sj->clipboard_text = g_strdup("");

    gint pos = scintilla_send_message(sj->sci, SCI_GETCURRENTPOS, 0, 0);
    gint line = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, pos, 0);

    sj->multicusor_eol_message_line = line;
    sj->current_cursor_pos = pos;
    sj->search_word_pos = -1;
    sj->search_word_pos_first = -1;
    sj->search_word_pos_last = -1;
    sj->search_change_made = FALSE;
    sj->replace_len = 0;
    sj->replace_instant = FALSE;
    sj->waiting_after_single_instance = FALSE;
    sj->search_results_count = 0;
}

void multicursor_replace_cancel(ShortcutJump *sj) {
    for (gint i = 0; i < sj->multicursor_words->len; i++) {
        Word word = g_array_index(sj->multicursor_words, Word, i);
        scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_MULTICURSOR, 0);
        scintilla_send_message(sj->sci, SCI_INDICATORCLEARRANGE, word.starting, word.word->len);
    }

    scintilla_send_message(sj->sci, SCI_SETREADONLY, 0, 0);
    scintilla_send_message(sj->sci, SCI_ENDUNDOACTION, 0, 0);
    multicursor_replace_clear_indicators(sj);
    annotation_clear(sj->sci, sj->eol_message_line);
    disconnect_key_press_action(sj);
    disconnect_click_action(sj);

    toggle_multicursor_menu(sj, FALSE);

    reset_cached_replace_action(sj);
    multicursor_replace_end(sj);
    ui_set_statusbar(TRUE, _("Multicursor string replacement canceled."));
}

void multicursor_replace_complete(ShortcutJump *sj) {
    ui_set_statusbar(TRUE, _("Multicursor string replacement completed (%i change%s made)."), sj->search_results_count,
                     sj->search_results_count == 1 ? "" : "s");

    if (sj->config_settings->disable_live_replace) {
        scintilla_send_message(sj->sci, SCI_SETTARGETSTART, sj->multicursor_first_pos, 0);
        scintilla_send_message(sj->sci, SCI_SETTARGETEND, sj->multicursor_last_pos, 0);
        scintilla_send_message(sj->sci, SCI_REPLACETARGET, -1, (sptr_t)sj->replace_cache->str);
    }

    scintilla_send_message(sj->sci, SCI_SETREADONLY, 0, 0);
    scintilla_send_message(sj->sci, SCI_ENDUNDOACTION, 0, 0);
    multicursor_replace_clear_indicators(sj);
    annotation_clear(sj->sci, sj->eol_message_line);
    disconnect_key_press_action(sj);
    disconnect_click_action(sj);

    toggle_multicursor_menu(sj, FALSE);

    reset_cached_replace_action(sj);
    multicursor_replace_end(sj);
}

gboolean on_click_event_multicursor_replace(GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;
    if (mouse_movement_performed(sj, event)) {
        gtk_check_menu_item_set_active(sj->multicursor_menu_checkbox, FALSE);
        return TRUE;
    }
    return FALSE;
}
