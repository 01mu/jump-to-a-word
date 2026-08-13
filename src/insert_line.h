#ifndef INSERT_LINE_H_
#define INSERT_LINE_H_

#include "jump_to_a_word.h"

void multicursor_line_insert_cancel(ShortcutJump *sj);
void multicursor_line_insert_complete(ShortcutJump *sj);
void line_insert_from_multicursor(ShortcutJump *sj);
void line_insert_from_search(ShortcutJump *sj);
void line_insert_cancel(ShortcutJump *sj);
void line_insert_complete(ShortcutJump *sj);
void line_insert_end(ShortcutJump *sj);
void multicursor_line_insert_end(ShortcutJump *sj);

#endif
