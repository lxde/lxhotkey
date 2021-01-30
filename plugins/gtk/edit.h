/*
 * Copyright (C) 2016-2021 Andriy Grytsenko <andrej@rep.kiev.ua>
 *
 * This file is a part of LXHotkey project.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef _EDIT_H_
#define _EDIT_H_ 1

#include <gtk/gtk.h>

typedef struct {
    /* general data */
    const gchar *wm;
    const LXHotkeyPluginInit *cb;
    gpointer *config;
    /* main window elements */
    GtkNotebook *notebook;
    GtkTreeView *acts, *apps;
    GtkAction *save_action;
    GtkAction *add_action;
    GtkAction *del_action;
    GtkAction *edit_action;
    GtkTreeView *current_page;
    /* edit window elements */
    GtkWindow *edit_window;
    GList *edit_options_copy;
    const GList *edit_template;
    GtkWidget *edit_key1, *edit_key2;
    GtkEntry *edit_exec;
    GtkTreeView *edit_tree;
    /* edit window toolbar elements */
    GtkAction *edit_apply_button;
    GtkAction *add_option_button;
    GtkAction *rm_option_button;
    GtkAction *edit_option_button;
    GtkAction *add_suboption_button;
    /* edit frame elements */
    GtkWidget *edit_frame;
    GtkComboBox *edit_actions;
    GtkWidget *edit_option_name; //GtkLabel
    GtkWidget *edit_value_label;
    GtkEntry *edit_value;
    GtkComboBox *edit_values;
    GtkWidget *edit_value_num; //GtkSpinButton
    GtkWidget *edit_value_num_label; //GtkLabel
    gint edit_mode;
    gboolean use_primary;
} PluginData;

#define LXHOTKEY_ICON "preferences-desktop-keyboard"

void _main_refresh(PluginData *data);

void _edit_action(PluginData *data, GError **error);

void _edit_cleanup(PluginData *data);

void _show_error(const char *prefix, GError *error);

#endif /* _EDIT_H_ */
