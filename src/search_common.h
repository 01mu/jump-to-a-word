#ifndef SEARCH_COMMON_H_
#define SEARCH_COMMON_H_

#include <geanyplugin.h>

#include "jump_to_a_word.h"

gint get_search_word_pos(ShortcutJump *sj);
gint get_search_word_pos_last(ShortcutJump *sj);
gint get_search_word_pos_first(ShortcutJump *sj);
gboolean valid_smart_case(char haystack_char, char needle_char);
gboolean set_search_word_pos_right_key(ShortcutJump *sj);
gboolean set_search_word_pos_left_key(ShortcutJump *sj);
gboolean on_click_event_search(GtkWidget *widget, GdkEventButton *event, gpointer user_data);

#endif
