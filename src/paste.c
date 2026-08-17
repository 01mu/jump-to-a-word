#include "annotation.h"
#include "jump_to_a_word.h"
#include "replace_handle_input.h"
#include "search_substring.h"
#include "search_word.h"

void paste_get_clipboard_text(ShortcutJump *sj) {
    GtkClipboard *clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    gchar *clipboard_text = gtk_clipboard_wait_for_text(clipboard);

    if (clipboard_text) {
        g_free(sj->clipboard_text);
        sj->clipboard_text = clipboard_text;
        sj->inserting_clipboard = TRUE;
    }
}

static void paste_insert_clipboard_text(ShortcutJump *sj) {
    gint chars_added = 0;
    gint c = -1;
    gint prev;
    gint clipboard_text_len = strlen(sj->clipboard_text);

    if (!sj->config_settings->disable_live_replace) {
        scintilla_send_message(sj->sci, SCI_DELETERANGE, sj->first_position, sj->replace_cache->len);
    }

    for (gint i = 0; i < sj->words->len; i++) {
        Word *word = &g_array_index(sj->words, Word, i);

        if (word->valid_search) {
            gint v = chars_added + sj->cursor_moved_to_eol;

            word->replace_pos += chars_added;
            g_string_insert(sj->replace_cache, word->replace_pos + sj->replace_len, sj->clipboard_text);
            chars_added += clipboard_text_len;

            if (c == -1 && word->replace_pos + sj->first_position > v) {
                c = prev;
            } else {
                prev = chars_added;
            }
        }
    }

    if (c == -1) {
        c = chars_added;
    }

    if (!sj->config_settings->disable_live_replace) {
        scintilla_send_message(sj->sci, SCI_INSERTTEXT, sj->first_position, (sptr_t)sj->replace_cache->str);

        for (gint i = 0; i < sj->words->len; i++) {
            Word word = g_array_index(sj->words, Word, i);

            if (word.valid_search) {
                gint start = sj->first_position + word.replace_pos;
                gint len = sj->replace_len + clipboard_text_len;

                scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_TAG, 0);
                scintilla_send_message(sj->sci, SCI_INDICATORCLEARRANGE, start, len);
                scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_TAG, 0);
                scintilla_send_message(sj->sci, SCI_INDICATORFILLRANGE, start, len);
            }
        }

        sj->cursor_moved_to_eol += c;
    }

    scintilla_send_message(sj->sci, SCI_GOTOPOS, sj->cursor_moved_to_eol, 0);

    g_string_append(sj->replace_query, sj->clipboard_text);
    sj->replace_len += clipboard_text_len;
    sj->search_change_made = TRUE;

    if (sj->config_settings->disable_live_replace) {
        annotation_display_replace_string(sj);
    }
}

gboolean on_paste_key_release_replace(GtkWidget *widget, GdkEventKey *event, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;

    if (sj->config_settings->replace_action == RA_REPLACE && !sj->search_change_made) {
        clear_occurrences(sj);
    }

    paste_insert_clipboard_text(sj);
    sj->inserting_clipboard = FALSE;
    g_signal_handler_disconnect(sj->sci, sj->paste_key_release_id);
    return TRUE;
}

gboolean on_paste_key_release_word_search(GtkWidget *widget, GdkEventKey *event, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;
    g_string_append(sj->search_query, sj->clipboard_text);
    search_word_mark_words(sj, FALSE);
    annotation_display_search(sj);
    sj->inserting_clipboard = FALSE;
    g_signal_handler_disconnect(sj->sci, sj->paste_key_release_id);
    return TRUE;
}

gboolean on_paste_key_release_substring_search(GtkWidget *widget, GdkEventKey *event, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;
    g_string_append(sj->search_query, sj->clipboard_text);
    search_substring_get_substrings(sj);
    annotation_display_substring(sj);
    sj->inserting_clipboard = FALSE;
    g_signal_handler_disconnect(sj->sci, sj->paste_key_release_id);
    return TRUE;
}
