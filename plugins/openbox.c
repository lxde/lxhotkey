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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "lxkeys.h"

#include <libfm/fm-extra.h>
#include <glib/gi18n.h>

typedef struct {
    char *path;
    FmXmlFile *xml;
    GList *actions; /* no-exec actions */
    GList *execs; /* exec-only actions */
    //GList *available_wm_actions;
    //GList *available_app_actions;
} ObXmlFile;

static FmXmlFileTag ObXmlFile_keyboard; /* section that we work on */
static FmXmlFileTag ObXmlFile_keybind; /* subsection, for each binding */
static FmXmlFileTag ObXmlFile_action; /* may be multiple for a binding */
static FmXmlFileTag ObXmlFile_command; /* for <action name="Execute"> */
static FmXmlFileTag ObXmlFile_execute; /* obsolete alias for command */

static gboolean tag_handler_keyboard(FmXmlFileItem *item, GList *children,
                                     char * const *attribute_names,
                                     char * const *attribute_values,
                                     guint n_attributes, gint line, gint pos,
                                     GError **error, gpointer user_data)
{
    /* anything to do? */
    return TRUE;
}

static gboolean tag_handler_keybind(FmXmlFileItem *item, GList *children,
                                    char * const *attribute_names,
                                    char * const *attribute_values,
                                    guint n_attributes, gint line, gint pos,
                                    GError **error, gpointer user_data)
{
    /* decide where to put: execs or actions */
    /* create LXKeysApp or LXKeysGlobal */
}

static gboolean tag_handler_action(FmXmlFileItem *item, GList *children,
                                   char * const *attribute_names,
                                   char * const *attribute_values,
                                   guint n_attributes, gint line, gint pos,
                                   GError **error, gpointer user_data)
{
    
}

static gboolean tag_handler_command(FmXmlFileItem *item, GList *children,
                                    char * const *attribute_names,
                                    char * const *attribute_values,
                                    guint n_attributes, gint line, gint pos,
                                    GError **error, gpointer user_data)
{
}

static gpointer obcfg_load(gpointer config, GError **error)
{
    ObXmlFile *cfg = (ObXmlFile *)config;

    if (config) {
        /* just discard any changes if any exist */
        FmXmlFile *old_xml = cfg->xml;

        cfg->xml = fm_xml_file_new(old_xml);
        g_object_unref(old_xml);
    } else {
        /* prepare data */
        cfg = g_new0(ObXmlFile, 1);
        cfg->xml = fm_xml_file_new(NULL);
        /* register handlers */
        ObXmlFile_keyboard = fm_xml_file_set_handler(cfg->xml, "keyboard",
                                                     &tag_handler_keyboard, FALSE, NULL);
        ObXmlFile_keybind = fm_xml_file_set_handler(cfg->xml, "keybind",
                                                     &tag_handler_keybind, FALSE, NULL);
        ObXmlFile_action = fm_xml_file_set_handler(cfg->xml, "action",
                                                     &tag_handler_action, FALSE, NULL);
        ObXmlFile_command = fm_xml_file_set_handler(cfg->xml, "command",
                                                     &tag_handler_command, FALSE, NULL);
        ObXmlFile_execute = fm_xml_file_set_handler(cfg->xml, "execute",
                                                     &tag_handler_command, FALSE, NULL);
        
        /* let try to detect rc.xml file currently in use:
           with Lubuntu it's lubuntu-rc.xml, with lxde session it's lxde-rc.xml */
        
    }
    /* try to load ~/.config/openbox/$xml */
    
    /* if it does not exist then try to load $XDG_SYSTEM_CONFDIR/openbox/rc.xml */
    

    return cfg;
}

static gboolean obcfg_save(gpointer config, GError **error)
{
    ObXmlFile *cfg = (ObXmlFile *)config;

    /* save as ~/.config/openbox/$xml */
}

static void obcfg_free(gpointer config)
{
    ObXmlFile *cfg = (ObXmlFile *)config;

    g_free(cfg->path);
    g_object_unref(cfg->xml);
    g_free(cfg);
}

static GList *obcfg_get_wm_keys(gpointer config, const char *mask, GError **error)
{
}

static gboolean obcfg_set_wm_key(gpointer config, LXKeysGlobal *data, GError **error)
{
}

FM_DEFINE_MODULE(lxkeys, Openbox)

LXKeysPluginInit fm_module_init_lxkeys = {
    .load = obcfg_load,
    .save = obcfg_save,
    .free = obcfg_free,
    .get_wm_keys = obcfg_get_wm_keys,
    .set_wm_key = obcfg_set_wm_key
    // .get_wm_actions = obcfg_get_wm_actions,
    // .get_app_keys = obcfg_get_app_keys,
    // .set_app_key = obcfg_set_app_key
};
