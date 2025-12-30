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

#include "jump_to_a_word.h"
#include "search_substring.h"
#include "search_word.h"
#include "selection.h"
#include "shortcut_char.h"
#include "util.h"
#include "values.h"

static void replace(ShortcutJump *sj) {
    gint chars_added = 0;
    gint chars_removed = 0;
    gint c = 0;
    scintilla_send_message(sj->sci, SCI_DELETERANGE, sj->first_position, sj->replace_cache->len);
    for (gint i = 0; i < sj->words->len; i++) {
        Word *word = &g_array_index(sj->words, Word, i);
        if (word->valid_search) {
            gint new_pos = word->replace_pos - chars_removed + chars_added;
            gint t = word->replace_pos;

            if (sj->previous_replace_action == RA_REPLACE) {
                g_string_erase(sj->replace_cache, new_pos, word->word->len);
                chars_removed += word->word->len;
            } else if (sj->previous_replace_action == RA_INSERT_END) {
                new_pos += word->word->len;
            }

            g_string_insert_len(sj->replace_cache, new_pos, sj->previous_replace_query->str,
                                sj->previous_replace_query->len);

            chars_added += sj->previous_replace_query->len;

            if (t + sj->first_position < sj->current_cursor_pos) {
                c = chars_added - chars_removed;
            }
        }
    }
    scintilla_send_message(sj->sci, SCI_INSERTTEXT, sj->first_position, (sptr_t)sj->replace_cache->str);
    sj->current_cursor_pos += c;

    scintilla_send_message(sj->sci, SCI_GOTOPOS, sj->current_cursor_pos, 0);
}

static void repeat_action(gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;

    if (!(sj->config_settings->replace_action == RA_REPLACE || sj->config_settings->replace_action == RA_INSERT_START ||
          sj->config_settings->replace_action == RA_INSERT_END)) {
        ui_set_statusbar(TRUE, _("Invalid replace mode."));
        return;
    }

    if (sj->multicursor_mode != MC_DISABLED) {
        ui_set_statusbar(TRUE, _("Cannot repeat the previous action while in multicursor mode."));
        return;
    }

    if (!sj->has_previous_action) {
        ui_set_statusbar(TRUE, _("No previous action to repeat."));
        return;
    }

    sj->sci = get_scintilla_object();
    set_selection_info(sj);
    get_view_positions(sj);

    define_indicators(sj->sci, sj->config_settings->tag_color, sj->config_settings->highlight_color,
                      sj->config_settings->text_color);

    gchar *screen_lines = sci_get_contents_range(sj->sci, sj->first_position, sj->last_position);

    sj->replace_cache = g_string_new(screen_lines);
    sj->buffer = g_string_new(screen_lines);
    sj->words = g_array_new(FALSE, FALSE, sizeof(Word));
    sj->search_query = g_string_new(sj->previous_search_query->str);

    sj->current_cursor_pos = scintilla_send_message(sj->sci, SCI_GETCURRENTPOS, 0, 0);

    if (sj->previous_mode == JM_REPLACE_SUBSTRING) {
        search_substring_get_substrings(sj);
        scintilla_send_message(sj->sci, SCI_BEGINUNDOACTION, 0, 0);
        replace(sj);
        scintilla_send_message(sj->sci, SCI_ENDUNDOACTION, 0, 0);
        ui_set_statusbar(TRUE, _("Substring replacement action repeated. Substrings \"%s\" replaced with \"%s\"."),
                         sj->search_query->str, sj->previous_replace_query->str);
    } else if (sj->previous_mode == JM_SHORTCUT_CHAR_REPLACING) {
        sj->lf_positions = g_array_new(FALSE, FALSE, sizeof(Word));
        shortcut_char_get_chars(sj, sj->search_query->str[0]);
        scintilla_send_message(sj->sci, SCI_BEGINUNDOACTION, 0, 0);
        replace(sj);
        scintilla_send_message(sj->sci, SCI_ENDUNDOACTION, 0, 0);
        g_array_free(sj->lf_positions, TRUE);
        ui_set_statusbar(TRUE, _("Character replacement action repeated. Occurances of \"%s\" replaced with \"%s\"."),
                         sj->search_query->str, sj->previous_replace_query->str);
    } else if (sj->previous_mode == JM_REPLACE_SEARCH) {
        search_word_get_words(sj);
        search_word_mark_words(sj, FALSE);
        scintilla_send_message(sj->sci, SCI_BEGINUNDOACTION, 0, 0);
        replace(sj);
        scintilla_send_message(sj->sci, SCI_ENDUNDOACTION, 0, 0);
        ui_set_statusbar(TRUE, _("Word replacement action repeated. Words marked with \"%s\" replaced with \"%s\"."),
                         sj->search_query->str, sj->previous_replace_query->str);
    }

    for (gint i = 0; i < sj->words->len; i++) {
        Word word = g_array_index(sj->words, Word, i);
        g_string_free(word.word, TRUE);
    }

    g_string_free(sj->replace_cache, TRUE);
    g_string_free(sj->buffer, TRUE);
    g_array_free(sj->words, TRUE);
    g_string_free(sj->search_query, TRUE);
}

gboolean repeat_action_kb(GeanyKeyBinding *kb, guint key_id, gpointer user_data) {
    repeat_action(user_data);
    return TRUE;
}

void repeat_action_cb(GtkMenuItem *menu_item, gpointer user_data) { repeat_action(user_data); }
