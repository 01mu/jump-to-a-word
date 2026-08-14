#include "jump_to_a_word.h"
#include "values.h"

static void previous_cursor_init(ShortcutJump *sj) {
    gint temp = scintilla_send_message(sj->sci, SCI_GETCURRENTPOS, 0, 0);
    if (sj->previous_cursor_pos == -1) {
        scintilla_send_message(sj->sci, SCI_GOTOPOS, temp, 0);
    } else {
        if (sj->config_settings->move_marker_to_line) {
            GeanyDocument *doc = document_get_current();
            if (!doc->is_valid) {
                exit(1);
            } else {
                gint line = scintilla_send_message(sj->sci, SCI_LINEFROMPOSITION, sj->previous_cursor_pos, 0);
                navqueue_goto_line(doc, doc, line + 1);
            }
        } else {
            scintilla_send_message(sj->sci, SCI_GOTOPOS, sj->previous_cursor_pos, 0);
        }
        sj->previous_cursor_pos = temp;
    }
}

void jump_to_previous_cursor_cb(GtkMenuItem *menu_item, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;
    sj->sci = get_scintilla_object();
    if (sj->current_mode == JM_NONE) {
        previous_cursor_init(sj);
    }
}

gboolean jump_to_previous_cursor_kb(GeanyKeyBinding *kb, guint key_id, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;
    sj->sci = get_scintilla_object();
    if (sj->current_mode == JM_NONE) {
        previous_cursor_init(sj);
    }
    return TRUE;
}
