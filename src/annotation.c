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

void annotation_clear(ScintillaObject *sci, gint eol_message_line) {
    scintilla_send_message(sci, SCI_EOLANNOTATIONSETTEXT, eol_message_line, (sptr_t) "");
    scintilla_send_message(sci, SCI_EOLANNOTATIONCLEARALL, 0, 0);
    scintilla_send_message(sci, SCI_RELEASEALLEXTENDEDSTYLES, 0, 0);
}

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
    gint line = sj->eol_message_line;
    scintilla_send_message(sj->sci, SCI_EOLANNOTATIONSETVISIBLE, EOLANNOTATION_STADIUM, 0);
    scintilla_send_message(sj->sci, SCI_STYLESETFORE, EOLANNOTATION_STADIUM, text_color);
    scintilla_send_message(sj->sci, SCI_STYLESETBACK, EOLANNOTATION_STADIUM, search_annotation_bg_color);
    scintilla_send_message(sj->sci, SCI_EOLANNOTATIONSETTEXT, line, (sptr_t)sj->eol_message->str);
    scintilla_send_message(sj->sci, SCI_EOLANNOTATIONSETSTYLEOFFSET, EOLANNOTATION_STADIUM, 0);
}

void annotation_display_search(ShortcutJump *sj) {
    if (!sj->config_settings->show_annotations) {
        return;
    }

    gchar *s = "Searching for word: \"%s\" (%i result%s)";
    gint count = sj->search_results_count;
    annotation_clear(sj->sci, sj->eol_message_line);
    g_string_printf(sj->eol_message, s, sj->search_query->str, count, count == 1 ? "" : "s");
    annotation_show(sj);
}

void annotation_display_substring(ShortcutJump *sj) {
    if (!sj->config_settings->show_annotations) {
        return;
    }

    gchar *s = "Searching for substring: \"%s\" (%i result%s)";
    gint count = sj->search_results_count;
    annotation_clear(sj->sci, sj->eol_message_line);
    g_string_printf(sj->eol_message, s, sj->search_query->str, count, count == 1 ? "" : "s");
    annotation_show(sj);
}

void annotation_display_char_search(ShortcutJump *sj) {
    if (!sj->config_settings->show_annotations) {
        return;
    }

    gchar *s = "%i occurance%s";
    gint count = sj->search_results_count;
    annotation_clear(sj->sci, sj->eol_message_line);
    g_string_printf(sj->eol_message, s, count, count == 1 ? "" : "s");
    annotation_show(sj);
}

void annotation_display_replace(ShortcutJump *sj) {
    if (!sj->config_settings->show_annotations) {
        return;
    }

    gchar *s = "Replacing selection (%i word%s";
    gint count = sj->search_results_count;
    annotation_clear(sj->sci, sj->eol_message_line);
    g_string_printf(sj->eol_message, s, count, count == 1 ? ")" : "s)");
    annotation_show(sj);
}

void annotation_display_inserting_line_from_search(ShortcutJump *sj) {
    if (!sj->config_settings->show_annotations) {
        return;
    }

    gchar *s = "Inserting line%s (%i string%s";
    gint count = sj->search_results_count;
    annotation_clear(sj->sci, sj->eol_message_line);
    g_string_printf(sj->eol_message, s, count == 1 ? "" : "s", count, count == 1 ? ")" : "s)");
    annotation_show(sj);
}

void annotation_display_replace_substring(ShortcutJump *sj) {
    if (!sj->config_settings->show_annotations) {
        return;
    }

    gchar *s = "Replacing selection (%i substring%s";
    gint count = sj->search_results_count;
    annotation_clear(sj->sci, sj->eol_message_line);
    g_string_printf(sj->eol_message, s, count, count == 1 ? ")" : "s)");
    annotation_show(sj);
}

void annotation_display_accepting_multicursor(ShortcutJump *sj) {
    if (!sj->config_settings->show_annotations) {
        return;
    }

    gchar *s = "Accepting multicursor input";
    gint count = sj->search_results_count;

    annotation_clear(sj->sci, sj->multicusor_eol_message_line);
    g_string_printf(sj->multicursor_eol_message, s, count, count == 1 ? ")" : "s)");

    gint text_color = sj->config_settings->text_color;
    gint search_annotation_bg_color = sj->config_settings->search_annotation_bg_color;
    gint line = sj->multicusor_eol_message_line;
    scintilla_send_message(sj->sci, SCI_EOLANNOTATIONSETVISIBLE, EOLANNOTATION_STADIUM, 0);
    scintilla_send_message(sj->sci, SCI_STYLESETFORE, EOLANNOTATION_STADIUM, text_color);
    scintilla_send_message(sj->sci, SCI_STYLESETBACK, EOLANNOTATION_STADIUM, search_annotation_bg_color);
    scintilla_send_message(sj->sci, SCI_EOLANNOTATIONSETTEXT, line, (sptr_t)sj->multicursor_eol_message->str);
    scintilla_send_message(sj->sci, SCI_EOLANNOTATIONSETSTYLEOFFSET, EOLANNOTATION_STADIUM, 0);
}

void annotation_display_inserting_line_multicursor(ShortcutJump *sj) {
    if (!sj->config_settings->show_annotations) {
        return;
    }

    gchar *s = "Inserting multicursor line%s (%i string%s";
    gint count = sj->search_results_count;
    annotation_clear(sj->sci, sj->multicusor_eol_message_line);
    g_string_printf(sj->multicursor_eol_message, s, count == 1 ? "" : "s", count, count == 1 ? ")" : "s)");

    gint text_color = sj->config_settings->text_color;
    gint search_annotation_bg_color = sj->config_settings->search_annotation_bg_color;
    gint line = sj->multicusor_eol_message_line;
    scintilla_send_message(sj->sci, SCI_EOLANNOTATIONSETVISIBLE, EOLANNOTATION_STADIUM, 0);
    scintilla_send_message(sj->sci, SCI_STYLESETFORE, EOLANNOTATION_STADIUM, text_color);
    scintilla_send_message(sj->sci, SCI_STYLESETBACK, EOLANNOTATION_STADIUM, search_annotation_bg_color);
    scintilla_send_message(sj->sci, SCI_EOLANNOTATIONSETTEXT, line, (sptr_t)sj->multicursor_eol_message->str);
    scintilla_send_message(sj->sci, SCI_EOLANNOTATIONSETSTYLEOFFSET, EOLANNOTATION_STADIUM, 0);
}

void annotation_display_replace_multicursor(ShortcutJump *sj) {
    if (!sj->config_settings->show_annotations) {
        return;
    }

    gchar *s = "Replacing multicursor selection (%i string%s";
    gint count = sj->search_results_count;
    annotation_clear(sj->sci, sj->multicusor_eol_message_line);
    g_string_printf(sj->multicursor_eol_message, s, count, count == 1 ? ")" : "s)");

    gint text_color = sj->config_settings->text_color;
    gint search_annotation_bg_color = sj->config_settings->search_annotation_bg_color;
    gint line = sj->multicusor_eol_message_line;
    scintilla_send_message(sj->sci, SCI_EOLANNOTATIONSETVISIBLE, EOLANNOTATION_STADIUM, 0);
    scintilla_send_message(sj->sci, SCI_STYLESETFORE, EOLANNOTATION_STADIUM, text_color);
    scintilla_send_message(sj->sci, SCI_STYLESETBACK, EOLANNOTATION_STADIUM, search_annotation_bg_color);
    scintilla_send_message(sj->sci, SCI_EOLANNOTATIONSETTEXT, line, (sptr_t)sj->multicursor_eol_message->str);
    scintilla_send_message(sj->sci, SCI_EOLANNOTATIONSETSTYLEOFFSET, EOLANNOTATION_STADIUM, 0);
}

void annotation_display_replace_char(ShortcutJump *sj) {
    if (!sj->config_settings->show_annotations) {
        return;
    }

    gchar *s = "Replacing selection (%i occurance%s";
    gint count = sj->search_results_count;
    annotation_clear(sj->sci, sj->eol_message_line);
    g_string_printf(sj->eol_message, s, count, count == 1 ? ")" : "s)");
    annotation_show(sj);
}

void annotation_display_shortcut_char(ShortcutJump *sj) {
    if (!sj->config_settings->show_annotations) {
        return;
    }

    annotation_clear(sj->sci, sj->eol_message_line);
    g_string_printf(sj->eol_message, "Waiting for character input");
    annotation_show(sj);
}

void annotation_display_replace_string(ShortcutJump *sj) {
    if (!sj->config_settings->show_annotations) {
        return;
    }

    gchar *s = "Inserting: \"%s\"";

    if (sj->current_mode == JM_REPLACE_MULTICURSOR) {
        annotation_clear(sj->sci, sj->multicusor_eol_message_line);

        g_string_printf(sj->multicursor_eol_message, s, sj->replace_query->str);

        gint text_color = sj->config_settings->text_color;
        gint search_annotation_bg_color = sj->config_settings->search_annotation_bg_color;
        gint line = sj->multicusor_eol_message_line;
        scintilla_send_message(sj->sci, SCI_EOLANNOTATIONSETVISIBLE, EOLANNOTATION_STADIUM, 0);
        scintilla_send_message(sj->sci, SCI_STYLESETFORE, EOLANNOTATION_STADIUM, text_color);
        scintilla_send_message(sj->sci, SCI_STYLESETBACK, EOLANNOTATION_STADIUM, search_annotation_bg_color);
        scintilla_send_message(sj->sci, SCI_EOLANNOTATIONSETTEXT, line, (sptr_t)sj->multicursor_eol_message->str);
        scintilla_send_message(sj->sci, SCI_EOLANNOTATIONSETSTYLEOFFSET, EOLANNOTATION_STADIUM, 0);
    } else {
        annotation_clear(sj->sci, sj->eol_message_line);
        g_string_printf(sj->eol_message, s, sj->replace_query->str);
        annotation_show(sj);
    }
}
