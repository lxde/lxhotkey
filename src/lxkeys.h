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

/* data for actions and options */
typedef struct {
    gchar *name; /* action or option name */
    GList *values; /* elements are char*, multiple only for get_wm_actions() */
    GList *subopts; /* elements are LXKeysAttr */
    gboolean has_actions; /* TRUE if option contains actions instead of subopts */
} LXKeysAttr;

/* data containing action(s), description, and key(s) */
typedef struct {
    GList *actions; /* elements are LXKeysAttr, no value usually */
    gchar *accel1;
    gchar *accel2;
} LXKeysGlobal;

/* data containing exec line, description, and key(s)
   only for single-exec commands, multiple-action exec are handled as global */
typedef struct {
    gchar *exec;
    GList *actions; /* elements are LXKeysAttr, empty value acceptable */
    gchar *accel1;
    gchar *accel2;
} LXKeysApp;

/* for errors */
extern GQuark LXKEYS_ERROR;
typedef enum {
    LXKEYS_BAD_ARGS, /* invalid commandline arguments */
    LXKEYS_NOT_SUPPORTED /* operation not supported */
} LXKeysError;

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
 * @get_app_actions: (allow-none): callback to get possible actions for commands
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
    GList *(*get_app_actions)(gpointer config, GError **error);
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
 * LXKeysPluginInit:
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

extern LXKeysGUIPluginInit fm_module_init_lxkeys_gui;

G_END_DECLS

#endif /* _LXKEYS_H_ */
