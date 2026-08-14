#include "duplicate_string.h"
#include "insert_line.h"
#include "jump_to_a_word.h"
#include "replace_instant.h"
#include "transpose_string.h"
#include "util.h"

void handle_action(gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;
    ReplaceAction ra = sj->config_settings->replace_action;
    MulticusrorMode mm = sj->multicursor_mode;
    JumpMode jm = sj->current_mode;

    if (sj->config_settings->instant_transpose && mm == MC_ACCEPTING) {
        if (jm == JM_NONE) {
            gint valid_count = 0;

            for (gint i = 0; i < sj->multicursor_words->len; i++) {
                Word word = g_array_index(sj->multicursor_words, Word, i);
                valid_count += word.valid_search ? 1 : 0;
            }

            if (valid_count == 2) {
                transpose_string(sj, TRUE);
                return;
            }
        }
    }

    if (ra == RA_REPLACE || ra == RA_INSERT_START || ra == RA_INSERT_END) {
        if (mm == MC_DISABLED) {
            if (jm == JM_SEARCH) {
                replace_word_init(sj);
                return;
            } else if (jm == JM_SUBSTRING) {
                replace_substring_init(sj);
                return;
            } else if (jm == JM_NONE) {
                replace_instant_init(sj);
                return;
            }
        } else if (mm == MC_ACCEPTING) {
            if (jm == JM_NONE) {
                multicursor_replace(sj);
                return;
            }
        }
    } else if (ra == RA_INSERT_NEXT_LINE || ra == RA_INSERT_PREVIOUS_LINE) {
        if (mm == MC_DISABLED) {
            if (jm == JM_NONE) {
                get_strings_for_instant_action(sj);
                line_insert_from_search(sj);
                return;
            } else if (jm == JM_SUBSTRING) {
                line_insert_from_search(sj);
                return;
            } else if (jm == JM_SEARCH) {
                line_insert_from_search(sj);
                return;
            }
        } else if (mm == MC_ACCEPTING) {
            if (jm == JM_NONE) {
                line_insert_from_multicursor(sj);
                return;
            }
        }
    } else if (ra == RA_TRANSPOSE_STRING) {
        if (mm == MC_ACCEPTING) {
            if (jm == JM_NONE) {
                transpose_string(sj, FALSE);
                return;
            }
        }
    } else if (ra == RA_DUPLICATE) {
        if (mm == MC_DISABLED) {
            if (jm == JM_NONE) {
                get_strings_for_instant_action(sj);
                duplicate_string(sj);
                return;
            } else if (jm == JM_SUBSTRING) {
                duplicate_string(sj);
                return;
            } else if (jm == JM_SEARCH) {
                duplicate_string(sj);
                return;
            }
        } else if (mm == MC_ACCEPTING) {
            if (jm == JM_NONE) {
                duplicate_string_for_multicursor(sj);
                return;
            }
        }
    }

    ui_set_statusbar(TRUE, _("Nothing to do."));
}

void replace_search_cb(GtkMenuItem *menu_item, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;

    if (!sj->waiting_after_single_instance) {
        handle_action(user_data);
    }
}

gboolean replace_search_kb(GeanyKeyBinding *kb, guint key_id, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;

    if (!sj->waiting_after_single_instance) {
        handle_action(user_data);
    }

    return TRUE;
}
