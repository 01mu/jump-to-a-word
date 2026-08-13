#ifndef SHORTCUT_COMMON_H_
#define SHORTCUT_COMMON_H_

#include <geanyplugin.h>

#include "jump_to_a_word.h"

void shortcut_end(ShortcutJump *sj, gboolean was_canceled);
void shortcut_set_to_first_visible_line(ShortcutJump *sj);
gint shortcut_get_max_words(gint shortcuts_include_single_char);
GString *shortcut_mask_bytes(GArray *words, GString *buffer, gint first_position);
GString *shortcut_set_tags_in_buffer(GArray *words, GString *buffer, gint first_position);
GString *shortcut_make_tag(gint shortcuts_include_single_char, gint shortcut_all_caps, gint position);
gint shortcut_get_utf8_char_length(gchar c);
gint shortcut_set_padding(ShortcutJump *sj, gint word_length);
void shortcut_set_after_placement(ShortcutJump *sj);
gint shortcut_on_key_press_action(GdkEventKey *event, gpointer user_data);
void shortcut_set_indicators(ScintillaObject *sci, GArray *words);

#endif
