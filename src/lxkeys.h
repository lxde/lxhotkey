/*
 * Copyright (C) 2016 Andriy Grytsenko <andrej@rep.kiev.ua>
 *
 * This file is a part of LXKeys project.
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

#ifndef _LXKEYS_H_
#define _LXKEYS_H_ 1

#include <libfm/fm.h>

G_BEGIN_DECLS

/**
 * LXKeysAttr:
 * @name: action or option name
 * @values: (element-type char *): option value
 * @subopts: (element-type LXKeysAttr): (allow-none): list of suboptions
 * @has_actions: %TRUE if @subopts contains actions, %FALSE if @subopts contains options
 *
 * Data descriptor for actions and options. Actions are ativated by keybinding.
 * Each action may contain arbitrary number of options that alter its execution.
 *
 * This data is also used in a result on LXKeysPluginInit:get_wm_actions() or
 * LXKeysPluginInit:get_app_options() call. In that case for each option the
 * @values list should be either %NULL if option accepts any value, or list of
 * acceptable values for the option ("#" value has a special meaning: integer
 * value matches it, the same as "%" for percent value). For such purpose it is
 * advisable to make @name and @values translateable constants because GUI might
 * represent them in target locale which might be convenient for users.
 */
typedef struct {
    gchar *name;
    GList *values;
    GList *subopts;
    gboolean has_actions;
} LXKeysAttr;

/**
 * LXKeysGlobal:
 * @actions: (element-type LXKeysAttr): list of actions
 * @accel1: a keybinding to activate @actions, in GDK accelerator format
 * @accel2: optional alternate keybinding to activate @actions
 * @data1: a pointer for using by WM plugin
 * @data2: a pointer for using by WM plugin
 *
 * Descriptor of a keybinding which isn't a single command line action.
 * The keybinding string is in format that looks like "<Control>a" or
 * "<Shift><Alt>F1" or "<Release>z" (note that not each WM supports last one
 * variant).
 */
typedef struct {
    GList *actions;
    gchar *accel1;
    gchar *accel2;
    gpointer data1;
    gpointer data2;
} LXKeysGlobal;

/**
 * LXKeysApp:
 * @exec: a command line to execute
 * @actions: (element-type LXKeysAttr): (allow-none): list of options
 * @accel1: a keybinding to activate @exec, in GDK accelerator format
 * @accel2: optional alternate keybinding to activate @exec
 * @data1: a pointer for using by WM plugin
 * @data2: a pointer for using by WM plugin
 *
 * Descriptor of a keybinding for a single command line action. The command
 * execution may be altered by some @options. See also #LXKeysGlobal.
 */
typedef struct {
    gchar *exec;
    GList *options;
    gchar *accel1;
    gchar *accel2;
    gpointer data1;
    gpointer data2;
} LXKeysApp;


/* WM support plugins */

#define FM_MODULE_lxkeys_VERSION 1 /* version of this API */

/**
 * LXKeysPluginInit:
 * @load: callback to (re)load bindings from WM configuration
 * @save: callback to save bindings to WM configuration
 * @free: callback to release allocated resources
 * @get_wm_keys: (allow-none): callback to get global keys by provided mask
 * @set_wm_key: (allow-none): callback to set a global key by provided data
 * @get_wm_actions: (allow-none): callback to get global actions list provided by WM
 * @get_app_keys: (allow-none): callback to get keys bound to commands
 * @set_app_key: (allow-none): callback to set a key for a command
 * @get_app_options: (allow-none): callback to get possible actions for commands
 *
 * Callbacks @get_wm_keys and @get_app_keys return list which should be freed
 * by caller (transfer container).
 * Callbacks @get_wm_actions and @get_app_actions return list that should be
 * not modified nor freed by caller (transfer none).
 * Callback @get_wm_keys returns list of keybindings by @mask which is a shell
 * style pattern for keys.
 * Callback @set_wm_key changes a keybinding for list of actions provided. If
 * @data::accel1 is %NULL then all keybindings for @data::actions will be
 * cleared, otherwise keybindings will be changed accordingly.
 */
typedef struct {
    /*< public >*/
    gpointer (*load)(gpointer config, GError **error);
    gboolean (*save)(gpointer config, GError **error);
    void (*free)(gpointer config);
    GList *(*get_wm_keys)(gpointer config, const char *mask, GError **error);
    gboolean (*set_wm_key)(gpointer config, LXKeysGlobal *data, GError **error);
    GList *(*get_wm_actions)(gpointer config, GError **error);
    GList *(*get_app_keys)(gpointer config, const char *mask, GError **error);
    gboolean (*set_app_key)(gpointer config, LXKeysApp *data, GError **error);
    GList *(*get_app_options)(gpointer config, GError **error);
    /*< private >*/
    gpointer _reserved1;
    gpointer _reserved2;
    gpointer _reserved3;
} LXKeysPluginInit;

/**
 * This descriptor instance should be defined in each plugin code as main
 * entry point for plugin creation. Primitive plugin example follows:
 *
 * #include <lxkeys/lxkeys.h>
 *
 * gpointer test_load(gpointer config, GError **error)
 * {
 *      if (config == NULL)
 *          config = g_strdup("");
 *      return config;
 * }
 *
 * gboolean test_save(gpointer config, GError **error)
 * {
 *      return FALSE;
 * }
 *
 * FM_DEFINE_MODULE(lxkeys, NoWM)
 *
 * LXKeysPluginInit fm_module_init_lxkeys = {
 *      .load = test_load,
 *      .save = test_save,
 *      .free = g_free
 * }
 */
extern LXKeysPluginInit fm_module_init_lxkeys;


/* GUI plugins */

#define FM_MODULE_lxkeys_gui_VERSION 1 /* version of this API */

/**
 * LXKeysGUIPluginInit:
 * @run: callback to run GUI
 * @alert: callback to show an error message
 *
 * The @run callback receives name of WM, pointer to callbacks, and pointer to
 * the config data which is already succesfully loaded and ready to use.
 */
typedef struct {
    /*< public >*/
    void (*run)(const gchar *wm, const LXKeysPluginInit *cb, gpointer config, GError **error);
    void (*alert)(GError *error);
    /*< private >*/
    gpointer _reserved1;
    gpointer _reserved2;
    gpointer _reserved3;
} LXKeysGUIPluginInit;

/**
 * This descriptor instance should be defined in each plugin code as main
 * entry point for a GUI plugin creation.
 */
extern LXKeysGUIPluginInit fm_module_init_lxkeys_gui;

G_END_DECLS

#endif /* _LXKEYS_H_ */
