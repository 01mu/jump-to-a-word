#ifndef PASTE_H_
#define PASTE_H_

#include "jump_to_a_word.h"

void paste_get_clipboard_text(ShortcutJump *sj);
gboolean on_paste_key_release_replace(GtkWidget *widget, GdkEventKey *event, gpointer user_data);
gboolean on_paste_key_release_word_search(GtkWidget *widget, GdkEventKey *event, gpointer user_data);
gboolean on_paste_key_release_substring_search(GtkWidget *widget, GdkEventKey *event, gpointer user_data);

#endif
