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
#include "replace_handle_input.h"
#include "search_substring.h"
#include "search_word.h"
#include "shortcut_char.h"

void paste_get_clipboard_text(ShortcutJump *sj) {
    GtkClipboard *clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);

    sj->clipboard_text = gtk_clipboard_wait_for_text(clipboard);

    if (sj->clipboard_text == NULL) {
        sj->clipboard_text = g_strdup("");
        sj->inserting_clipboard = FALSE;
    } else {
        sj->inserting_clipboard = TRUE;
    }
}

static void paste_insert_clipboard_text(ShortcutJump *sj) {
    gint chars_added = 0;
    gint c = 0;
    gint clipboard_text_len = strlen(sj->clipboard_text);

    scintilla_send_message(sj->sci, SCI_DELETERANGE, sj->first_position, sj->replace_cache->len);

    for (gint i = 0; i < sj->words->len; i++) {
        Word *word = &g_array_index(sj->words, Word, i);

        if (word->valid_search) {
            gint t = word->replace_pos;

            word->replace_pos += chars_added;

            gint insert_pos = word->replace_pos + sj->replace_len;
            g_string_insert(sj->replace_cache, insert_pos, sj->clipboard_text);
            chars_added += clipboard_text_len;

            if (t + sj->first_position < sj->current_cursor_pos) {
                c = chars_added;
            }
        }
    }

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

    sj->current_cursor_pos += c;

    if (sj->cursor_in_word) {
        scintilla_send_message(sj->sci, SCI_GOTOPOS, sj->current_cursor_pos + sj->replace_len + clipboard_text_len, 0);
    } else {
        scintilla_send_message(sj->sci, SCI_GOTOPOS, sj->current_cursor_pos, 0);
    }

    g_string_append(sj->replace_query, sj->clipboard_text);

    sj->replace_len += clipboard_text_len;
    sj->search_change_made = TRUE;
}

gboolean on_paste_key_release_replace(GtkWidget *widget, GdkEventKey *event, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;

    if (sj->inserting_clipboard) {
        if (sj->config_settings->replace_action == RA_REPLACE && !sj->search_change_made) {
            clear_occurances(sj);
        }
        paste_insert_clipboard_text(sj);
        sj->inserting_clipboard = FALSE;
        g_signal_handler_disconnect(sj->sci, sj->paste_key_release_id);
    }

    return TRUE;
}

gboolean on_paste_key_release_word_search(GtkWidget *widget, GdkEventKey *event, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;

    if (sj->inserting_clipboard) {
        g_string_append(sj->search_query, sj->clipboard_text);

        search_word_mark_words(sj, FALSE);
        annotation_display_search(sj);

        sj->inserting_clipboard = FALSE;
        g_signal_handler_disconnect(sj->sci, sj->paste_key_release_id);
    }

    return TRUE;
}

gboolean on_paste_key_release_substring_search(GtkWidget *widget, GdkEventKey *event, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;

    if (sj->inserting_clipboard) {
        g_string_append(sj->search_query, sj->clipboard_text);

        search_substring_get_substrings(sj);
        annotation_display_substring(sj);

        sj->inserting_clipboard = FALSE;
        g_signal_handler_disconnect(sj->sci, sj->paste_key_release_id);
    }

    return TRUE;
}
