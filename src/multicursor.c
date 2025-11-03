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
    get_view_positions(sj);

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
        scintilla_send_message(sj->sci, SCI_SETCURRENTPOS, sj->current_cursor_pos, 0);
        annotation_clear(sj->sci, sj->multicusor_eol_message_line);
        disconnect_key_press_action(sj);
        disconnect_click_action(sj);
    }

    if ((sj->config_settings->replace_action == RA_INSERT_PREVIOUS_LINE ||
         sj->config_settings->replace_action == RA_INSERT_NEXT_LINE) &&
        sj->multicursor_enabled == MC_REPLACING && !sj->search_change_made) {
        gint lines_removed = 0;
        for (gint i = 0; i < sj->multicursor_lines->len; i++) {
            Word word = g_array_index(sj->multicursor_lines, Word, i);
            gint pos = scintilla_send_message(sj->sci, SCI_POSITIONFROMLINE, word.line - lines_removed, 0);
            scintilla_send_message(sj->sci, SCI_DELETERANGE, pos, 1);
            lines_removed++;
        }
    }

    for (gint i = 0; i < sj->multicursor_words->len; i++) {
        Word word = g_array_index(sj->multicursor_words, Word, i);
        gint start_pos = word.starting_doc;
        gint clear_len = sj->replace_len;
        if (sj->replace_len == 0) {
            clear_len = word.word->len;
        }
        scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_TAG, 0);
        scintilla_send_message(sj->sci, SCI_INDICATORCLEARRANGE, start_pos, clear_len);
        scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_MULTICURSOR, 0);
        scintilla_send_message(sj->sci, SCI_INDICATORCLEARRANGE, start_pos, clear_len);
        g_string_free(word.word, TRUE);
    }

    if ((sj->config_settings->replace_action == RA_INSERT_PREVIOUS_LINE ||
         sj->config_settings->replace_action == RA_INSERT_NEXT_LINE) &&
        sj->multicursor_enabled == MC_REPLACING) {
        for (gint i = 0; i < sj->multicursor_lines->len; i++) {
            Word word = g_array_index(sj->multicursor_lines, Word, i);
            g_string_free(word.word, TRUE);
        }
        g_array_free(sj->multicursor_lines, TRUE);
    }

    g_string_free(sj->multicursor_eol_message, TRUE);
    g_array_free(sj->multicursor_words, TRUE);

    sj->current_mode = JM_NONE;
    sj->multicursor_enabled = MC_DISABLED;
}

void multicursor_cancel(ShortcutJump *sj) {
    ui_set_statusbar(TRUE, _("Multicursor string replacement canceled."));
    annotation_clear(sj->sci, sj->multicusor_eol_message_line);

    scintilla_send_message(sj->sci, SCI_DELETERANGE, sj->first_position, sj->replace_cache->len);
    scintilla_send_message(sj->sci, SCI_INSERTTEXT, sj->first_position, (sptr_t)sj->replace_cache->str);
    scintilla_send_message(sj->sci, SCI_ENDUNDOACTION, 0, 0);

    if (!sj->search_change_made) {
        scintilla_send_message(sj->sci, SCI_UNDO, 0, 0);
    }

    scintilla_send_message(sj->sci, SCI_SETREADONLY, 0, 0);

    scintilla_send_message(sj->sci, SCI_SETCURRENTPOS, sj->current_cursor_pos, 0);

    g_string_free(sj->cache, TRUE);
    g_string_free(sj->buffer, TRUE);
    g_string_free(sj->replace_cache, TRUE);
    multicursor_end(sj);
}

void multicursor_complete(ShortcutJump *sj) {
    if (sj->multicursor_lines) {
        for (gint i = 0; i < sj->multicursor_lines->len; i++) {
            Word word = g_array_index(sj->multicursor_lines, Word, i);

            if (word.valid_search) {
                gint start_pos = sj->first_position + word.replace_pos;
                scintilla_send_message(sj->sci, SCI_INDICATORCLEARRANGE, start_pos, sj->replace_len + 1);
            }
        }
    }

    for (gint i = 0; i < sj->multicursor_words->len; i++) {
        Word word = g_array_index(sj->multicursor_words, Word, i);

        if (word.valid_search) {
            gint start_pos = sj->first_position + word.replace_pos;
            scintilla_send_message(sj->sci, SCI_INDICATORCLEARRANGE, start_pos, sj->replace_len + 1);
        }
    }

    gint changes = sj->search_results_count;

    if (!sj->search_change_made) {
        changes = 0;
    }

    ui_set_statusbar(TRUE, _("Multicursor string replacement completed (%i change%s made)."), changes,
                     changes == 1 ? "" : "s");

    annotation_clear(sj->sci, sj->eol_message_line);
    scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_TAG, 0);

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
        ui_set_statusbar(TRUE, _("Multicursor mode disabled."));
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
        ui_set_statusbar(TRUE, _("Multicursor mode disabled."));
        multicursor_end(sj);
    } else if (sj->multicursor_enabled == MC_DISABLED) {
        multicursor_begin(sj);
    }

    return TRUE;
}

void multicursor_add_word_selection(ShortcutJump *sj, gint start, gint end) {
    Word multicursor_word;

    multicursor_word.word = g_string_new(sci_get_contents_range(sj->sci, start, end));
    multicursor_word.line = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, start, 0);
    multicursor_word.starting = start;
    multicursor_word.starting_doc = start;
    multicursor_word.valid_search = TRUE;

    gint new_word_start = multicursor_word.starting_doc;
    gint new_word_end = multicursor_word.starting_doc + multicursor_word.word->len;

    scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_MULTICURSOR, 0);

    for (gint i = 0; i < sj->multicursor_words->len; i++) {
        Word *word = &g_array_index(sj->multicursor_words, Word, i);

        gint word_start = word->starting_doc;
        gint word_end = word->starting_doc + word->word->len;

        if (word->valid_search && (new_word_start == word_start && new_word_end == word_end)) {
            scintilla_send_message(sj->sci, SCI_INDICATORCLEARRANGE, word->starting_doc, word->word->len);
            word->valid_search = FALSE;
            return;
        }

        if (word->valid_search && ((new_word_start >= word_start && new_word_start <= word_end) ||
                                   (new_word_end >= word_start && new_word_end <= word_end))) {
            scintilla_send_message(sj->sci, SCI_INDICATORCLEARRANGE, word->starting_doc, word->word->len);
            word->valid_search = FALSE;
        }
    }

    scintilla_send_message(sj->sci, SCI_INDICATORFILLRANGE, multicursor_word.starting_doc, multicursor_word.word->len);

    if (multicursor_word.starting_doc <= sj->multicursor_first_pos) {
        sj->multicursor_first_pos = multicursor_word.starting_doc;
    }

    if (new_word_end >= sj->multicursor_last_pos) {
        sj->multicursor_last_pos = new_word_end;
    }

    g_array_append_val(sj->multicursor_words, multicursor_word);
}

void multicursor_add_word(ShortcutJump *sj, Word word) {
    Word multicursor_word;

    multicursor_word.word = g_string_new(word.word->str);
    multicursor_word.line = word.line;
    multicursor_word.starting = word.starting;
    multicursor_word.starting_doc = word.starting_doc;
    multicursor_word.valid_search = TRUE;

    gint new_word_start = multicursor_word.starting_doc;
    gint new_word_end = multicursor_word.starting_doc + multicursor_word.word->len;

    scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_MULTICURSOR, 0);

    for (gint i = 0; i < sj->multicursor_words->len; i++) {
        Word *word = &g_array_index(sj->multicursor_words, Word, i);

        gint word_start = word->starting_doc;
        gint word_end = word->starting_doc + word->word->len;

        if (word->valid_search && (new_word_start == word_start)) {
            scintilla_send_message(sj->sci, SCI_INDICATORCLEARRANGE, word->starting_doc, word->word->len);
            word->valid_search = FALSE;
            return;
        }

        if (word->valid_search && ((new_word_start > word_start && new_word_start <= word_end) ||
                                   (new_word_end >= word_start && new_word_end <= word_end))) {
            scintilla_send_message(sj->sci, SCI_INDICATORCLEARRANGE, word->starting_doc, word->word->len);
            word->valid_search = FALSE;
        }
    }

    if (multicursor_word.starting_doc <= sj->multicursor_first_pos) {
        sj->multicursor_first_pos = multicursor_word.starting_doc;
    }

    if (new_word_end >= sj->multicursor_last_pos) {
        sj->multicursor_last_pos = new_word_end;
    }

    g_array_append_val(sj->multicursor_words, multicursor_word);
}

gboolean on_click_event_multicursor(GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;

    if (mouse_movement_performed(sj, event)) {
        if (sj->multicursor_enabled == MC_ACCEPTING) {

        } else if (sj->multicursor_enabled == MC_DISABLED) {

        } else if (sj->multicursor_enabled == MC_REPLACING) {
            multicursor_complete(sj);
        }

        return TRUE;
    }

    return FALSE;
}

gint sort_words_by_starting_doc(gconstpointer a, gconstpointer b) {
    const Word *struct_a = (const Word *)a;
    const Word *struct_b = (const Word *)b;

    if (struct_a->starting_doc < struct_b->starting_doc) {
        return -1;
    } else if (struct_a->starting_doc > struct_b->starting_doc) {
        return 1;
    } else {
        return 0;
    }
}
