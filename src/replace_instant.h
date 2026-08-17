#ifndef REPLACE_INSTANT_H
#define REPLACE_INSTANT_H

#include "jump_to_a_word.h"

gboolean on_key_press_search_replace(GtkWidget *widget, GdkEventKey *event, gpointer user_data);
void multicursor_replace_init(ShortcutJump *sj);
void replace_substring_init(ShortcutJump *sj);
void replace_word_init(ShortcutJump *sj);
void replace_instant_init(ShortcutJump *sj);

#endif
