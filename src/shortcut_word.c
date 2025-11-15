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

#include "action_text_after.h"
#include "annotation.h"
#include "jump_to_a_word.h"
#include "selection.h"
#include "shortcut_common.h"
#include "util.h"
#include "values.h"

void shortcut_word_complete(ShortcutJump *sj, gint pos, gint word_length, gint line) {
    scintilla_send_message(sj->sci, SCI_SETREADONLY, 0, 0);
    scintilla_send_message(sj->sci, SCI_DELETERANGE, sj->first_position, sj->buffer->len);
    scintilla_send_message(sj->sci, SCI_INSERTTEXT, sj->first_position, (sptr_t)sj->cache->str);
    scintilla_send_message(sj->sci, SCI_ENDUNDOACTION, 0, 0);
    scintilla_send_message(sj->sci, SCI_UNDO, 0, 0);

    sj->previous_cursor_pos = sj->current_cursor_pos;

    if (sj->config_settings->move_marker_to_line) {
        GeanyDocument *doc = document_get_current();
        if (!doc->is_valid) {
            exit(1);
        } else {
            navqueue_goto_line(doc, doc, line + 1);
        }
    }

    if (sj->multicursor_mode == MC_DISABLED) {
        handle_text_after_action(sj, pos, word_length, line);
    }

    if (sj->multicursor_mode == MC_ACCEPTING) {
        scintilla_send_message(sj->sci, SCI_GOTOPOS, sj->current_cursor_pos, 0);
    }

    shortcut_set_to_first_visible_line(sj);
    annotation_clear(sj->sci, sj->eol_message_line);
    margin_markers_reset(sj);
    disconnect_key_press_action(sj);
    disconnect_click_action(sj);
    shortcut_end(sj, FALSE);

    if (sj->multicursor_mode == MC_ACCEPTING) {
        scintilla_send_message(sj->sci, SCI_SETREADONLY, 1, 0);
    }

    ui_set_statusbar(TRUE, _("Word jump completed."));
}

void shortcut_word_cancel(ShortcutJump *sj) {
    scintilla_send_message(sj->sci, SCI_SETREADONLY, 0, 0);
    scintilla_send_message(sj->sci, SCI_DELETERANGE, sj->first_position, sj->buffer->len);
    scintilla_send_message(sj->sci, SCI_INSERTTEXT, sj->first_position, (sptr_t)sj->cache->str);
    scintilla_send_message(sj->sci, SCI_ENDUNDOACTION, 0, 0);
    scintilla_send_message(sj->sci, SCI_UNDO, 0, 0);
    scintilla_send_message(sj->sci, SCI_GOTOPOS, sj->current_cursor_pos, 0);
    shortcut_set_to_first_visible_line(sj);
    annotation_clear(sj->sci, sj->eol_message_line);
    sj->range_is_set = FALSE;
    margin_markers_reset(sj);
    disconnect_key_press_action(sj);
    disconnect_click_action(sj);
    shortcut_end(sj, FALSE);
    ui_set_statusbar(TRUE, _("Word jump canceled."));
}

static gboolean shortcut_word_on_key_press(GtkWidget *widget, GdkEventKey *event, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;
    return shortcut_on_key_press_action(event, sj);
}

static gboolean shortcut_word_on_click_event(GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;
    if (mouse_movement_performed(sj, event)) {
        sj->current_cursor_pos = scintilla_send_message(sj->sci, SCI_GETCURRENTPOS, 0, 0);
        sj->current_cursor_pos = set_cursor_position_with_lfs(sj);
        shortcut_word_cancel(sj);
        return TRUE;
    }
    return FALSE;
}

static GString *shortcut_word_hide_word(const ShortcutJump *sj, GArray *words, GString *buffer, gint first_position) {
    for (gint i = 0; i < words->len; i++) {
        Word word = g_array_index(words, Word, i);
        gint starting = word.starting - first_position;

        for (gint j = 0; j < word.word->len; j++) {
            buffer->str[starting + j] = ' ';
        }
    }

    return buffer;
}

void shortcut_word_init(ShortcutJump *sj) {
    sj->current_mode = JM_SHORTCUT_WORD;
    set_sj_scintilla_object(sj);
    set_selection_info(sj);
    init_sj_values(sj);
    define_indicators(sj->sci, sj);

    gint prev_line;

    if (sj->in_selection && sj->config_settings->search_from_selection) {
        prev_line = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, sj->first_position, 0);
    } else {
        prev_line = sj->first_line_on_screen - 1;
    }

    gint lfs_added = 0;

    for (gint i = sj->first_position; i < sj->last_position; i++) {
        if (sj->words->len == shortcut_get_max_words(sj)) {
            break;
        }

        gint start = scintilla_send_message(sj->sci, SCI_WORDSTARTPOSITION, i, TRUE);
        gint end = scintilla_send_message(sj->sci, SCI_WORDENDPOSITION, i, TRUE);

        if (start == end || start < sj->first_position || end > sj->last_position) {
            continue;
        }

        Word word;

        word.word = g_string_new(sci_get_contents_range(sj->sci, start, end));
        word.starting = start + lfs_added;
        word.starting_doc = start;
        word.is_hidden_neighbor = FALSE;
        word.bytes = shortcut_get_utf8_char_length(word.word->str[0]);
        word.shortcut = shortcut_make_tag(sj, sj->words->len);
        word.line = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, start, 0);
        word.padding = shortcut_set_padding(sj, word.word->len);

        gchar line_ending_char = scintilla_send_message(sj->sci, SCI_GETCHARAT, end, TRUE);

        if (line_ending_char == '\n' && word.word->len == 1 && word.shortcut->len == 2) {
            g_string_insert_c(sj->buffer, lfs_added + end - sj->first_position, '\n');

            gint line = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, i, 0);

            if (line != prev_line) {
                for (gint j = prev_line; j < line; j++) {
                    g_array_append_val(sj->lf_positions, lfs_added);
                }

                prev_line = line;
            }

            lfs_added++;
        }

        g_array_append_val(sj->words, word);
        i += word.word->len;
    }

    for (gint i = prev_line; i < sj->last_line_on_screen; i++) {
        g_array_append_val(sj->lf_positions, lfs_added);
    }

    sj->buffer = shortcut_mask_bytes(sj->words, sj->buffer, sj->first_position);

    if (sj->config_settings->hide_word_shortcut_jump) {
        sj->buffer = shortcut_word_hide_word(sj, sj->words, sj->buffer, sj->first_position);
    }

    sj->buffer = shortcut_set_tags_in_buffer(sj->words, sj->buffer, sj->first_position);

    shortcut_set_after_placement(sj);
    shortcut_set_indicators(sj);
    connect_key_press_action(sj, shortcut_word_on_key_press);
    connect_click_action(sj, shortcut_word_on_click_event);
    ui_set_statusbar(TRUE, _("%i word%s in view."), sj->words->len, sj->words->len == 1 ? "" : "s");
}

void shortcut_word_cb(GtkMenuItem *menu_item, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;

    if (sj->current_mode == JM_NONE) {
        shortcut_word_init(sj);
    }
}

gboolean shortcut_word_kb(GeanyKeyBinding *kb, guint key_id, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;

    if (sj->current_mode == JM_NONE) {
        shortcut_word_init(sj);
        return TRUE;
    }

    return FALSE;
}
