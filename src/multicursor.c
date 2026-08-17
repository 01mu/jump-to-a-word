#include "annotation.h"
#include "duplicate_string.h"
#include "insert_line_multicursor.h"
#include "jump_to_a_word.h"
#include "multicursor_replace.h"
#include "transpose_string.h"
#include "util.h"
#include "values.h"

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

void multicursor_cancel(ShortcutJump *sj) {
    for (gint i = 0; i < sj->multicursor_words->len; i++) {
        Word word = g_array_index(sj->multicursor_words, Word, i);

        scintilla_send_message(sj->sci, SCI_SETINDICATORCURRENT, INDICATOR_MULTICURSOR, 0);
        scintilla_send_message(sj->sci, SCI_INDICATORCLEARRANGE, word.starting, word.word->len);
    }

    scintilla_send_message(sj->sci, SCI_SETREADONLY, 0, 0);
    annotation_clear(sj->sci, sj->eol_message_line);

    toggle_multicursor_menu(sj, FALSE);

    multicursor_end(sj);
    ui_set_statusbar(TRUE, _("Multicursor mode canceled."));
}

static void multicursor_start(ShortcutJump *sj) {
    sj->sci = get_scintilla_object();

    define_indicators(sj->sci, sj->config_settings->tag_color, sj->config_settings->highlight_color,
                      sj->config_settings->text_color);

    get_view_positions(sj);

    sj->multicursor_eol_message = g_string_new("");
    sj->multicursor_words = g_array_new(TRUE, FALSE, sizeof(Word));

    sj->multicursor_first_pos = scintilla_send_message(sj->sci, SCI_GETLENGTH, 0, 0);
    sj->multicursor_last_pos = 0;

    gint pos = scintilla_send_message(sj->sci, SCI_GETCURRENTPOS, 0, 0);
    gint line = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, pos, 0);

    sj->multicusor_eol_message_line = line;
    annotation_display_accepting_multicursor(sj);

    sj->multicursor_mode = MC_ACCEPTING;
    scintilla_send_message(sj->sci, SCI_SETREADONLY, 1, 0);
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
            multicursor_cancel(sj);
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
