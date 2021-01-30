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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "lxhotkey.h"
#include "edit.h"

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

static int inited = 0;

static const char menu_xml[] =
"<menubar>"
  "<menu action='FileMenu'>"
    "<menuitem action='Save'/>"
    "<menuitem action='Reload'/>"
    "<separator/>"
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
    "<toolitem action='Reload'/>"
    "<toolitem action='Save'/>"
    "<separator/>"
    "<toolitem action='New'/>"
    "<toolitem action='Del'/>"
    "<toolitem action='Edit'/>"
"</toolbar>";

static void set_actions_list(PluginData *data);
static void set_apps_list(PluginData *data);

void _main_refresh(PluginData *data)
{
    set_actions_list(data);
    set_apps_list(data);
}

static void on_reload(GtkAction *act, PluginData *data)
{
    GError *error = NULL;

    *data->config = data->cb->load(*data->config, &error);
    if (error)
    {
        _show_error(_("Problems loading configuration: "), error);
        g_error_free(error);
    }
    _main_refresh(data);
    gtk_action_set_sensitive(data->save_action, FALSE);
}

static void on_save(GtkAction *act, PluginData *data)
{
    GError *error = NULL;

    if (data->cb->save(*data->config, &error))
        gtk_action_set_sensitive(data->save_action, FALSE);
    else
    {
        _show_error(_("Problems saving configuration: "), error);
        g_error_free(error);
    }
}

static void on_quit(GtkAction *act, PluginData *data)
{
    gtk_main_quit();
}

static void on_new(GtkAction *act, PluginData *data)
{
    gtk_tree_selection_unselect_all(gtk_tree_view_get_selection(data->current_page));
    _edit_action(data, NULL);
}

static void on_del(GtkAction *act, PluginData *data)
{
    LXHotkeyGlobal *gl = NULL;
    LXHotkeyApp *app = NULL;
    GError *error = NULL;
    GtkTreeModel *model;
    GtkTreeIter iter;

    _edit_cleanup(data);
    if (data->current_page == data->acts)
    {
        /* global */
        if (gtk_tree_selection_get_selected(gtk_tree_view_get_selection(data->acts),
                                            &model, &iter))
            gtk_tree_model_get(model, &iter, 4, &gl, -1);
        if (gl)
        {
            LXHotkeyGlobal rem_act = *gl;

            rem_act.accel1 = rem_act.accel2 = NULL;
            if (data->cb->set_wm_key(*data->config, &rem_act, &error))
            {
                gtk_action_set_sensitive(data->save_action, TRUE);
                _main_refresh(data);
            }
            else
            {
                _show_error(_("Cannot delete keybinding: "), error);
                g_error_free(error);
            }
        }
    }
    else
    {
        /* application */
        if (gtk_tree_selection_get_selected(gtk_tree_view_get_selection(data->apps),
                                            &model, &iter))
            gtk_tree_model_get(model, &iter, 3, &app, -1);
        if (act)
        {
            LXHotkeyApp rem_app = *app;

            rem_app.accel1 = rem_app.accel2 = NULL;
            if (data->cb->set_app_key(*data->config, &rem_app, &error))
            {
                gtk_action_set_sensitive(data->save_action, TRUE);
                _main_refresh(data);
            }
            else
            {
                _show_error(_("Cannot delete keybinding: "), error);
                g_error_free(error);
            }
        }
    }
}

static void on_edit(GtkAction *act, PluginData *data)
{
    _edit_action(data, NULL);
}

static void on_about(GtkAction *act, PluginData *data)
{
    GtkAboutDialog *about;
    const gchar *authors[] = {
        "Andriy Grytsenko <andrej@rep.kiev.ua>",
        NULL
    };
    /* TRANSLATORS: Replace this string with your names, one name per line. */
    gchar *translators = _("translator-credits");

    about = GTK_ABOUT_DIALOG(gtk_about_dialog_new());
    gtk_window_set_icon_name(GTK_WINDOW(about), LXHOTKEY_ICON);
    gtk_about_dialog_set_version(about, VERSION);
    gtk_about_dialog_set_program_name(about, "LXHotkey"); //FIXME: translated?
    gtk_about_dialog_set_logo_icon_name(about, LXHOTKEY_ICON);

    gtk_about_dialog_set_copyright(about, _("Copyright (C) 2016-2021"));
    gtk_about_dialog_set_comments(about, _( "Keyboard shortcuts configurator"));
    gtk_about_dialog_set_license(about, "This program is free software; you can redistribute it and/or\n"
                                        "modify it under the terms of the GNU General Public License\n"
                                        "as published by the Free Software Foundation; either version 2\n"
                                        "of the License, or (at your option) any later version.\n"
                                        "\n"
                                        "This program is distributed in the hope that it will be useful,\n"
                                        "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
                                        "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
                                        "GNU General Public License for more details.\n"
                                        "\n"
                                        "You should have received a copy of the GNU General Public License\n"
                                        "along with this program; if not, write to the Free Software Foundation,\n"
                                        "Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.");
    gtk_about_dialog_set_website(about, "http://lxde.org/");
    gtk_about_dialog_set_authors(about, authors);
    gtk_about_dialog_set_translator_credits(about, translators);
    gtk_dialog_run(GTK_DIALOG(about));
    gtk_widget_destroy(GTK_WIDGET(about));
}

static GtkActionEntry actions[] =
{
    { "FileMenu", NULL, N_("_File"), NULL, NULL, NULL },
        { "Reload", GTK_STOCK_UNDO, N_("_Reload"), NULL,
                    N_("Drop all unsaved changes and reload all keys from Window Manager configuration"),
                    G_CALLBACK(on_reload) },
        { "Save", GTK_STOCK_SAVE, NULL, NULL,
                    N_("Save all changes and apply them to Window Manager configuration"),
                    G_CALLBACK(on_save) },
        { "Quit", GTK_STOCK_QUIT, NULL, NULL,
                    N_("Exit the application without saving"), G_CALLBACK(on_quit) },
    { "EditMenu", NULL, N_("_Edit"), NULL, NULL, NULL },
        { "New", GTK_STOCK_NEW, NULL, NULL, N_("Create new action"),
                    G_CALLBACK(on_new) },
        { "Del", GTK_STOCK_DELETE, NULL, "", N_("Remove selected action"),
                    G_CALLBACK(on_del) },
        { "Edit", GTK_STOCK_EDIT, NULL, NULL, N_("Change selected action"),
                    G_CALLBACK(on_edit) },
    { "HelpMenu", NULL, N_("_Help"), NULL, NULL, NULL },
        { "About", GTK_STOCK_ABOUT, NULL, NULL, NULL, G_CALLBACK(on_about) }
};

static void on_notebook_switch_page(GtkNotebook *nb, GtkTreeView *page, guint num,
                                    PluginData *data)
{
    gboolean has_selection;
    gboolean can_edit;

    _edit_cleanup(data); /* abort edit */
    data->current_page = page;
    has_selection = gtk_tree_selection_get_selected(gtk_tree_view_get_selection(page), NULL, NULL);
    if (page == data->acts)
        can_edit = (data->cb->set_wm_key != NULL);
    else /* apps page */
        can_edit = (data->cb->set_app_key != NULL);
    gtk_action_set_sensitive(data->del_action, has_selection && can_edit);
    gtk_action_set_sensitive(data->edit_action, has_selection && can_edit);
    gtk_action_set_sensitive(data->add_action, can_edit);
}

static void on_selection_changed(GtkTreeSelection *selection, PluginData *data)
{
    gboolean has_selection;

    if (gtk_tree_view_get_selection(data->current_page) != selection)
        return;

    _edit_cleanup(data); /* abort edit */
    has_selection = gtk_tree_selection_get_selected(selection, NULL, NULL);
    gtk_action_set_sensitive(data->del_action, has_selection);
    gtk_action_set_sensitive(data->edit_action, has_selection);
}

static void on_row_activated(GtkTreeView *view, GtkTreePath *path,
                             GtkTreeViewColumn *column, PluginData *data)
{
    _edit_action(data, NULL);
}

static void set_actions_list(PluginData *data)
{
    GtkListStore *model = gtk_list_store_new(5, G_TYPE_STRING, G_TYPE_STRING,
                                                G_TYPE_STRING, G_TYPE_STRING,
                                                G_TYPE_POINTER);
    GList *acts = data->cb->get_wm_keys(*data->config, "*", NULL);
    GList *l;
    LXHotkeyGlobal *act;
    LXHotkeyAttr *attr, *opt;
    char *val, *_val;
    GtkTreeIter iter;

    for (l = acts; l; l = l->next)
    {
        act = l->data;
        if (act->actions == NULL)
            continue;
        attr = act->actions->data;
        _val = val = NULL;
        opt = NULL;
        if (attr->subopts)
        {
            opt = attr->subopts->data;
            if (opt->values)
            {
                if (attr->subopts->next)
                    _val = val = g_strdup_printf("%s:%s, ...", _(opt->name), _((char *)opt->values->data));
                else
                    _val = val = g_strdup_printf("%s:%s", _(opt->name), _((char *)opt->values->data));
            }
            else if (attr->subopts->next)
                _val = val = g_strdup_printf("%s, ...", _(opt->name));
            else
                val = _(opt->name);
        }
        gtk_list_store_insert_with_values(model, &iter, -1, 0, _(attr->name),
                                                            1, val,
                                                            2, act->accel1,
                                                            3, act->accel2,
                                                            4, act, -1);
        g_free(_val);
        //FIXME: this is a stub, it should show something better than just first action
    }
    g_list_free(acts);
    gtk_tree_view_set_model(data->acts, GTK_TREE_MODEL(model));
    g_object_unref(model);
}

static void set_apps_list(PluginData *data)
{
    GtkListStore *model = gtk_list_store_new(4, G_TYPE_STRING, G_TYPE_STRING,
                                                G_TYPE_STRING, G_TYPE_POINTER);
    GList *apps = data->cb->get_app_keys(*data->config, "*", NULL);
    GList *l;
    LXHotkeyApp *app;
    GtkTreeIter iter;

    for (l = apps; l; l = l->next)
    {
        app = l->data;
        gtk_list_store_insert_with_values(model, &iter, -1, 0, app->exec,
                                                            1, app->accel1,
                                                            2, app->accel2,
                                                            3, app, -1);
    }
    g_list_free(apps);
    gtk_tree_view_set_model(data->apps, GTK_TREE_MODEL(model));
    g_object_unref(model);
}

static void module_gtk_run(const gchar *wm, const LXHotkeyPluginInit *cb,
                           gpointer *config, GError **error)
{
    GtkUIManager *ui;
    GtkActionGroup *act_grp;
    GtkAccelGroup *accel_grp;
    GtkWidget *win, *menubar;
    GtkToolbar *toolbar;
    GtkBox *vbox;
    GtkScrolledWindow *scrwin;
    PluginData data;

    if (!inited)
        gtk_init(&inited, NULL);
    inited = 1;

    /* force style for GtkComboBox, it's ugly when list reaches screen bottom */
#if GTK_CHECK_VERSION(3, 0, 0)
    GtkCssProvider *provider = gtk_css_provider_new();
    if (gtk_css_provider_load_from_data(provider,
                                        "#gtk-widget {\n"
                                        "-GtkComboBox-appears-as-list : 1;\n"
                                        "}\n", -1, NULL))
        gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
                                                  GTK_STYLE_PROVIDER(provider),
                                                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);
#else
    gtk_rc_parse_string("style 'default-style'\n"
                        "{\n"
                        "  GtkComboBox::appears-as-list = 1\n"
                        "}\n"
                        "class 'GtkWidget' style 'default-style'");
#endif

    memset(&data, 0, sizeof(data));
    data.wm = wm;
    data.cb = cb;
    data.config = config;

#if GTK_CHECK_VERSION(3, 0, 0)
    if (gtk_get_major_version() > 3 || gtk_get_minor_version() >= 2)
        data.use_primary = TRUE;
#else
    if (gtk_check_version(2, 24, 0) == NULL)
        data.use_primary = TRUE;
#endif

    win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(GTK_WINDOW(win), 800, 480);
    gtk_window_set_icon_name(GTK_WINDOW(win), LXHOTKEY_ICON);
    g_signal_connect(win, "unmap", G_CALLBACK(gtk_main_quit), NULL);

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

    data.save_action = gtk_ui_manager_get_action(ui, "/menubar/FileMenu/Save");
    gtk_action_set_sensitive(data.save_action, FALSE);
    data.add_action = gtk_ui_manager_get_action(ui, "/menubar/EditMenu/New");
    data.del_action = gtk_ui_manager_get_action(ui, "/menubar/EditMenu/Del");
    data.edit_action = gtk_ui_manager_get_action(ui, "/menubar/EditMenu/Edit");
    gtk_action_set_sensitive(data.del_action, FALSE);
    gtk_action_set_sensitive(data.edit_action, FALSE);

    /* FIXME: use some style? */
    gtk_toolbar_set_icon_size(toolbar, GTK_ICON_SIZE_SMALL_TOOLBAR);
    gtk_toolbar_set_style(toolbar, GTK_TOOLBAR_ICONS);

    gtk_box_pack_start(vbox, menubar, FALSE, TRUE, 0);
    gtk_box_pack_start(vbox, GTK_WIDGET(toolbar), FALSE, TRUE, 0);

    /* notebook - it contains two tabs: Actions and Programs */
    data.notebook = (GtkNotebook*)gtk_notebook_new();
    gtk_notebook_set_scrollable(data.notebook, TRUE);
    gtk_container_set_border_width(GTK_CONTAINER(data.notebook), 0);

    g_signal_connect_after(data.notebook, "switch-page",
                           G_CALLBACK(on_notebook_switch_page), &data);

    /* notebook should be in a scrolled window if there is no enough space */
    scrwin = (GtkScrolledWindow *)gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_add_with_viewport(scrwin, GTK_WIDGET(data.notebook));
    gtk_scrolled_window_set_policy(scrwin, GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);

    gtk_box_pack_start(vbox, GTK_WIDGET(scrwin), TRUE, TRUE, 0);

    /* setup notebook */
    if (cb->get_wm_keys)
    {
        data.acts = GTK_TREE_VIEW(gtk_tree_view_new());
        gtk_tree_view_insert_column_with_attributes(data.acts, 0, _("Action"),
                                                    gtk_cell_renderer_text_new(),
                                                    "text", 0, NULL);
        gtk_tree_view_insert_column_with_attributes(data.acts, 1, _("Option"),
                                                    gtk_cell_renderer_text_new(),
                                                    "text", 1, NULL);
        gtk_tree_view_insert_column_with_attributes(data.acts, 2, _("Hotkey 1"),
                                                    gtk_cell_renderer_text_new(),
                                                    "text", 2, NULL);
        gtk_tree_view_insert_column_with_attributes(data.acts, 3, _("Hotkey 2"),
                                                    gtk_cell_renderer_text_new(),
                                                    "text", 3, NULL);
        set_actions_list(&data);
        g_signal_connect(data.acts, "row-activated", G_CALLBACK(on_row_activated), &data);
        g_signal_connect(gtk_tree_view_get_selection(data.acts), "changed",
                         G_CALLBACK(on_selection_changed), &data);
        gtk_notebook_append_page(data.notebook, GTK_WIDGET(data.acts),
                                 gtk_label_new(_("Actions")));
    }

    if (cb->get_app_keys)
    {
        data.apps = GTK_TREE_VIEW(gtk_tree_view_new());
        gtk_tree_view_insert_column_with_attributes(data.apps, 0, _("Command"),
                                                    gtk_cell_renderer_text_new(),
                                                    "text", 0, NULL);
        gtk_tree_view_insert_column_with_attributes(data.apps, 1, _("Hotkey 1"),
                                                    gtk_cell_renderer_text_new(),
                                                    "text", 1, NULL);
        gtk_tree_view_insert_column_with_attributes(data.apps, 2, _("Hotkey 2"),
                                                    gtk_cell_renderer_text_new(),
                                                    "text", 2, NULL);
        set_apps_list(&data);
        g_signal_connect(data.apps, "row-activated", G_CALLBACK(on_row_activated), &data);
        g_signal_connect(gtk_tree_view_get_selection(data.apps), "changed",
                         G_CALLBACK(on_selection_changed), &data);
        gtk_notebook_append_page(data.notebook, GTK_WIDGET(data.apps),
                                 gtk_label_new(_("Programs")));
    }

    /* and finally run it all */
    data.current_page = (GtkTreeView *)gtk_notebook_get_nth_page(data.notebook, 0);
    gtk_container_add(GTK_CONTAINER(win), GTK_WIDGET(vbox));
    gtk_widget_show_all(win);
    gtk_main();
    _edit_cleanup(&data);
}

void _show_error(const char *prefix, GError *error)
{
    GtkWidget *dlg;

    if (error && error->message)
    {
        dlg = gtk_message_dialog_new(NULL, 0, GTK_MESSAGE_ERROR,
                                     GTK_BUTTONS_OK, "%s%s", prefix, error->message);
        gtk_window_set_title(GTK_WINDOW(dlg), _("Error"));
        gtk_window_set_keep_above(GTK_WINDOW(dlg), TRUE);
        gtk_dialog_run(GTK_DIALOG(dlg));
        gtk_widget_destroy(dlg);
    }
}

static void module_gtk_alert(GError *error)
{
    _show_error("", error);
}

static void module_gtk_init(int argc, char **argv)
{
    if (!inited)
        gtk_init(&argc, &argv);
    inited = 1;
}

FM_DEFINE_MODULE(lxhotkey_gui, gtk)

LXHotkeyGUIPluginInit fm_module_init_lxhotkey_gui = {
    .run = module_gtk_run,
    .alert = module_gtk_alert,
    .init = module_gtk_init
};
