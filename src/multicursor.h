#ifndef MULTICURSOR_H_
#define MULTICURSOR_H_

#include "jump_to_a_word.h"

void multicursor_cancel(ShortcutJump *sj);
void multicursor_cb(GtkMenuItem *menu_item, gpointer user_data);
gboolean multicursor_kb(GeanyKeyBinding *kb, guint key_id, gpointer user_data);
void multicursor_end(ShortcutJump *sj);
void multicursor_toggle(ShortcutJump *sj);

#endif
