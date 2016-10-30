/*
 * Copyright (C) 2016 Andriy Grytsenko <andrej@rep.kiev.ua>
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

#ifndef _LXKEYS_H_
#define _LXKEYS_H_ 1

#include <libfm/fm.h>

G_BEGIN_DECLS

/**
 * LXHotkeyAttr:
 * @name: action or option name
 * @values: (element-type char *): option value
 * @subopts: (element-type LXHotkeyAttr): (allow-none): list of suboptions
 * @desc: (allow-none): action or option description
 * @has_actions: %TRUE if @subopts contains actions, %FALSE if @subopts contains options
 * @has_value: %TRUE if option has value even if @subopts isn't %NULL
 *
 * Data descriptor for actions and options. Actions are ativated by keybinding.
 * Each action may contain arbitrary number of options that alter its execution.
 *
 * This data is also used in a result on LXHotkeyPluginInit:get_wm_actions() or
 * LXHotkeyPluginInit:get_app_options() call. In that case for each option the
 * @values list should be either %NULL if option accepts any value, or list of
 * acceptable values for the option ("#" value has a special meaning: integer
 * value matches it, the same as "%" for percent value). For such purpose it is
 * advisable to make @name and @values translateable constants because GUI might
 * represent them in target locale which might be convenient for users.
 *
 * The @desc appears in data returned by LXHotkeyPluginInit:get_wm_actions() or
 * LXHotkeyPluginInit:get_app_options() call, it has human-readable descriptions
 * for option or action which can be used in GUI tooltip.
 */
typedef struct {
    gchar *name;
    GList *values;
    GList *subopts;
    gchar *desc;
    gboolean has_actions;
    gboolean has_value;
} LXHotkeyAttr;

#ifdef WANT_OPTIONS_EQUAL
static gboolean values_equal(GList *vals1, GList *vals2)
{
    while (vals1 && vals2) {
        if (g_strcmp0(vals1->data, vals2->data) != 0)
            return FALSE;
        vals1 = vals1->next;
        vals2 = vals2->next;
    }
    return (vals1 == NULL && vals2 == NULL);
}

/**
 * options_equal
 * @opts1: (element-type LXHotkeyAttr): list of options
 * @opts2: (element-type LXHotkeyAttr): list of options
 *
 * Recursively compare two lists of options.
 *
 * Returns: %TRUE if two lists are equal.
 */
static gboolean options_equal(GList *opts1, GList *opts2)
{
    while (opts1 && opts2) {
        LXHotkeyAttr *attr1 = opts1->data, *attr2 = opts2->data;

        if (g_strcmp0(attr1->name, attr2->name) != 0)
            return FALSE;
        if (!values_equal(attr1->values, attr2->values))
            return FALSE;
        if (!options_equal(attr1->subopts, attr2->subopts))
            return FALSE;
        opts1 = opts1->next;
        opts2 = opts2->next;
    }
    return (opts1 == NULL && opts2 == NULL);
}
#endif /* WANT_OPTIONS_EQUAL */

/**
 * LXHotkeyGlobal:
 * @actions: (element-type LXHotkeyAttr): list of actions
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
} LXHotkeyGlobal;

/**
 * LXHotkeyApp:
 * @exec: a command line to execute
 * @options: (element-type LXHotkeyAttr): (allow-none): list of options
 * @accel1: a keybinding to activate @exec, in GDK accelerator format
 * @accel2: optional alternate keybinding to activate @exec
 * @data1: a pointer for using by WM plugin
 * @data2: a pointer for using by WM plugin
 *
 * Descriptor of a keybinding for a single command line action. The command
 * execution may be altered by some @options. See also #LXHotkeyGlobal.
 */
typedef struct {
    gchar *exec;
    GList *options;
    gchar *accel1;
    gchar *accel2;
    gpointer data1;
    gpointer data2;
} LXHotkeyApp;


/* WM support plugins */

#define FM_MODULE_lxhotkey_VERSION 1 /* version of this API */

/**
 * LXHotkeyPluginInit:
 * @load: callback to (re)load bindings from WM configuration
 * @save: callback to save bindings to WM configuration and apply them
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
 * Callbacks @get_wm_actions and @get_app_options return list that should be
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
    gboolean (*set_wm_key)(gpointer config, LXHotkeyGlobal *data, GError **error);
    GList *(*get_wm_actions)(gpointer config, GError **error);
    GList *(*get_app_keys)(gpointer config, const char *mask, GError **error);
    gboolean (*set_app_key)(gpointer config, LXHotkeyApp *data, GError **error);
    GList *(*get_app_options)(gpointer config, GError **error);
    /*< private >*/
    gpointer _reserved1;
    gpointer _reserved2;
    gpointer _reserved3;
} LXHotkeyPluginInit;

/**
 * This descriptor instance should be defined in each plugin code as main
 * entry point for plugin creation. Primitive plugin example follows:
 *
 * #include <lxhotkey/lxhotkey.h>
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
 * FM_DEFINE_MODULE(lxhotkey, NoWM)
 *
 * LXHotkeyPluginInit fm_module_init_lxhotkey = {
 *      .load = test_load,
 *      .save = test_save,
 *      .free = g_free
 * }
 */
extern LXHotkeyPluginInit fm_module_init_lxhotkey;


/* GUI plugins */

#define FM_MODULE_lxhotkey_gui_VERSION 1 /* version of this API */

/**
 * LXHotkeyGUIPluginInit:
 * @run: callback to run GUI
 * @alert: callback to show an error message
 *
 * The @run callback receives name of WM, pointer to callbacks, and pointer to
 * pointer to the config data which is already succesfully loaded and ready to
 * use. Config may be reloaded by plugin if needed.
 */
typedef struct {
    /*< public >*/
    void (*run)(const gchar *wm, const LXHotkeyPluginInit *cb, gpointer *config, GError **error);
    void (*alert)(GError *error);
    void (*init)(int argc, char **argv);
    /*< private >*/
    gpointer _reserved1;
    gpointer _reserved2;
    gpointer _reserved3;
} LXHotkeyGUIPluginInit;

/**
 * This descriptor instance should be defined in each plugin code as main
 * entry point for a GUI plugin creation.
 */
extern LXHotkeyGUIPluginInit fm_module_init_lxhotkey_gui;

G_END_DECLS

#endif /* _LXKEYS_H_ */
