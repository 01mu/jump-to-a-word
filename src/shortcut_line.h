#ifndef SHORTCUT_LINE_H_
#define SHORTCUT_LINE_H_

#include "jump_to_a_word.h"

void shortcut_line_complete(ShortcutJump *sj, gint pos, gint word_length, gint line);
void shortcut_line_cancel(ShortcutJump *sj);
void shortcut_line_init(ShortcutJump *sj);
void shortcut_line_cb(GtkMenuItem *menu_item, gpointer user_data);
gboolean shortcut_line_kb(GeanyKeyBinding *kb, guint key_id, gpointer user_data);

#endif
