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

#include "action_line_after.h"
#include "jump_to_a_word.h"
#include "selection.h"
#include "shortcut_common.h"
#include "util.h"
#include "values.h"

void shortcut_line_complete(ShortcutJump *sj, gint pos, gint word_length, gint line) {
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

    shortcut_line_handle_after_action(sj, line);
    shortcut_set_to_first_visible_line(sj);
    shortcut_end(sj, FALSE);
    ui_set_statusbar(TRUE, _("Line jump completed."));
}

void shortcut_line_cancel(ShortcutJump *sj) {
    scintilla_send_message(sj->sci, SCI_SETREADONLY, 0, 0);
    scintilla_send_message(sj->sci, SCI_DELETERANGE, sj->first_position, sj->buffer->len);
    scintilla_send_message(sj->sci, SCI_INSERTTEXT, sj->first_position, (sptr_t)sj->cache->str);
    scintilla_send_message(sj->sci, SCI_ENDUNDOACTION, 0, 0);
    scintilla_send_message(sj->sci, SCI_GOTOPOS, sj->current_cursor_pos, 0);
    shortcut_set_to_first_visible_line(sj);
    sj->range_is_set = FALSE;
    shortcut_end(sj, TRUE);
    ui_set_statusbar(TRUE, _("Line jump canceled."));
}

static gboolean shortcut_line_on_key_press(GtkWidget *widget, GdkEventKey *event, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;
    return shortcut_on_key_press_action(event, sj);
}

static gboolean shortcut_line_on_click_event(GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;
    if (mouse_movement_performed(sj, event)) {
        sj->current_cursor_pos = scintilla_send_message(sj->sci, SCI_GETCURRENTPOS, 0, 0);
        sj->current_cursor_pos = set_cursor_position_with_lfs(sj);
        shortcut_line_cancel(sj);
        return TRUE;
    }
    return FALSE;
}

void shortcut_line_init(ShortcutJump *sj) {
    sj->current_mode = JM_LINE;
    set_sj_scintilla_object(sj);
    set_selection_info(sj);
    init_sj_values(sj);
    define_indicators(sj->sci, sj);

    gint lfs_added = 0;
    gint prev_line = sj->first_line_on_screen;
    gint indent_width = get_indent_width() - 1;

    for (gint current_line = sj->first_line_on_screen; current_line < sj->last_line_on_screen; current_line++) {
        if (sj->words->len == shortcut_get_max_words(sj)) {
            break;
        }

        gint pos = scintilla_send_message(sj->sci, SCI_POSITIONFROMLINE, current_line, TRUE);

        if (pos == sj->last_position) {
            break;
        }

        Word word;

        word.word = g_string_new(sci_get_contents_range(sj->sci, pos, pos + 1));
        word.starting = pos + lfs_added;
        word.starting_doc = pos;
        word.is_hidden_neighbor = FALSE;
        word.bytes = shortcut_get_utf8_char_length(word.word->str[0]);
        word.shortcut = shortcut_make_tag(sj, sj->words->len);
        word.line = current_line;
        word.padding = 0;

        gchar c = scintilla_send_message(sj->sci, SCI_GETCHARAT, pos, TRUE);

        if (c == '\t') {
            for (gint i = 0; i < indent_width; i++) {
                g_string_insert_c(sj->buffer, lfs_added + pos - sj->first_position, ' ');
            }

            lfs_added += indent_width;
        }

        if (word.shortcut->len == 1) {
            gchar first_char_on_line = scintilla_send_message(sj->sci, SCI_GETCHARAT, pos, TRUE);

            if (first_char_on_line == '\n') {
                g_string_insert_c(sj->buffer, lfs_added++ + pos - sj->first_position, ' ');
            }
        }

        if (word.shortcut->len == 2) {
            gchar first_char_on_line = scintilla_send_message(sj->sci, SCI_GETCHARAT, pos, TRUE);
            gchar next_char = scintilla_send_message(sj->sci, SCI_GETCHARAT, pos + 1, TRUE);
            gchar line_of_next_char = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, pos + 1, TRUE);

            if (current_line != line_of_next_char && first_char_on_line == '\n') {
                g_string_insert_c(sj->buffer, lfs_added++ + pos - sj->first_position, ' ');
                g_string_insert_c(sj->buffer, lfs_added++ + pos - sj->first_position, ' ');
            }

            if (next_char == '\n') {
                g_string_insert_c(sj->buffer, lfs_added++ + pos - sj->first_position, ' ');
            }
        }

        g_array_append_val(sj->lf_positions, lfs_added);
        g_array_append_val(sj->words, word);
    }

    for (gint i = prev_line; i < sj->last_line_on_screen; i++) {
        g_array_append_val(sj->lf_positions, lfs_added);
    }

    sj->buffer = shortcut_mask_bytes(sj->words, sj->buffer, sj->first_position);
    sj->buffer = shortcut_set_tags_in_buffer(sj->words, sj->buffer, sj->first_position);

    shortcut_set_after_placement(sj);
    shortcut_set_indicators(sj);
    connect_key_press_action(sj, shortcut_line_on_key_press);
    connect_click_action(sj, shortcut_line_on_click_event);
    ui_set_statusbar(TRUE, _("%i line%s in view."), sj->words->len, sj->words->len == 1 ? "" : "s");
}

void shortcut_line_cb(GtkMenuItem *menu_item, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;

    if (sj->current_mode == JM_NONE) {
        shortcut_line_init(sj);
    }
}

gboolean shortcut_line_kb(GeanyKeyBinding *kb, guint key_id, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;
    if (sj->current_mode == JM_NONE) {
        shortcut_line_init(sj);
    }
    return TRUE;
}
