#ifndef REPLACE_HANDLE_INPUT_H_
#define REPLACE_HANDLE_INPUT_H_

#include "jump_to_a_word.h"

void clear_occurrences(ShortcutJump *sj);
gboolean replace_handle_input(ShortcutJump *sj, GdkEventKey *event, gunichar keychar,
                              void complete_func(ShortcutJump *), void cancel_func(ShortcutJump *));

#endif
