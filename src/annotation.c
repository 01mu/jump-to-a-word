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

#include "jump_to_a_word.h"
#include "util.h"

/**
 * @brief Clears the end of line annotation for either a search, search replacement, or a character shortcut jump.
 *
 * @param ScintillaObject *sci: The Scintilla object
 * @param gint eol_message_line: The line the annotation appears on
 */
void annotation_clear(ScintillaObject *sci, gint eol_message_line) {
    scintilla_send_message(sci, SCI_EOLANNOTATIONSETTEXT, eol_message_line, (sptr_t) "");
    scintilla_send_message(sci, SCI_EOLANNOTATIONCLEARALL, 0, 0);
    scintilla_send_message(sci, SCI_RELEASEALLEXTENDEDSTYLES, 0, 0);
}

/**
 * @brief Sets the values for the annotation display. SCI_ALLOCATEEXTENDEDSTYLES is needed so the defined annotation
 * styles do not conflict with others in the current editor. Adjusts eol_message_line based on how many lfs were
 * added in order to preverse the cursor position.
 *
 * @param ShortcutJump *sj: The plugin object
 */
void annotation_show(ShortcutJump *sj) {
    sj->eol_message_line = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, sj->current_cursor_pos, 0);

    if (sj->lf_positions && sj->lf_positions->len > 0) {
        gint pos = scintilla_send_message(sj->sci, SCI_GETCURRENTPOS, 0, 0);
        gint current_line = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, pos, 0);
        gint lfs_added = get_lfs(sj, current_line);
        gint line = sj->current_cursor_pos + lfs_added;

        sj->eol_message_line = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, line, 0);
    }

    gint text_color = sj->config_settings->text_color;
    gint search_annotation_bg_color = sj->config_settings->search_annotation_bg_color;

    scintilla_send_message(sj->sci, SCI_EOLANNOTATIONSETVISIBLE, EOLANNOTATION_STADIUM, 0);
    scintilla_send_message(sj->sci, SCI_STYLESETFORE, EOLANNOTATION_STADIUM, text_color);
    scintilla_send_message(sj->sci, SCI_STYLESETBACK, EOLANNOTATION_STADIUM, search_annotation_bg_color);
    scintilla_send_message(sj->sci, SCI_EOLANNOTATIONSETTEXT, sj->eol_message_line, (sptr_t)sj->eol_message->str);
    scintilla_send_message(sj->sci, SCI_EOLANNOTATIONSETSTYLEOFFSET, EOLANNOTATION_STADIUM, 0);
}

/**
 * @brief Sets the end of line annotation message for a search.
 *
 * @param ShortcutJump *sj: The plugin object
 */
void annotation_display_search(ShortcutJump *sj) {
    if (sj->config_settings->show_annotations) {
        gchar *s = "Searching for word: \"%s\" (%i result%s)";

        annotation_clear(sj->sci, sj->eol_message_line);
        g_string_printf(sj->eol_message, s, sj->search_query->str, sj->search_results_count,
                        sj->search_results_count == 1 ? "" : "s");
        annotation_show(sj);
    }
}

/**
 * @brief Sets the end of line annotation message for a word search.
 *
 * @param ShortcutJump *sj: The plugin object
 */
void annotation_display_substring(ShortcutJump *sj) {
    if (sj->config_settings->show_annotations) {
        gchar *s = "Searching for substring: \"%s\" (%i result%s)";

        annotation_clear(sj->sci, sj->eol_message_line);
        g_string_printf(sj->eol_message, s, sj->search_query->str, sj->search_results_count,
                        sj->search_results_count == 1 ? "" : "s");
        annotation_show(sj);
    }
}

/**
 * @brief Sets the end of line annotation message for while searching for a character.
 *
 * @param ShortcutJump *sj: The plugin object
 */
void annotation_display_char_search(ShortcutJump *sj) {
    if (sj->config_settings->show_annotations) {
        gchar *s = "%i occurance%s";

        annotation_clear(sj->sci, sj->eol_message_line);
        g_string_printf(sj->eol_message, s, sj->search_results_count, sj->search_results_count == 1 ? "" : "s");
        annotation_show(sj);
    }
}

/**
 * @brief Sets the end of line annotation message for a word replacement.
 *
 * @param ShortcutJump *sj: The plugin object
 */
void annotation_display_replace(ShortcutJump *sj) {
    if (sj->config_settings->show_annotations) {
        gchar *s = "Replacing selection (%i word%s";

        annotation_clear(sj->sci, sj->eol_message_line);
        g_string_printf(sj->eol_message, s, sj->search_results_count, sj->search_results_count == 1 ? ")" : "s)");
        annotation_show(sj);
    }
}

/**
 * @brief Sets the end of line annotation message while replacing a substring.
 *
 * @param ShortcutJump *sj: The plugin object
 */
void annotation_display_replace_substring(ShortcutJump *sj) {
    if (sj->config_settings->show_annotations) {
        gchar *s = "Replacing selection (%i substring%s";

        annotation_clear(sj->sci, sj->eol_message_line);
        g_string_printf(sj->eol_message, s, sj->search_results_count, sj->search_results_count == 1 ? ")" : "s)");
        annotation_show(sj);
    }
}

/**
 * @brief Sets the end of line annotation message while replacing a multicursor string.
 *
 * @param ShortcutJump *sj: The plugin object
 */
void annotation_display_multicusor_enabled_message(ShortcutJump *sj) {
    if (sj->config_settings->show_annotations) {
        annotation_clear(sj->sci, sj->eol_message_line);
        g_string_printf(sj->eol_message, "Multicusor enabled");
        annotation_show(sj);
    }
}

/**
 * @brief Sets the end of line annotation message while replacing a multicursor string.
 *
 * @param ShortcutJump *sj: The plugin object
 */
void annotation_display_accepting_multicursor(ShortcutJump *sj) {
    if (sj->config_settings->show_annotations) {
        gchar *s = "Accepting multicusror input";

        annotation_clear(sj->sci, sj->multicusor_eol_message_line);
        g_string_printf(sj->multicursor_eol_message, s, sj->search_results_count,
                        sj->search_results_count == 1 ? ")" : "s)");

        gint text_color = sj->config_settings->text_color;
        gint search_annotation_bg_color = sj->config_settings->search_annotation_bg_color;

        scintilla_send_message(sj->sci, SCI_EOLANNOTATIONSETVISIBLE, EOLANNOTATION_STADIUM, 0);
        scintilla_send_message(sj->sci, SCI_STYLESETFORE, EOLANNOTATION_STADIUM, text_color);
        scintilla_send_message(sj->sci, SCI_STYLESETBACK, EOLANNOTATION_STADIUM, search_annotation_bg_color);
        scintilla_send_message(sj->sci, SCI_EOLANNOTATIONSETTEXT, sj->multicusor_eol_message_line,
                               (sptr_t)sj->multicursor_eol_message->str);
        scintilla_send_message(sj->sci, SCI_EOLANNOTATIONSETSTYLEOFFSET, EOLANNOTATION_STADIUM, 0);
    }
}

/**
 * @brief Sets the end of line annotation message while replacing a multicursor string.
 *
 * @param ShortcutJump *sj: The plugin object
 */
void annotation_display_replace_multicursor(ShortcutJump *sj) {
    if (sj->config_settings->show_annotations) {
        gchar *s = "Replacing multicursor selection (%i string%s";

        annotation_clear(sj->sci, sj->multicusor_eol_message_line);
        g_string_printf(sj->multicursor_eol_message, s, sj->words->len, sj->words->len == 1 ? ")" : "s)");

        gint text_color = sj->config_settings->text_color;
        gint search_annotation_bg_color = sj->config_settings->search_annotation_bg_color;

        scintilla_send_message(sj->sci, SCI_EOLANNOTATIONSETVISIBLE, EOLANNOTATION_STADIUM, 0);
        scintilla_send_message(sj->sci, SCI_STYLESETFORE, EOLANNOTATION_STADIUM, text_color);
        scintilla_send_message(sj->sci, SCI_STYLESETBACK, EOLANNOTATION_STADIUM, search_annotation_bg_color);
        scintilla_send_message(sj->sci, SCI_EOLANNOTATIONSETTEXT, sj->multicusor_eol_message_line,
                               (sptr_t)sj->multicursor_eol_message->str);
        scintilla_send_message(sj->sci, SCI_EOLANNOTATIONSETSTYLEOFFSET, EOLANNOTATION_STADIUM, 0);
    }
}

/**
 * @brief Sets the end of line annotation message while replacing a character.
 *
 * @param ShortcutJump *sj: The plugin object
 */

void annotation_display_replace_char(ShortcutJump *sj) {
    if (sj->config_settings->show_annotations) {
        gchar *s = "Replacing selection (%i occurance%s";

        annotation_clear(sj->sci, sj->eol_message_line);
        g_string_printf(sj->eol_message, s, sj->search_results_count, sj->search_results_count == 1 ? ")" : "s)");
        annotation_show(sj);
    }
}

/**
 * @brief Sets the end of line annotation message for a character shortcut jump.
 *
 * @param ShortcutJump *sj: The plugin object
 */
void annotation_display_shortcut_char(ShortcutJump *sj) {
    if (sj->config_settings->show_annotations) {
        annotation_clear(sj->sci, sj->eol_message_line);
        g_string_printf(sj->eol_message, "Waiting for character input");
        annotation_show(sj);
    }
}
