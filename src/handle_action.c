#include "duplicate_string.h"
#include "insert_line_multicursor.h"
#include "insert_line_search.h"
#include "jump_to_a_word.h"
#include "replace_instant.h"
#include "transpose_string.h"
#include "util.h"

static void _action_replace(ShortcutJump *sj, MulticusrorMode mm, JumpMode jm) {
    if (mm == MC_DISABLED) {
        if (jm == JM_SEARCH) {
            replace_word_init(sj);
        } else if (jm == JM_SUBSTRING) {
            replace_substring_init(sj);
        } else if (jm == JM_NONE) {
            replace_instant_init(sj);
        }
    } else if (mm == MC_ACCEPTING) {
        if (jm == JM_NONE) {
            multicursor_replace_init(sj);
        }
    }
}

static void _action_insert_line(ShortcutJump *sj, MulticusrorMode mm, JumpMode jm) {
    if (mm == MC_DISABLED) {
        if (jm == JM_NONE) {
            get_strings_for_instant_action(sj);
            line_insert_from_search_init(sj);
        } else if (jm == JM_SUBSTRING) {
            line_insert_from_search_init(sj);
        } else if (jm == JM_SEARCH) {
            line_insert_from_search_init(sj);
        }
    } else if (mm == MC_ACCEPTING) {
        if (jm == JM_NONE) {
            line_insert_from_multicursor_init(sj);
        }
    }
}

static void _action_transpose(ShortcutJump *sj, MulticusrorMode mm, JumpMode jm) {
    if (mm == MC_ACCEPTING) {
        if (jm == JM_NONE) {
            transpose_string(sj, FALSE);
        }
    }
}

void _action_duplicate(ShortcutJump *sj, MulticusrorMode mm, JumpMode jm) {
    if (mm == MC_DISABLED) {
        if (jm == JM_NONE) {
            get_strings_for_instant_action(sj);
            duplicate_string(sj);
        } else if (jm == JM_SUBSTRING) {
            duplicate_string(sj);
        } else if (jm == JM_SEARCH) {
            duplicate_string(sj);
        }
    } else if (mm == MC_ACCEPTING) {
        if (jm == JM_NONE) {
            duplicate_string_for_multicursor(sj);
        }
    }
}

static void handle_action(gpointer user_data) {
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
        _action_replace(sj, sj->multicursor_mode, sj->current_mode);
    } else if (ra == RA_INSERT_NEXT_LINE || ra == RA_INSERT_PREVIOUS_LINE) {
        _action_insert_line(sj, sj->multicursor_mode, sj->current_mode);
    } else if (ra == RA_TRANSPOSE_STRING) {
        _action_transpose(sj, sj->multicursor_mode, sj->current_mode);
    } else if (ra == RA_DUPLICATE) {
        _action_duplicate(sj, sj->multicursor_mode, sj->current_mode);
    }

    ui_set_statusbar(TRUE, _("Nothing to do."));
}

static void _replace_common(ShortcutJump *sj) {
    sj->cached_replace_action = sj->config_settings->replace_action;
    sj->has_cached_replace_action = TRUE;
    sj->config_settings->replace_action = RA_REPLACE;
    _action_replace(sj, sj->multicursor_mode, sj->current_mode);
}

static void _insert_start_common(ShortcutJump *sj) {
    sj->cached_replace_action = sj->config_settings->replace_action;
    sj->has_cached_replace_action = TRUE;
    sj->config_settings->replace_action = RA_INSERT_START;
    _action_replace(sj, sj->multicursor_mode, sj->current_mode);
}

static void _insert_end_common(ShortcutJump *sj) {
    sj->cached_replace_action = sj->config_settings->replace_action;
    sj->has_cached_replace_action = TRUE;
    sj->config_settings->replace_action = RA_INSERT_END;
    _action_replace(sj, sj->multicursor_mode, sj->current_mode);
}

static void _insert_previous_line_common(ShortcutJump *sj) {
    sj->cached_replace_action = sj->config_settings->replace_action;
    sj->has_cached_replace_action = TRUE;
    sj->config_settings->replace_action = RA_INSERT_PREVIOUS_LINE;
    _action_insert_line(sj, sj->multicursor_mode, sj->current_mode);
}

static void _insert_next_line_common(ShortcutJump *sj) {
    sj->cached_replace_action = sj->config_settings->replace_action;
    sj->has_cached_replace_action = TRUE;
    sj->config_settings->replace_action = RA_INSERT_NEXT_LINE;
    _action_insert_line(sj, sj->multicursor_mode, sj->current_mode);
}

static void _transpose_common(ShortcutJump *sj) {
    sj->cached_replace_action = sj->config_settings->replace_action;
    sj->has_cached_replace_action = TRUE;
    sj->config_settings->replace_action = RA_TRANSPOSE_STRING;
    _action_transpose(sj, sj->multicursor_mode, sj->current_mode);
}

static void _duplicate_common(ShortcutJump *sj) {
    sj->cached_replace_action = sj->config_settings->replace_action;
    sj->has_cached_replace_action = TRUE;
    sj->config_settings->replace_action = RA_DUPLICATE;
    _action_duplicate(sj, sj->multicursor_mode, sj->current_mode);
}

void cb_action_replace(GtkMenuItem *menu_item, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;
    _replace_common(sj);
}

void cb_action_insert_start(GtkMenuItem *menu_item, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;
    _insert_start_common(sj);
}

void cb_action_insert_end(GtkMenuItem *menu_item, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;
    _insert_end_common(sj);
}

void cb_action_insert_previous_line(GtkMenuItem *menu_item, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;
    _insert_previous_line_common(sj);
}

void cb_action_insert_next_line(GtkMenuItem *menu_item, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;
    _insert_next_line_common(sj);
}

void cb_action_transpose(GtkMenuItem *menu_item, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;
    _transpose_common(sj);
}

void cb_action_duplicate(GtkMenuItem *menu_item, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;
    _duplicate_common(sj);
}

gboolean action_replace_kb(GeanyKeyBinding *kb, guint key_id, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;
    _replace_common(sj);
    return TRUE;
}

gboolean action_insert_start_kb(GeanyKeyBinding *kb, guint key_id, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;
    _insert_start_common(sj);
    return TRUE;
}

gboolean action_insert_end_kb(GeanyKeyBinding *kb, guint key_id, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;
    _insert_end_common(sj);
    return TRUE;
}

gboolean action_insert_previous_line_kb(GeanyKeyBinding *kb, guint key_id, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;
    _insert_previous_line_common(sj);
    return TRUE;
}

gboolean action_insert_next_line_kb(GeanyKeyBinding *kb, guint key_id, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;
    _insert_next_line_common(sj);
    return TRUE;
}

gboolean action_transpose_kb(GeanyKeyBinding *kb, guint key_id, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;
    _transpose_common(sj);
    return TRUE;
}

gboolean action_duplicate_kb(GeanyKeyBinding *kb, guint key_id, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;
    _duplicate_common(sj);
    return TRUE;
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
