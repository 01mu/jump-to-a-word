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

#include "annotation.h"
#include "jump_to_a_word.h"
#include "util.h"
#include "values.h"
#include <plugindata.h>

static void enable_multicursor(ShortcutJump *sj) {
    sj->multicursor_enabled = TRUE;

    if (!sj->sci) {
        ScintillaObject *sci = get_scintilla_object();
        sj->sci = sci;
    }

    define_indicators(sj->sci, sj);
    init_sj_values_multicursor(sj);

    sj->multicursor_words = g_array_new(TRUE, FALSE, sizeof(Word));
}

void disable_multicusor(ShortcutJump *sj) {
    sj->multicursor_enabled = FALSE;

    search_clear_indicators(sj->sci, sj->words);
    annotation_clear(sj->sci, sj->eol_message_line);

    scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_MULTICURSOR, 0);

    for (gint i = 0; i < sj->words->len; i++) {
        Word word = g_array_index(sj->words, Word, i);
        scintilla_send_message(sj->sci, SCI_INDICATORCLEARRANGE, word.starting_doc, 1);
    }

    scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_TAG, 0);

    for (gint i = 0; i < sj->words->len; i++) {
        Word word = g_array_index(sj->words, Word, i);
        scintilla_send_message(sj->sci, SCI_INDICATORCLEARRANGE, sj->first_position + word.replace_pos,
                               sj->replace_len + 1);
    }

    for (gint i = 0; i < sj->words->len; i++) {
        Word word = g_array_index(sj->words, Word, i);

        g_string_free(word.word, TRUE);

        if (!word.is_hidden_neighbor) {
            g_string_free(word.shortcut, TRUE);
        }
    }

    g_array_free(sj->words, TRUE);

    sj->current_mode = JM_NONE;
    sj->search_results_count = 0;
    g_string_free(sj->cache, TRUE);
    g_string_free(sj->buffer, TRUE);
    g_string_free(sj->replace_cache, TRUE);

    disconnect_key_press_action(sj);
    disconnect_click_action(sj);
}

/**
 * @brief Provides a menu callback for entering multicursor mode.
 *
 * @param GtkMenuItem *menu_item: (unused)
 * @param gpointer user_data: The plugin data
 */
void multicursor_cb(GtkMenuItem *menu_item, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;

    if (sj->multicursor_enabled) {
        disable_multicusor(sj);
    } else {
        enable_multicursor(sj);
    }

    ui_set_statusbar(TRUE, _("Multicursor mode %s."), sj->multicursor_enabled ? "enabled" : "disabled");
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

    if (sj->multicursor_enabled) {
        disable_multicusor(sj);
    } else {
        enable_multicursor(sj);
    }

    ui_set_statusbar(TRUE, _("Multicursor mode %s."), sj->multicursor_enabled ? "enabled" : "disabled");

    return TRUE;
}
