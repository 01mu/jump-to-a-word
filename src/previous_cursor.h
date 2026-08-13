#ifndef PREVIOUS_CURSOR_H_
#define PREVIOUS_CURSOR_H_

#include <geanyplugin.h>

void jump_to_previous_cursor_cb(GtkMenuItem *menu_item, gpointer user_data);
gboolean jump_to_previous_cursor_kb(GeanyKeyBinding *kb, guint key_id, gpointer user_data);

#endif
