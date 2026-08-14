#ifndef HANDLE_ACTION_H_
#define HANDLE_ACTION_H_

#include "jump_to_a_word.h"

void replace_search_cb(GtkMenuItem *menu_item, gpointer user_data);
gboolean replace_search_kb(GeanyKeyBinding *kb, guint key_id, gpointer user_data);

#endif
