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
#include "replace_handle_input.h"
#include "selection.h"
#include "shortcut_common.h"
#include "util.h"
#include "values.h"

void shortcut_char_jumping_cancel(ShortcutJump *sj) {
    shortcut_set_to_first_visible_line(sj);
    scintilla_send_message(sj->sci, SCI_SETREADONLY, 0, 0);
    scintilla_send_message(sj->sci, SCI_DELETERANGE, sj->first_position, sj->buffer->len);
    scintilla_send_message(sj->sci, SCI_INSERTTEXT, sj->first_position, (sptr_t)sj->cache->str);
    scintilla_send_message(sj->sci, SCI_ENDUNDOACTION, 0, 0);
    scintilla_send_message(sj->sci, SCI_UNDO, 0, 0);
    scintilla_send_message(sj->sci, SCI_GOTOPOS, sj->current_cursor_pos, 0);
    annotation_clear(sj->sci, sj->eol_message_line);
    sj->range_is_set = FALSE;
    shortcut_end(sj, FALSE);
    ui_set_statusbar(TRUE, _("Character jump canceled."));
}

void shortcut_char_jumping_complete(ShortcutJump *sj, gint pos, gint word_length, gint line) {
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

    if (sj->multicursor_enabled == MC_DISABLED) {
        handle_text_after_action(sj, pos, word_length, line);
    }

    if (sj->multicursor_enabled == MC_ACCEPTING) {
        scintilla_send_message(sj->sci, SCI_GOTOPOS, sj->current_cursor_pos, 0);
    }

    shortcut_set_to_first_visible_line(sj);
    annotation_clear(sj->sci, sj->eol_message_line);
    shortcut_end(sj, FALSE);
    ui_set_statusbar(TRUE, _("Character jump completed."));
}

void shortcut_char_waiting_cancel(ShortcutJump *sj) {
    annotation_clear(sj->sci, sj->eol_message_line);
    shortcut_end(sj, FALSE);
    ui_set_statusbar(TRUE, _("Character serach canceled."));
}

static void shortcut_char_replacing_end(ShortcutJump *sj) {
    for (gint i = 0; i < sj->words->len; i++) {
        Word word = g_array_index(sj->words, Word, i);
        if (word.valid_search) {
            scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_TAG, 0);
            scintilla_send_message(sj->sci, SCI_INDICATORCLEARRANGE, word.replace_pos + sj->first_position, 1);
        }
    }

    scintilla_send_message(sj->sci, SCI_SETREADONLY, 0, 0);
    scintilla_send_message(sj->sci, SCI_DELETERANGE, sj->first_position, sj->replace_cache->len);
    scintilla_send_message(sj->sci, SCI_INSERTTEXT, sj->first_position, (sptr_t)sj->replace_cache->str);
    scintilla_send_message(sj->sci, SCI_ENDUNDOACTION, 0, 0);
    scintilla_send_message(sj->sci, SCI_GOTOPOS, sj->current_cursor_pos, 0);
}

void shortcut_char_replacing_cancel(ShortcutJump *sj) {
    shortcut_set_to_first_visible_line(sj);
    shortcut_char_replacing_end(sj);
    shortcut_end(sj, FALSE);
    ui_set_statusbar(TRUE, _("Character replacement canceled."));
}

void shortcut_char_replacing_complete(ShortcutJump *sj) {
    shortcut_set_to_first_visible_line(sj);
    shortcut_char_replacing_end(sj);
    shortcut_end(sj, FALSE);
    ui_set_statusbar(TRUE, _("Character replacement completed (%i change%s made)."), sj->words->len,
                     sj->words->len == 1 ? "" : "s");
}

static void shortcut_char_get_chars(ShortcutJump *sj, gchar query) {
    gint lfs_added = 0;
    gint toggle = 1;
    gint added = 0;
    gint prev_line;
    gchar prev_char;

    if (sj->in_selection && sj->config_settings->search_from_selection) {
        prev_line = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, sj->first_position, 0);
    } else {
        prev_line = sj->first_line_on_screen - 1;
    }

    if (sj->delete_added_bracket) {
        scintilla_send_message(sj->sci, SCI_DELETERANGE, sj->current_cursor_pos, 1);
        sj->current_cursor_pos = scintilla_send_message(sj->sci, SCI_GETCURRENTPOS, 0, 0);
        scintilla_send_message(sj->sci, SCI_SETCURRENTPOS, sj->current_cursor_pos, 0);
        sj->delete_added_bracket = FALSE;
    }

    for (gint i = sj->first_position; i < sj->last_position; i++) {
        if (added == shortcut_get_max_words(sj)) {
            break;
        }

        gchar current_char = scintilla_send_message(sj->sci, SCI_GETCHARAT, i, TRUE);

        if (current_char != query) {
            continue;
        }

        gchar char_to_the_left = scintilla_send_message(sj->sci, SCI_GETCHARAT, i - 1, TRUE);

        if (char_to_the_left != prev_char) {
            toggle = 1;
        }

        Word word;
        gboolean ignore_hidden_neighbor_skip = FALSE;

        if (sj->config_settings->shortcuts_include_single_char && sj->words->len <= 26) {
            ignore_hidden_neighbor_skip = TRUE;
        }

        if (current_char == query && prev_char == current_char && toggle == 0 && !ignore_hidden_neighbor_skip) {
            GString *ch = g_string_new("");
            g_string_insert_c(ch, 0, current_char);
            word.word = ch;
            word.valid_search = TRUE;
            word.is_hidden_neighbor = TRUE;
            word.starting = i + lfs_added;
            word.starting_doc = i;
            word.bytes = shortcut_get_utf8_char_length(word.word->str[0]);
            word.line = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, i, 0);
            word.padding = shortcut_set_padding(sj, word.word->len);
            word.replace_pos = i - sj->first_position;
            g_array_append_val(sj->words, word);
            toggle ^= 1;
            continue;
        }

        GString *ch = g_string_new("");
        g_string_insert_c(ch, 0, current_char);
        word.word = ch;
        word.valid_search = TRUE;
        word.is_hidden_neighbor = FALSE;
        word.starting = i + lfs_added;
        word.starting_doc = i;
        word.bytes = shortcut_get_utf8_char_length(word.word->str[0]);
        word.line = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, i, 0);
        word.padding = shortcut_set_padding(sj, word.word->len);
        word.replace_pos = i - sj->first_position;
        word.shortcut = shortcut_make_tag(sj, added++);
        toggle ^= 1;

        gchar line_ending_char = scintilla_send_message(sj->sci, SCI_GETCHARAT, i + 1, TRUE);

        if (line_ending_char == '\n' && word.shortcut->len == 2) {
            g_string_insert_c(sj->buffer, lfs_added + i - sj->first_position, '\n');
            gint line = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, i, 0);

            if (line != prev_line) {
                for (gint i = prev_line; i < line; i++) {
                    g_array_append_val(sj->lf_positions, lfs_added);
                }

                prev_line = line;
            }

            lfs_added += 1;
        }

        g_array_append_val(sj->words, word);
        prev_char = current_char;
    }

    for (gint i = prev_line; i < sj->last_line_on_screen; i++) {
        g_array_append_val(sj->lf_positions, lfs_added);
    }

    sj->search_results_count = sj->words->len;
}

static gboolean shortcut_char_on_click_event(GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;

    if (mouse_movement_performed(sj, event)) {
        if (sj->current_mode == JM_SHORTCUT_CHAR_ACCEPTING) {
            shortcut_char_waiting_cancel(sj);
        } else if (sj->current_mode == JM_SHORTCUT_CHAR_JUMPING) {
            sj->current_cursor_pos = scintilla_send_message(sj->sci, SCI_GETCURRENTPOS, 0, 0);
            sj->current_cursor_pos = set_cursor_position_with_lfs(sj);
            shortcut_char_jumping_cancel(sj);
        } else if (sj->current_mode == JM_SHORTCUT_CHAR_REPLACING) {
            sj->current_cursor_pos = scintilla_send_message(sj->sci, SCI_GETCURRENTPOS, 0, 0);
            shortcut_char_replacing_cancel(sj);
        }

        return TRUE;
    }

    return FALSE;
}

static gboolean shortcut_char_on_key_press(GtkWidget *widget, GdkEventKey *event, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;
    gunichar query = gdk_keyval_to_unicode(event->keyval);

    if (sj->current_mode == JM_SHORTCUT_CHAR_ACCEPTING) {
        if (mod_key_pressed(event)) {
            return TRUE;
        }

        shortcut_char_get_chars(sj, query);

        if (sj->words->len == 0) {
            shortcut_char_waiting_cancel(sj);
            annotation_clear(sj->sci, sj->eol_message_line);
            return FALSE;
        }

        sj->buffer = shortcut_mask_bytes(sj->words, sj->buffer, sj->first_position);
        sj->buffer = shortcut_set_tags_in_buffer(sj->words, sj->buffer, sj->first_position);
        sj->current_cursor_pos = scintilla_send_message(sj->sci, SCI_GETCURRENTPOS, 0, 0);
        shortcut_set_after_placement(sj);
        shortcut_set_indicators(sj);
        sj->current_mode = JM_SHORTCUT_CHAR_JUMPING;
        ui_set_statusbar(TRUE, _("%i character%s in view."), sj->words->len, sj->words->len == 1 ? "" : "s");

        annotation_display_char_search(sj);
        return TRUE;
    } else if (sj->current_mode == JM_SHORTCUT_CHAR_JUMPING) {
        annotation_clear(sj->sci, sj->eol_message_line);
        return shortcut_on_key_press_action(event, sj);
    } else if (sj->current_mode == JM_SHORTCUT_CHAR_REPLACING) {
        annotation_clear(sj->sci, sj->eol_message_line);
        return replace_handle_input(sj, event, query);
    }

    return FALSE;
}

void shortcut_char_init_with_query(ShortcutJump *sj, gchar query) {
    sj->current_mode = JM_SHORTCUT_CHAR_ACCEPTING;
    set_sj_scintilla_object(sj);
    init_sj_values(sj);
    define_indicators(sj->sci, sj);

    if (sj->in_selection) {
        if (sj->selection_is_a_char && sj->config_settings->use_selected_word_or_char) {
            shortcut_char_get_chars(sj, query);

            sj->current_mode = JM_SHORTCUT_CHAR_JUMPING;
            ui_set_statusbar(TRUE, _("%i character%s in view."), sj->words->len, sj->words->len == 1 ? "" : "s");
        }
    } else {
        annotation_display_shortcut_char(sj);
    }

    connect_key_press_action(sj, shortcut_char_on_key_press);
    connect_click_action(sj, shortcut_char_on_click_event);
}

void shortcut_char_init(ShortcutJump *sj) {
    sj->current_mode = JM_SHORTCUT_CHAR_ACCEPTING;
    set_sj_scintilla_object(sj);
    set_selection_info(sj);
    init_sj_values(sj);
    define_indicators(sj->sci, sj);

    if (sj->in_selection) {
        if (sj->selection_is_a_char && sj->config_settings->use_selected_word_or_char) {
            gchar query = scintilla_send_message(sj->sci, SCI_GETCHARAT, sj->selection_start, sj->selection_end);
            shortcut_char_get_chars(sj, query);

            sj->buffer = shortcut_mask_bytes(sj->words, sj->buffer, sj->first_position);
            sj->buffer = shortcut_set_tags_in_buffer(sj->words, sj->buffer, sj->first_position);
            sj->current_cursor_pos = scintilla_send_message(sj->sci, SCI_GETCURRENTPOS, 0, 0);
            shortcut_set_after_placement(sj);
            shortcut_set_indicators(sj);

            sj->current_mode = JM_SHORTCUT_CHAR_JUMPING;
            ui_set_statusbar(TRUE, _("%i character%s in view."), sj->words->len, sj->words->len == 1 ? "" : "s");
        }
    } else {
        annotation_display_shortcut_char(sj);
    }

    connect_key_press_action(sj, shortcut_char_on_key_press);
    connect_click_action(sj, shortcut_char_on_click_event);
}

void shortcut_char_cb(GtkMenuItem *menu_item, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;
    if (sj->current_mode == JM_NONE) {
        shortcut_char_init(sj);
    }
}

gboolean shortcut_char_kb(GeanyKeyBinding *kb, guint key_id, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;
    if (sj->current_mode == JM_NONE) {
        shortcut_char_init(sj);
        return TRUE;
    }
    return TRUE;
}
