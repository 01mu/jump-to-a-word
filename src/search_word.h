#ifndef SEARCH_WORD_H
#define SEARCH_WORD_H

#include "jump_to_a_word.h"

void search_word_mark_words(ShortcutJump *sj, gboolean instant_replace);
void search_word_end(ShortcutJump *sj);
void search_word_init(ShortcutJump *sj, gboolean instant_replace);
void search_word_cb(GtkMenuItem *menu_item, gpointer user_data);
gboolean search_word_kb(GeanyKeyBinding *kb, guint key_id, gpointer user_data);
void search_word_get_words(ShortcutJump *sj);
void search_word_set_query(ShortcutJump *sj, gboolean instant_replace);
void search_word_replace_cancel(ShortcutJump *sj);
void search_word_replace_complete(ShortcutJump *sj);
void search_word_jump_cancel(ShortcutJump *sj);
void search_word_jump_complete(ShortcutJump *sj);

#endif
