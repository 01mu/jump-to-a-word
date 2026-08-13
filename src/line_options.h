#ifndef LINE_OPTIONS_H
#define LINE_OPTIONS_H

#include <geanyplugin.h>

#define PATH_SEPARATOR " \342\206\222 "
#define SEPARATORS " -_./\\\"'"
#define IS_SEPARATOR(c) (strchr(SEPARATORS, (c)) != NULL)
#define next_separator(p) (strpbrk(p, SEPARATORS))

enum { COL_LABEL, COL_TYPE, COL_COUNT };
typedef enum { COL_TYPE_MENU_ITEM = 1 << 0, COL_TYPE_FILE = 1 << 1, COL_TYPE_ANY = 0xffff } ColType;

void open_text_options_cb(GtkMenuItem *menu_item, gpointer user_data);
void open_line_options_cb(GtkMenuItem *menu_item, gpointer user_data);
void open_replace_options_cb(GtkMenuItem *menu_item, gpointer user_data);
gboolean open_text_options_kb(GeanyKeyBinding *kb, guint key_id, gpointer user_data);
gboolean open_line_options_kb(GeanyKeyBinding *kb, guint key_id, gpointer user_data);
gboolean open_replace_options_kb(GeanyKeyBinding *kb, guint key_id, gpointer user_data);

#endif
