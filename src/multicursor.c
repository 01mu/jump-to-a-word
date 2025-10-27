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
#include "util.h"
#include "values.h"

static void multicursor_begin(ShortcutJump *sj) {
    if (!sj->sci) {
        ScintillaObject *sci = get_scintilla_object();
        sj->sci = sci;
    }

    define_indicators(sj->sci, sj);

    sj->multicursor_first_pos = scintilla_send_message(sj->sci, SCI_GETLENGTH, 0, 0);
    sj->multicursor_last_pos = 0;

    sj->multicursor_eol_message = g_string_new("");
    sj->multicursor_words = g_array_new(TRUE, FALSE, sizeof(Word));

    gint pos = scintilla_send_message(sj->sci, SCI_GETCURRENTPOS, 0, 0);
    gint line = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, pos, 0);
    sj->multicusor_eol_message_line = line;
    annotation_display_accepting_multicursor(sj);

    sj->multicursor_enabled = MC_ACCEPTING;

    ui_set_statusbar(TRUE, _("Multicursor mode enabled."));
}

void multicursor_end(ShortcutJump *sj) {
    if (sj->multicursor_enabled == MC_ACCEPTING) {
        annotation_clear(sj->sci, sj->multicusor_eol_message_line);
    }

    if (sj->multicursor_enabled == MC_REPLACING) {
        disconnect_key_press_action(sj);
        g_array_free(sj->lf_positions, TRUE);
    }

    scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_MULTICURSOR, 0);

    for (gint i = 0; i < sj->multicursor_words->len; i++) {
        Word word = g_array_index(sj->multicursor_words, Word, i);
        scintilla_send_message(sj->sci, SCI_INDICATORCLEARRANGE, word.starting_doc, word.word->len);
        g_string_free(word.word, TRUE);
    }

    g_string_free(sj->multicursor_eol_message, TRUE);
    g_array_free(sj->multicursor_words, TRUE);

    sj->current_mode = JM_NONE;
    sj->multicursor_enabled = MC_DISABLED;

    ui_set_statusbar(TRUE, _("Multicursor mode disabled."));
}

void multicursor_cancel(ShortcutJump *sj) {
    annotation_clear(sj->sci, sj->multicusor_eol_message_line);

    scintilla_send_message(sj->sci, SCI_DELETERANGE, sj->first_position, sj->replace_cache->len);
    scintilla_send_message(sj->sci, SCI_INSERTTEXT, sj->first_position, (sptr_t)sj->replace_cache->str);
    scintilla_send_message(sj->sci, SCI_ENDUNDOACTION, 0, 0);

    if (!sj->search_change_made) {
        scintilla_send_message(sj->sci, SCI_UNDO, 0, 0);
    }

    scintilla_send_message(sj->sci, SCI_SETCURRENTPOS, sj->current_cursor_pos, 0);

    g_string_free(sj->cache, TRUE);
    g_string_free(sj->buffer, TRUE);
    g_string_free(sj->replace_cache, TRUE);
    multicursor_end(sj);
}

void multicursor_complete(ShortcutJump *sj) {
    annotation_clear(sj->sci, sj->eol_message_line);
    scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_TAG, 0);

    for (gint i = 0; i < sj->multicursor_words->len; i++) {
        Word word = g_array_index(sj->multicursor_words, Word, i);
        gint start_pos = sj->first_position + word.replace_pos;
        scintilla_send_message(sj->sci, SCI_INDICATORCLEARRANGE, start_pos, sj->replace_len + 1);
    }

    scintilla_send_message(sj->sci, SCI_SETREADONLY, 0, 0);
    scintilla_send_message(sj->sci, SCI_ENDUNDOACTION, 0, 0);
    multicursor_end(sj);
}

/**
 * @brief Provides a menu callback for entering multicursor mode.
 *
 * @param GtkMenuItem *menu_item: (unused)
 * @param gpointer user_data: The plugin data
 */
void multicursor_cb(GtkMenuItem *menu_item, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;

    end_actions(sj);

    if (sj->multicursor_enabled == MC_ACCEPTING || sj->multicursor_enabled == MC_REPLACING) {
        multicursor_end(sj);
    } else if (sj->multicursor_enabled == MC_DISABLED) {
        multicursor_begin(sj);
    }
}

/**
 * @brief Provides a keybinding callback for entering multicursor mode.
 *
 * @param GtkMenuItem *kb: (unused)
 * @param guint key_id: (unused)
 * @param gpointer user_data: The plugin data
 *
 * @return gboolean: TRUE
 */
gboolean multicursor_kb(GeanyKeyBinding *kb, guint key_id, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;

    end_actions(sj);

    if (sj->multicursor_enabled == MC_ACCEPTING || sj->multicursor_enabled == MC_REPLACING) {
        multicursor_end(sj);
    } else if (sj->multicursor_enabled == MC_DISABLED) {
        multicursor_begin(sj);
    }

    return TRUE;
}

void multicursor_add_word(ShortcutJump *sj, Word word) {
    Word multicursor_word;

    multicursor_word.word = g_string_new(word.word->str);
    multicursor_word.starting = word.starting;
    multicursor_word.starting_doc = word.starting_doc;
    multicursor_word.valid_search = TRUE;

    for (gint i = 0; i < sj->multicursor_words->len; i++) {
        Word word = g_array_index(sj->multicursor_words, Word, i);

        if (word.starting_doc == multicursor_word.starting_doc) {
            g_array_remove_index(sj->multicursor_words, i);
            return;
        }
    }

    g_array_append_val(sj->multicursor_words, multicursor_word);
}
