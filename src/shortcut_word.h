#ifndef SHORTCUT_WORD_H_
#define SHORTCUT_WORD_H_

#include "jump_to_a_word.h"

void shortcut_word_complete(ShortcutJump *sj, gint pos, gint word_length, gint line);
void shortcut_word_cancel(ShortcutJump *sj);
void shortcut_word_init(ShortcutJump *sj);
void shortcut_word_cb(GtkMenuItem *menu_item, gpointer user_data);
gboolean shortcut_word_kb(GeanyKeyBinding *kb, guint key_id, gpointer user_data);

#endif
