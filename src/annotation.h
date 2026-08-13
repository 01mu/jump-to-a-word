#ifndef ANNOTATION_H
#define ANNOTATION_H

#include "jump_to_a_word.h"

void annotation_clear(ScintillaObject *sci, gint eol_message_line);
void annotation_show(ShortcutJump *sj);
void annotation_display_search(ShortcutJump *sj);
void annotation_display_substring(ShortcutJump *sj);
void annotation_display_char_search(ShortcutJump *sj);
void annotation_display_replace(ShortcutJump *sj);
void annotation_display_inserting_line_from_search(ShortcutJump *sj);
void annotation_display_replace_substring(ShortcutJump *sj);
void annotation_display_accepting_multicursor(ShortcutJump *sj);
void annotation_display_inserting_line_multicursor(ShortcutJump *sj);
void annotation_display_replace_multicursor(ShortcutJump *sj);
void annotation_display_replace_char(ShortcutJump *sj);
void annotation_display_shortcut_char(ShortcutJump *sj);
void annotation_display_replace_string(ShortcutJump *sj);

#endif
