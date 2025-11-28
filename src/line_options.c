/*
   Jump to a Word - Move the cursor to a word in Geany
   Copyright (C) 2025 01mu <github.com/01mu>

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

// sourced from the commander plugin

#include <plugindata.h>

#include "jump_to_a_word.h"
#include "line_options.h"
#include "preferences.h"
#include "util.h"

extern const struct {
    gchar *label;
    LineAfter type;
} line_conf[];

extern const struct {
    gchar *label;
    TextAfter type;
} text_conf[];

extern const struct {
    gchar *label;
    ReplaceAction type;
} replace_conf[];

static void tree_view_activate_focused_row(GtkTreeView *view) {
    GtkTreePath *path;
    GtkTreeViewColumn *column;
    gtk_tree_view_get_cursor(view, &path, &column);
    if (path) {
        gtk_tree_view_row_activated(view, path, column);
        gtk_tree_path_free(path);
    }
}

static void on_entry_activate(GtkEntry *entry, gpointer dummy) {
    ShortcutJump *sj = (ShortcutJump *)dummy;
    tree_view_activate_focused_row(GTK_TREE_VIEW(sj->tl_window->view));
}

static void tree_view_set_cursor_from_iter(GtkTreeView *view, GtkTreeIter *iter) {
    GtkTreePath *path = gtk_tree_model_get_path(gtk_tree_view_get_model(view), iter);
    gtk_tree_view_set_cursor(view, path, NULL, FALSE);
    gtk_tree_path_free(path);
}

static void tree_view_move_focus(GtkTreeView *view, GtkMovementStep step, gint amount) {
    GtkTreeIter iter;
    GtkTreePath *path;
    GtkTreeModel *model = gtk_tree_view_get_model(view);
    gboolean valid = FALSE;
    gtk_tree_view_get_cursor(view, &path, NULL);
    if (!path) {
        valid = gtk_tree_model_get_iter_first(model, &iter);
    } else {
        switch (step) {
        case GTK_MOVEMENT_BUFFER_ENDS:
            valid = gtk_tree_model_get_iter_first(model, &iter);
            if (valid && amount > 0) {
                GtkTreeIter prev;
                do {
                    prev = iter;
                } while (gtk_tree_model_iter_next(model, &iter));
                iter = prev;
            }
            break;
        case GTK_MOVEMENT_PAGES:
        case GTK_MOVEMENT_DISPLAY_LINES:
            gtk_tree_model_get_iter(model, &iter, path);
            if (amount > 0) {
                while ((valid = gtk_tree_model_iter_next(model, &iter)) && --amount > 0)
                    ;
            } else if (amount < 0) {
                while ((valid = gtk_tree_path_prev(path)) && --amount > 0)
                    ;
                if (valid) {
                    gtk_tree_model_get_iter(model, &iter, path);
                }
            }
            break;
        default:
            g_assert_not_reached();
        }
        gtk_tree_path_free(path);
    }
    if (valid) {
        tree_view_set_cursor_from_iter(view, &iter);
    } else {
        gtk_widget_error_bell(GTK_WIDGET(view));
    }
}

static void fill_store_line(const ShortcutJump *sj, GtkListStore *store) {
    for (gint i = 0; i < LA_COUNT; i++) {
        gchar *label;
        if (sj->config_settings->line_after == line_conf[i].type) {
            label = g_markup_printf_escaped("%i. <b>%s</b>", i + 1, line_conf[i].label);
        } else {
            label = g_markup_printf_escaped("%i. %s", i + 1, line_conf[i].label);
        }
        gtk_list_store_insert_with_values(store, NULL, line_conf[i].type, COL_LABEL, label, COL_TYPE, line_conf[i].type,
                                          -1);
    }
}

static void fill_store_text(const ShortcutJump *sj, GtkListStore *store) {
    for (gint i = 0; i < TX_COUNT; i++) {
        gchar *label;
        if (sj->config_settings->text_after == text_conf[i].type) {
            label = g_markup_printf_escaped("%i. <b>%s</b>", i + 1, text_conf[i].label);
        } else {
            label = g_markup_printf_escaped("%i. %s", i + 1, text_conf[i].label);
        }
        gtk_list_store_insert_with_values(store, NULL, text_conf[i].type, COL_LABEL, label, COL_TYPE, text_conf[i].type,
                                          -1);
    }
}

static void fill_store_replace(const ShortcutJump *sj, GtkListStore *store) {
    for (gint i = 0; i < RA_COUNT; i++) {
        gchar *label;
        if (sj->config_settings->replace_action == replace_conf[i].type) {
            label = g_markup_printf_escaped("%i. <b>%s</b>", i + 1, replace_conf[i].label);
        } else {
            label = g_markup_printf_escaped("%i. %s", i + 1, replace_conf[i].label);
        }
        gtk_list_store_insert_with_values(store, NULL, replace_conf[i].type, COL_LABEL, label, COL_TYPE,
                                          replace_conf[i].type, -1);
    }
}

static inline gint get_score(const gchar *needle, const gchar *haystack) {
    if (!needle || !haystack) {
        return needle == NULL;
    } else if (!*needle || !*haystack) {
        return *needle == 0;
    }
    if (IS_SEPARATOR(*haystack)) {
        return get_score(needle + IS_SEPARATOR(*needle), haystack + 1);
    }
    if (IS_SEPARATOR(*needle)) {
        return get_score(needle + 1, next_separator(haystack));
    }
    if (*needle == *haystack) {
        gint a = get_score(needle + 1, haystack + 1) + 1 + IS_SEPARATOR(haystack[1]);
        gint b = get_score(needle, next_separator(haystack));
        return MAX(a, b);
    } else {
        return get_score(needle, next_separator(haystack));
    }
}

static const gchar *get_key(gint *type_, ShortcutJump *sj) {
    gint type = COL_TYPE_ANY;
    const gchar *key = gtk_entry_get_text(GTK_ENTRY(sj->tl_window->entry));
    if (type_) {
        *type_ = type;
    }
    return key;
}

static gint key_score(const gchar *key_, const gchar *text_) {
    gchar *text = g_utf8_casefold(text_, -1);
    gchar *key = g_utf8_casefold(key_, -1);
    gint score = get_score(key, text);
    g_free(text);
    g_free(key);
    return score;
}

static gint sort_func(GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, gpointer dummy) {
    ShortcutJump *sj = (ShortcutJump *)dummy;
    gint scorea;
    gint scoreb;
    gchar *labela;
    gchar *labelb;
    gint type;
    const gchar *key = get_key(&type, sj);
    gtk_tree_model_get(model, a, COL_LABEL, &labela, -1);
    gtk_tree_model_get(model, b, COL_LABEL, &labelb, -1);
    scorea = key_score(key, labela);
    scoreb = key_score(key, labelb);
    g_free(labela);
    g_free(labelb);
    return scoreb - scorea;
}

static void on_entry_text_notify(GObject *object, GParamSpec *pspec, gpointer dummy) {
    ShortcutJump *sj = (ShortcutJump *)dummy;
    GtkTreeIter iter;
    GtkTreeView *view = GTK_TREE_VIEW(sj->tl_window->view);
    GtkTreeModel *model = gtk_tree_view_get_model(view);
    const gchar *entry_text = gtk_entry_get_text(GTK_ENTRY(sj->tl_window->entry));
    gchar *endptr = NULL;
    gint value = (gint)g_ascii_strtoll(entry_text, &endptr, 10);
    if (sj->option_mod == OM_LINE && (value > 0 && value <= LA_COUNT)) {
        sj->config_settings->line_after = value - 1;
        ui_set_statusbar(TRUE, _("Line action set to \"%s.\""), line_conf[value - 1].label);
        update_settings(SOURCE_OPTION_MENU, sj);
        gtk_widget_hide(sj->tl_window->panel);
    }
    if (sj->option_mod == OM_TEXT && (value > 0 && value <= TX_COUNT)) {
        sj->config_settings->text_after = value - 1;
        ui_set_statusbar(TRUE, _("Text action set to \"%s.\""), text_conf[value - 1].label);
        update_settings(SOURCE_OPTION_MENU, sj);
        gtk_widget_hide(sj->tl_window->panel);
    }
    if (sj->option_mod == OM_REPLACE && (value > 0 && value <= RA_COUNT)) {
        sj->config_settings->replace_action = value - 1;
        ui_set_statusbar(TRUE, _("Replace action set to \"%s.\""), replace_conf[value - 1].label);
        update_settings(SOURCE_OPTION_MENU, sj);
        gtk_widget_hide(sj->tl_window->panel);
    }
    gtk_tree_model_sort_reset_default_sort_func(GTK_TREE_MODEL_SORT(model));
    gtk_tree_sortable_set_default_sort_func(GTK_TREE_SORTABLE(model), sort_func, sj, NULL);
    if (gtk_tree_model_get_iter_first(model, &iter)) {
        tree_view_set_cursor_from_iter(view, &iter);
    }
}

static void on_view_row_activated(GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, gpointer dummy) {
    ShortcutJump *sj = (ShortcutJump *)dummy;
    GtkTreeModel *model = gtk_tree_view_get_model(view);
    GtkTreeIter iter;
    if (gtk_tree_model_get_iter(model, &iter, path)) {
        gint type;
        gchar *label;
        gtk_tree_model_get(model, &iter, COL_TYPE, &type, -1);
        gtk_tree_model_get(model, &iter, COL_LABEL, &label, -1);
        if (sj->option_mod == OM_LINE) {
            ui_set_statusbar(TRUE, _("Line action set to \"%s.\""), line_conf[type].label);
            sj->config_settings->line_after = type;
        } else if (sj->option_mod == OM_TEXT) {
            ui_set_statusbar(TRUE, _("Text action set to \"%s.\""), text_conf[type].label);
            sj->config_settings->text_after = type;
        } else if (sj->option_mod == OM_REPLACE) {
            ui_set_statusbar(TRUE, _("Replace action set to \"%s.\""), replace_conf[type].label);
            sj->config_settings->replace_action = type;
        }
        update_settings(SOURCE_OPTION_MENU, sj);
        gtk_widget_hide(sj->tl_window->panel);
    }
}

static gboolean on_panel_key_press_event(GtkWidget *widget, GdkEventKey *event, gpointer dummy) {
    ShortcutJump *sj = (ShortcutJump *)dummy;
    switch (event->keyval) {
    case GDK_KEY_Escape:
        gtk_widget_hide(widget);
        return TRUE;
    case GDK_KEY_Tab:
        return TRUE;
    case GDK_KEY_Return:
    case GDK_KEY_KP_Enter:
    case GDK_KEY_ISO_Enter:
        tree_view_activate_focused_row(GTK_TREE_VIEW(sj->tl_window->view));
        return TRUE;
    case GDK_KEY_Page_Up:
    case GDK_KEY_Page_Down:
        tree_view_move_focus(GTK_TREE_VIEW(sj->tl_window->view), GTK_MOVEMENT_PAGES,
                             event->keyval == GDK_KEY_Page_Up ? -1 : 1);
        return TRUE;
    case GDK_KEY_Up:
    case GDK_KEY_Down: {
        tree_view_move_focus(GTK_TREE_VIEW(sj->tl_window->view), GTK_MOVEMENT_DISPLAY_LINES,
                             event->keyval == GDK_KEY_Up ? -1 : 1);
        return TRUE;
    }
    }
    return FALSE;
}

static void on_panel_hide(GtkWidget *widget, gpointer dummy) {
    ShortcutJump *sj = (ShortcutJump *)dummy;
    GtkTreeView *view = GTK_TREE_VIEW(sj->tl_window->view);
    if (sj->tl_window->last_path) {
        gtk_tree_path_free(sj->tl_window->last_path);
        sj->tl_window->last_path = NULL;
    }
    gtk_tree_view_get_cursor(view, &sj->tl_window->last_path, NULL);
    gtk_list_store_clear(sj->tl_window->store);
}

static void on_panel_show(GtkWidget *widget, gpointer dummy) {
    ShortcutJump *sj = (ShortcutJump *)dummy;
    GtkTreePath *path;
    GtkTreeView *view = GTK_TREE_VIEW(sj->tl_window->view);
    if (sj->option_mod == OM_LINE) {
        fill_store_line(sj, sj->tl_window->store);
    } else if (sj->option_mod == OM_TEXT) {
        fill_store_text(sj, sj->tl_window->store);
    } else if (sj->option_mod == OM_REPLACE) {
        fill_store_replace(sj, sj->tl_window->store);
    }
    gtk_widget_grab_focus(sj->tl_window->entry);
    if (sj->tl_window->last_path) {
        gtk_tree_view_set_cursor(view, sj->tl_window->last_path, NULL, FALSE);
        gtk_tree_view_scroll_to_cell(view, sj->tl_window->last_path, NULL, TRUE, 0.5, 0.5);
    }
    gtk_tree_view_get_cursor(view, &path, NULL);
    if (path) {
        gtk_tree_path_free(path);
    } else {
        GtkTreeIter iter;
        if (gtk_tree_model_get_iter_first(gtk_tree_view_get_model(view), &iter)) {
            tree_view_set_cursor_from_iter(GTK_TREE_VIEW(sj->tl_window->view), &iter);
        }
    }
}

static void create_panel(ShortcutJump *sj) {
    if (sj->multicursor_mode == MC_DISABLED) {
        cancel_actions(sj);
    }

    sj->tl_window->panel =
        g_object_new(GTK_TYPE_WINDOW, "decorated", FALSE, "default-width", 275, "default-height", 275, "transient-for",
                     sj->geany_data->main_widgets->window, "window-position", GTK_WIN_POS_CENTER_ON_PARENT, "type-hint",
                     GDK_WINDOW_TYPE_HINT_DIALOG, "skip-taskbar-hint", TRUE, "skip-pager-hint", TRUE, NULL);

    GtkWidget *frame = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
    gtk_container_add(GTK_CONTAINER(sj->tl_window->panel), frame);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(frame), box);

    sj->tl_window->entry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(box), sj->tl_window->entry, FALSE, TRUE, 0);

    sj->tl_window->store = gtk_list_store_new(COL_COUNT, G_TYPE_STRING, G_TYPE_INT);

    sj->tl_window->sort = gtk_tree_model_sort_new_with_model(GTK_TREE_MODEL(sj->tl_window->store));
    gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(sj->tl_window->sort),
                                         GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, GTK_SORT_ASCENDING);

    GtkWidget *scroll = g_object_new(GTK_TYPE_SCROLLED_WINDOW, "hscrollbar-policy", GTK_POLICY_AUTOMATIC,
                                     "vscrollbar-policy", GTK_POLICY_AUTOMATIC, NULL);

    gtk_box_pack_start(GTK_BOX(box), scroll, TRUE, TRUE, 0);

    sj->tl_window->view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(sj->tl_window->sort));
    gtk_widget_set_can_focus(sj->tl_window->view, FALSE);
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(sj->tl_window->view), FALSE);

    GtkCellRenderer *cell = gtk_cell_renderer_text_new();
    g_object_set(cell, "ellipsize", PANGO_ELLIPSIZE_END, NULL);

    GtkTreeViewColumn *col = gtk_tree_view_column_new_with_attributes(NULL, cell, "markup", COL_LABEL, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(sj->tl_window->view), col);
    gtk_container_add(GTK_CONTAINER(scroll), sj->tl_window->view);

    g_signal_connect(sj->tl_window->view, "row-activated", G_CALLBACK(on_view_row_activated), sj);
    g_signal_connect(sj->tl_window->panel, "focus-out-event", G_CALLBACK(gtk_widget_hide), NULL);
    g_signal_connect(sj->tl_window->panel, "show", G_CALLBACK(on_panel_show), sj);
    g_signal_connect(sj->tl_window->panel, "hide", G_CALLBACK(on_panel_hide), sj);
    g_signal_connect(sj->tl_window->panel, "key-press-event", G_CALLBACK(on_panel_key_press_event), sj);
    g_signal_connect(sj->tl_window->entry, "notify::text", G_CALLBACK(on_entry_text_notify), sj);
    g_signal_connect(sj->tl_window->entry, "activate", G_CALLBACK(on_entry_activate), NULL);

    gtk_widget_show_all(frame);
}

gboolean open_line_options_kb(GeanyKeyBinding *kb, guint key_id, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;
    sj->option_mod = OM_LINE;
    create_panel(sj);
    gtk_widget_show(sj->tl_window->panel);
    return TRUE;
}

gboolean open_text_options_kb(GeanyKeyBinding *kb, guint key_id, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;
    sj->option_mod = OM_TEXT;
    create_panel(sj);
    gtk_widget_show(sj->tl_window->panel);
    return TRUE;
}

gboolean open_replace_options_kb(GeanyKeyBinding *kb, guint key_id, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;
    sj->option_mod = OM_REPLACE;
    create_panel(sj);
    gtk_widget_show(sj->tl_window->panel);
    return TRUE;
}

void open_line_options_cb(GtkMenuItem *menu_item, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;
    sj->option_mod = OM_LINE;
    create_panel(sj);
    gtk_widget_show(sj->tl_window->panel);
}

void open_text_options_cb(GtkMenuItem *menu_item, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;
    sj->option_mod = OM_TEXT;
    create_panel(sj);
    gtk_widget_show(sj->tl_window->panel);
}

void open_replace_options_cb(GtkMenuItem *menu_item, gpointer user_data) {
    ShortcutJump *sj = (ShortcutJump *)user_data;
    sj->option_mod = OM_REPLACE;
    create_panel(sj);
    gtk_widget_show(sj->tl_window->panel);
}
