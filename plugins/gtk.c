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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "lxhotkey.h"

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

typedef struct {
    const gchar *wm;
    const LXHotkeyPluginInit *cb;
    gpointer config;
} PluginData;

static const char menu_xml[] =
"<menubar>"
  "<menu action='FileMenu'>"
    "<menuitem action='Save'/>"
    "<menuitem action='Quit'/>"
  "</menu>"
  "<menu action='EditMenu'>"
    "<menuitem action='New'/>"
    "<menuitem action='Del'/>"
    "<menuitem action='Edit'/>"
  "</menu>"
  "<menu action='HelpMenu'>"
    "<menuitem action='About'/>"
  "</menu>"
"</menubar>"
"<toolbar>"
    "<toolitem action='Save'/>"
    "<separator/>"
    "<toolitem action='New'/>"
    "<toolitem action='Del'/>"
    "<toolitem action='Edit'/>"
"</toolbar>";

static void on_save(GtkAction *act, PluginData *data)
{
}

static void on_quit(GtkAction *act, PluginData *data)
{
    gtk_main_quit();
}

static void on_new(GtkAction *act, PluginData *data)
{
}

static void on_del(GtkAction *act, PluginData *data)
{
}

static void on_edit(GtkAction *act, PluginData *data)
{
}

static void on_about(GtkAction *act, PluginData *data)
{
}

static GtkActionEntry actions[] =
{
    { "FileMenu", NULL, N_("_File"), NULL, NULL, NULL },
        { "Save", GTK_STOCK_SAVE, NULL, NULL, NULL, G_CALLBACK(on_save) },
        { "Quit", GTK_STOCK_QUIT, NULL, NULL, NULL, G_CALLBACK(on_quit) },
    { "EditMenu", NULL, N_("_Edit"), NULL, NULL, NULL },
        { "New", GTK_STOCK_NEW, NULL, NULL, NULL, G_CALLBACK(on_new) },
        { "Del", GTK_STOCK_DELETE, NULL, "", NULL, G_CALLBACK(on_del) },
        { "Edit", GTK_STOCK_EDIT, NULL, NULL, NULL, G_CALLBACK(on_edit) },
    { "HelpMenu", NULL, N_("_Help"), NULL, NULL, NULL },
        { "About", GTK_STOCK_ABOUT, NULL, NULL, NULL, G_CALLBACK(on_about) }
};

static void on_notebook_switch_page(GtkNotebook *nb, gpointer *page, guint num,
                                    PluginData *data)
{
}

static void module_gtk_run(const gchar *wm, const LXHotkeyPluginInit *cb,
                           gpointer config, GError **error)
{
    GtkUIManager *ui;
    GtkActionGroup *act_grp;
    GtkAccelGroup *accel_grp;
    GtkWidget *win, *menubar;
    GtkToolbar *toolbar;
    GtkBox *vbox;
    GtkWidget *acts, *apps;
    GtkNotebook *notebook;
    PluginData data;
    int i = 0;

    gtk_init(&i, NULL);

    data.wm = wm;
    data.cb = cb;
    data.config = config;

    win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(GTK_WINDOW(win), 400, 300);
    g_signal_connect(win, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    vbox = (GtkBox *)gtk_vbox_new(FALSE, 0);

    /* create menu bar and toolbar */
    ui = gtk_ui_manager_new();
    act_grp = gtk_action_group_new("Main");
    gtk_action_group_set_translation_domain(act_grp, NULL);
    gtk_action_group_add_actions(act_grp, actions, G_N_ELEMENTS(actions), &data);

    accel_grp = gtk_ui_manager_get_accel_group(ui);
    gtk_window_add_accel_group(GTK_WINDOW(win), accel_grp);

    gtk_ui_manager_insert_action_group(ui, act_grp, 0);
    gtk_ui_manager_add_ui_from_string(ui, menu_xml, -1, NULL);
    g_object_unref(act_grp);

    menubar = gtk_ui_manager_get_widget(ui, "/menubar");
    toolbar = GTK_TOOLBAR(gtk_ui_manager_get_widget(ui, "/toolbar"));

    /* FIXME: use some style? */
    gtk_toolbar_set_icon_size(toolbar, GTK_ICON_SIZE_SMALL_TOOLBAR);
    gtk_toolbar_set_style(toolbar, GTK_TOOLBAR_ICONS);

    gtk_box_pack_start(vbox, menubar, FALSE, TRUE, 0);
    gtk_box_pack_start(vbox, GTK_WIDGET(toolbar), FALSE, TRUE, 0);

    /* notebook - it contains two tabs: Actions and Programs */
    notebook = (GtkNotebook*)gtk_notebook_new();
    gtk_notebook_set_scrollable(notebook, TRUE);
    gtk_container_set_border_width(GTK_CONTAINER(notebook), 0);

    g_signal_connect_after(notebook, "switch-page",
                           G_CALLBACK(on_notebook_switch_page), &data);

    gtk_box_pack_start(vbox, GTK_WIDGET(notebook), TRUE, TRUE, 0);

    /* setup notebook */
    acts = gtk_tree_view_new();
    //...
    
    gtk_notebook_append_page(notebook, acts, gtk_label_new(_("Actions")));

    apps = gtk_tree_view_new();
    //...
    
    gtk_notebook_append_page(notebook, apps, gtk_label_new(_("Programs")));

    /* and finally run it all */
    gtk_container_add(GTK_CONTAINER(win), GTK_WIDGET(vbox));
    gtk_widget_show_all(win);
    gtk_main();
}

static void module_gtk_alert(GError *error)
{
}

FM_DEFINE_MODULE(lxhotkey_gui, gtk)

LXHotkeyGUIPluginInit fm_module_init_lxhotkey_gui = {
    .run = module_gtk_run,
    .alert = module_gtk_alert
};
