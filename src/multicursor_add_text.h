#ifndef MULTICURSOR_ADD_TEXT_H_
#define MULTICURSOR_ADD_TEXT_H_

#include "jump_to_a_word.h"

void multicursor_add_text_from_search(ShortcutJump *sj, Word word);
void multicursor_add_text_from_selection(ShortcutJump *sj, gint start, gint end);

#endif
