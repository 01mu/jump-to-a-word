#ifndef SHORTCUT_CHAR_H_
#define SHORTCUT_CHAR_H_

#include <geanyplugin.h>

#include "jump_to_a_word.h"

void shortcut_char_get_chars(ShortcutJump *sj, gchar query);
void shortcut_char_common(ShortcutJump *sj);
void shortcut_char_jumping_cancel(ShortcutJump *sj);
void shortcut_char_jumping_complete(ShortcutJump *sj, gint pos, gint word_length, gint line);
void shortcut_char_waiting_cancel(ShortcutJump *sj);
void shortcut_char_replacing_cancel(ShortcutJump *sj);
void shortcut_char_replacing_complete(ShortcutJump *sj);
void shortcut_char_init_with_query(ShortcutJump *sj, gchar query);
void shortcut_char_init(ShortcutJump *sj);
void shortcut_char_cb(GtkMenuItem *menu_item, gpointer user_data);
gboolean shortcut_char_kb(GeanyKeyBinding *kb, guint key_id, gpointer user_data);

#endif
