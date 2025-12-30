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
#include "duplicate_string.h"
#include "insert_line.h"
#include "jump_to_a_word.h"
#include "transpose_string.h"
#include "util.h"
#include "values.h"

static void multicursor_start(ShortcutJump *sj) {
    sj->sci = get_scintilla_object();

    define_indicators(sj->sci, sj->config_settings->tag_color, sj->config_settings->highlight_color,
                      sj->config_settings->text_color);

    get_view_positions(sj);

    sj->multicursor_first_pos = scintilla_send_message(sj->sci, SCI_GETLENGTH, 0, 0);
    sj->multicursor_last_pos = 0;
    sj->multicursor_eol_message = g_string_new("");
    sj->multicursor_words = g_array_new(TRUE, FALSE, sizeof(Word));

    sj->search_word_pos = -1;
    sj->search_word_pos_first = -1;
    sj->search_word_pos_last = -1;
    sj->search_change_made = FALSE;
    sj->replace_len = 0;
    sj->replace_instant = FALSE;

    sj->waiting_after_single_instance = FALSE;

    gint pos = scintilla_send_message(sj->sci, SCI_GETCURRENTPOS, 0, 0);
    gint line = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, pos, 0);

    sj->multicusor_eol_message_line = line;

    annotation_display_accepting_multicursor(sj);
    sj->multicursor_mode = MC_ACCEPTING;
    scintilla_send_message(sj->sci, SCI_SETREADONLY, 1, 0);
}

void multicursor_replace_end(ShortcutJump *sj) {
    for (gint i = 0; i < sj->multicursor_words->len; i++) {
        Word word = g_array_index(sj->multicursor_words, Word, i);
        g_string_free(word.word, TRUE);
    }

    g_string_free(sj->replace_query, TRUE);

    g_string_free(sj->replace_cache, TRUE);
    g_string_free(sj->buffer, TRUE);
    g_string_free(sj->cache, TRUE);

    g_string_free(sj->multicursor_eol_message, TRUE);
    g_array_free(sj->multicursor_words, TRUE);
    sj->current_mode = JM_NONE;
    sj->multicursor_mode = MC_DISABLED;
}

void multicursor_end(ShortcutJump *sj) {
    for (gint i = 0; i < sj->multicursor_words->len; i++) {
        Word word = g_array_index(sj->multicursor_words, Word, i);
        g_string_free(word.word, TRUE);
    }

    g_string_free(sj->multicursor_eol_message, TRUE);
    g_array_free(sj->multicursor_words, TRUE);
    sj->current_mode = JM_NONE;
    sj->multicursor_mode = MC_DISABLED;
}

void multicursor_replace_clear_indicators(ShortcutJump *sj) {
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

void multicursor_accepting_cancel(ShortcutJump *sj) {
    for (gint i = 0; i < sj->multicursor_words->len; i++) {
        Word word = g_array_index(sj->multicursor_words, Word, i);
        scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_MULTICURSOR, 0);
        scintilla_send_message(sj->sci, SCI_INDICATORCLEARRANGE, word.starting, word.word->len);
    }
    scintilla_send_message(sj->sci, SCI_SETREADONLY, 0, 0);
    multicursor_replace_clear_indicators(sj);
    annotation_clear(sj->sci, sj->eol_message_line);

    toggle_multicursor_menu(sj, FALSE);

    multicursor_end(sj);
    ui_set_statusbar(TRUE, _("Multicursor mode canceled."));
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

    multicursor_replace_end(sj);
}

void multicursor_add_word_from_selection(ShortcutJump *sj, gint start, gint end) {
    Word multicursor_word;
    multicursor_word.word = g_string_new(sci_get_contents_range(sj->sci, start, end));
    multicursor_word.line = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, start, 0);
    multicursor_word.starting = start;
    multicursor_word.starting_doc = start;
    multicursor_word.valid_search = TRUE;

    gint new_word_start = multicursor_word.starting_doc;
    gint new_word_end = multicursor_word.starting_doc + multicursor_word.word->len;

    for (gint i = 0; i < sj->multicursor_words->len; i++) {
        Word *word = &g_array_index(sj->multicursor_words, Word, i);

        gint old_word_start = word->starting_doc;
        gint old_word_end = word->starting_doc + word->word->len;

        if (!word->valid_search) {
            continue;
        }

        if (new_word_start == old_word_start && new_word_end == old_word_end) {
            scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_MULTICURSOR, 0);
            scintilla_send_message(sj->sci, SCI_INDICATORCLEARRANGE, word->starting_doc, word->word->len);
            word->valid_search = FALSE;
            return;
        }

        gboolean old_enclosed_by_new = old_word_start >= new_word_start && old_word_end <= new_word_end;
        gboolean new_start_enclosed_by_old = new_word_start > old_word_start && new_word_start < old_word_end;
        gboolean new_end_enclosed_by_old = new_word_end > old_word_start && new_word_end < old_word_end;

        if (old_enclosed_by_new || new_start_enclosed_by_old || new_end_enclosed_by_old) {
            scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_MULTICURSOR, 0);
            scintilla_send_message(sj->sci, SCI_INDICATORCLEARRANGE, word->starting_doc, word->word->len);
            scintilla_send_message(sj->sci, SCI_INDICATORFILLRANGE, multicursor_word.starting_doc,
                                   multicursor_word.word->len);
            scintilla_send_message(sj->sci, SCI_INDICATORCLEARRANGE, word->starting_doc, word->word->len);
            word->valid_search = FALSE;
        }
    }

    scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_MULTICURSOR, 0);
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

    for (gint i = 0; i < sj->multicursor_words->len; i++) {
        Word *word = &g_array_index(sj->multicursor_words, Word, i);

        gint old_word_start = word->starting_doc;
        gint old_word_end = word->starting_doc + word->word->len;

        if (!word->valid_search) {
            continue;
        }

        if (new_word_start == old_word_start) {
            scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_MULTICURSOR, 0);
            scintilla_send_message(sj->sci, SCI_INDICATORCLEARRANGE, word->starting_doc, word->word->len);
            word->valid_search = FALSE;
            return;
        }

        gboolean old_enclosed_by_new = old_word_start >= new_word_start && old_word_end <= new_word_end;
        gboolean new_start_enclosed_by_old = new_word_start > old_word_start && new_word_start < old_word_end;
        gboolean new_end_enclosed_by_old = new_word_end > old_word_start && new_word_end < old_word_end;

        if (old_enclosed_by_new || new_start_enclosed_by_old || new_end_enclosed_by_old) {
            scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_MULTICURSOR, 0);
            scintilla_send_message(sj->sci, SCI_INDICATORCLEARRANGE, word->starting_doc, word->word->len);
            scintilla_send_message(sj->sci, SCI_INDICATORFILLRANGE, multicursor_word.starting_doc,
                                   multicursor_word.word->len);
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

gboolean on_click_event_multicursor_replace(GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;
    if (mouse_movement_performed(sj, event)) {
        gtk_check_menu_item_set_active(sj->multicursor_menu_checkbox, FALSE);
        return TRUE;
    }
    return FALSE;
}

void multicursor_toggle(ShortcutJump *sj) {
    if (sj->multicursor_mode == MC_ACCEPTING) {
        for (gint i = 0; i < sj->multicursor_words->len; i++) {
            Word word = g_array_index(sj->multicursor_words, Word, i);
            scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_MULTICURSOR, 0);
            scintilla_send_message(sj->sci, SCI_INDICATORCLEARRANGE, word.starting, word.word->len);
        }

        if (sj->current_mode == JM_TRANSPOSE_MULTICURSOR) {
            multicursor_transpose_cancel(sj);
        } else if (sj->current_mode == JM_INSERTING_LINE_MULTICURSOR) {
            multicursor_line_insert_cancel(sj);
        } else if (sj->current_mode == JM_DUPLICATE_MULTICURSOR) {
            multicursor_duplicate_cancel(sj);
        } else if (sj->current_mode == JM_REPLACE_MULTICURSOR) {
            multicursor_replace_cancel(sj);
        } else if (sj->current_mode == JM_NONE) {
            multicursor_accepting_cancel(sj);
        }
    } else if (sj->multicursor_mode == MC_DISABLED) {
        ui_set_statusbar(TRUE, _("Multicursor mode enabled."));
        end_actions(sj);
        multicursor_start(sj);
    }
}

void multicursor_cb(GtkMenuItem *menu_item, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;

    if (sj->multicursor_mode == MC_ACCEPTING) {
        gtk_check_menu_item_set_active(sj->multicursor_menu_checkbox, FALSE);
    } else if (sj->multicursor_mode == MC_DISABLED) {
        gtk_check_menu_item_set_active(sj->multicursor_menu_checkbox, TRUE);
    }
}

gboolean multicursor_kb(GeanyKeyBinding *kb, guint key_id, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;

    if (sj->multicursor_mode == MC_ACCEPTING) {
        gtk_check_menu_item_set_active(sj->multicursor_menu_checkbox, FALSE);
    } else if (sj->multicursor_mode == MC_DISABLED) {
        gtk_check_menu_item_set_active(sj->multicursor_menu_checkbox, TRUE);
    }

    return TRUE;
}
