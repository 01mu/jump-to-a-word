#ifndef SEARCH_SUBSTRING_H_
#define SEARCH_SUBSTRING_H_

#include <geanyplugin.h>

#include "jump_to_a_word.h"

void search_substring_end(ShortcutJump *sj);
void search_substring_set_query(ShortcutJump *sj);
void search_substring_get_substrings(ShortcutJump *sj);
void search_substring_replace_complete(ShortcutJump *sj);
void search_substring_replace_cancel(ShortcutJump *sj);
void search_substring_jump_cancel(ShortcutJump *sj);
void serach_substring_init(ShortcutJump *sj);
void search_substring_cb(GtkMenuItem *menu_item, gpointer user_data);
gboolean search_substring_kb(GeanyKeyBinding *kb, guint key_id, gpointer user_data);

#endif
