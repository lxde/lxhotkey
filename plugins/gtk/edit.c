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

#define WANT_OPTIONS_EQUAL

#include "lxhotkey.h"
#include "edit.h"

#include <stdlib.h>
#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#if !GTK_CHECK_VERSION(2, 21, 0)
# define GDK_KEY_Tab            GDK_Tab
# define GDK_KEY_BackSpace      GDK_BackSpace
# define GDK_KEY_Escape         GDK_Escape
# define GDK_KEY_space          GDK_space
#endif

enum {
    EDIT_MODE_NONE,
    EDIT_MODE_ADD, /* add action */
    EDIT_MODE_EDIT, /* change selected */
    EDIT_MODE_OPTION /* add suboption */
};

static const LXHotkeyAttr *find_template_for_option(GtkTreeModel *model,
                                                    GtkTreeIter *iter,
                                                    const GList *t_list)
{
    const LXHotkeyAttr *opt, *tmpl;

    gtk_tree_model_get(model, iter, 2, &opt, -1);
    while (t_list)
    {
        tmpl = t_list->data;
        if (g_strcmp0(tmpl->name, opt->name) == 0)
            return tmpl;
        t_list = t_list->next;
    }
    return NULL;
}

static const GList *get_options_from_template(const LXHotkeyAttr *tmpl, PluginData *data)
{
    if (tmpl == NULL)
        return NULL;
    else if (tmpl->has_actions)
        return data->edit_template;
    else
        return tmpl->subopts;
}

static const GList *get_parent_template_list(GtkTreeModel *model, GtkTreeIter *iter,
                                             PluginData *data)
{
    const GList *tmpl_list;
    GtkTreeIter parent_iter;
    const LXHotkeyAttr *parent;

    if (!gtk_tree_model_iter_parent(model, &parent_iter, iter))
        return data->edit_template;
    /* get list for parent of parent first - recursion is here */
    tmpl_list = get_parent_template_list(model, &parent_iter, data);
    /* now find parent in that list */
    parent = find_template_for_option(model, &parent_iter, tmpl_list);
    return get_options_from_template(parent, data);
}

static void fill_edit_frame(PluginData *data, const LXHotkeyAttr *opt,
                            const GList *subopts, const GList *exempt)
{
    GtkListStore *names_store;
    const LXHotkeyAttr *sub;
    const GList *l;
    int i = 0;

    names_store = GTK_LIST_STORE(gtk_list_store_new(3, G_TYPE_STRING, /* description */
                                                       G_TYPE_STRING, /* name */
                                                       G_TYPE_POINTER)); /* template */
    while (subopts)
    {
        sub = subopts->data;
        /* ignore existing opts */
        for (l = exempt; l; l = l->next)
            if (strcmp(sub->name, ((LXHotkeyAttr *)l->data)->name) == 0)
                break;
        if (l == NULL)
            gtk_list_store_insert_with_values(names_store, NULL, i++, 0, _(sub->name),
                                                                      1, sub->name,
                                                                      2, sub, -1);
        subopts = subopts->next;
    }
    gtk_combo_box_set_model(data->edit_actions, GTK_TREE_MODEL(names_store));
    g_object_unref(names_store);
    gtk_combo_box_set_active(data->edit_actions, 0);
    /* values box will be set by changing active callback */
    gtk_widget_set_visible(GTK_WIDGET(data->edit_actions), opt == NULL); /* visible on add */
    gtk_widget_set_visible(data->edit_option_name, opt != NULL); /* visible on edit */
    if (opt)
        gtk_label_set_text(GTK_LABEL(data->edit_option_name), _(opt->name));
}

static void update_edit_toolbar(PluginData *data)
{
    const LXHotkeyAttr *opt, *tmpl;
    const GList *tmpl_list;
    GtkTreeModel *model;
    GtkTreeIter iter;
    gboolean is_action;

    /* update AddOption button -- exec only */
    if (gtk_action_get_visible(data->add_option_button))
    {
        /* make not clickable if no more options to add */
        gtk_action_set_sensitive(data->add_option_button,
                g_list_length((GList *)data->edit_template) != g_list_length(data->edit_options_copy));
    }
    /* update Remove button */
    if (!gtk_tree_selection_get_selected(gtk_tree_view_get_selection(data->edit_tree),
                                         &model, &iter))
    {
        gtk_action_set_sensitive(data->rm_option_button, FALSE);
        gtk_action_set_sensitive(data->edit_option_button, FALSE);
        gtk_action_set_sensitive(data->add_suboption_button, FALSE);
        return;
    }
    gtk_action_set_sensitive(data->rm_option_button, TRUE);
    gtk_tree_model_get(model, &iter, 2, &opt, -1);
    tmpl_list = get_parent_template_list(model, &iter, data);
    is_action = (data->current_page == data->acts && tmpl_list == data->edit_template);
    while (tmpl_list)
    {
        tmpl = tmpl_list->data;
        if (g_strcmp0(tmpl->name, opt->name) == 0)
            break;
        tmpl_list = tmpl_list->next;
    }
    if (G_UNLIKELY(tmpl_list == NULL))
    {
        /* option isn't supported, probably deprecated one */
        gtk_action_set_sensitive(data->edit_option_button, FALSE);
        gtk_action_set_sensitive(data->add_suboption_button, FALSE);
        return;
    }
    gtk_action_set_sensitive(data->edit_option_button,
                             !is_action && (tmpl->subopts == NULL || tmpl->has_value));
    gtk_action_set_sensitive(data->add_suboption_button,
                             (tmpl->has_actions ||
                              g_list_length(tmpl->subopts) != g_list_length(opt->subopts)));
}

static void on_cancel(GtkAction *action, PluginData *data)
{
    gtk_widget_destroy(GTK_WIDGET(data->edit_window));
}

static void on_save(GtkAction *action, PluginData *data)
{
    GError *error = NULL;
    LXHotkeyGlobal *act = NULL;
    LXHotkeyApp *app = NULL;
    GtkTreeModel *model;
    GtkTreeIter iter;
    LXHotkeyGlobal new_act;
    LXHotkeyApp new_app;
    gboolean ok = FALSE;

    if (data->current_page == data->acts)
    {
        /* it's global */
        new_act.accel1 = g_object_get_data(G_OBJECT(data->edit_key1), "accelerator_name");
        new_act.accel2 = g_object_get_data(G_OBJECT(data->edit_key2), "accelerator_name");
        if (new_act.accel1 == NULL || new_act.accel1[0] == 0)
        {
            new_act.accel1 = new_act.accel2;
            new_act.accel2 = NULL;
        }
        new_act.actions = data->edit_options_copy;
        if (gtk_tree_selection_get_selected(gtk_tree_view_get_selection(data->acts),
                                            &model, &iter))
            gtk_tree_model_get(model, &iter, 4, &act, -1);
        if (act)
        {
            /* global edited */
            if (!options_equal(act->actions, new_act.actions))
            {
                /* actions list changed, remove old binding and add new */
                LXHotkeyGlobal rem_act = *act;

                rem_act.accel1 = rem_act.accel2 = NULL;
                if (!data->cb->set_wm_key(*data->config, &rem_act, &error))
                    goto _exit;
            }
            else if (g_strcmp0(act->accel1, new_act.accel1) == 0 &&
                     g_strcmp0(act->accel2, new_act.accel2) == 0)
                /* nothing was changed */
                goto _exit;
        }
        /* else it was added */
        ok = data->cb->set_wm_key(*data->config, &new_act, &error);
    }
    else
    {
        /* it's application */
        new_app.accel1 = g_object_get_data(G_OBJECT(data->edit_key1), "accelerator_name");
        new_app.accel2 = g_object_get_data(G_OBJECT(data->edit_key2), "accelerator_name");
        if (new_app.accel1 == NULL || new_app.accel1[0] == 0)
        {
            new_app.accel1 = new_app.accel2;
            new_app.accel2 = NULL;
        }
        new_app.exec = (char *)gtk_entry_get_text(data->edit_exec);
        new_app.options = data->edit_options_copy;
        if (gtk_tree_selection_get_selected(gtk_tree_view_get_selection(data->apps),
                                            &model, &iter))
            gtk_tree_model_get(model, &iter, 3, &app, -1);
        if (app)
        {
            /* app edited */
            if (g_strcmp0(app->exec, new_app.exec) != 0 ||
                !options_equal(app->options, new_app.options))
            {
                /* exec line or options list changed, remove old binding and add new */
                LXHotkeyApp rem_app = *app;

                rem_app.accel1 = rem_app.accel2 = NULL;
                if (!data->cb->set_app_key(*data->config, &rem_app, &error))
                    goto _exit;
            }
            else if (g_strcmp0(app->accel1, new_app.accel1) == 0 &&
                     g_strcmp0(app->accel2, new_app.accel2) == 0)
                /* nothing was changed */
                goto _exit;
        }
        /* else it was added */
        ok = data->cb->set_app_key(*data->config, &new_app, &error);
    }

_exit:
    if (error)
    {
        _show_error(_("Apply error: "), error);
        g_error_free(error);
    }
    /* update main window */
    if (ok)
        gtk_action_set_sensitive(data->save_action, TRUE);
    gtk_widget_destroy(GTK_WIDGET(data->edit_window));
    if (ok)
        _main_refresh(data);
}

static void on_add_action(GtkAction *action, PluginData *data)
{
    /* fill frame with empty data, set choices from data->edit_template, hide value */
    data->edit_mode = EDIT_MODE_ADD;
    gtk_frame_set_label(GTK_FRAME(data->edit_frame), _("Add action"));
    fill_edit_frame(data, NULL, data->edit_template, NULL);
    gtk_widget_hide(GTK_WIDGET(data->edit_values));
    gtk_widget_hide(GTK_WIDGET(data->edit_value));
    gtk_widget_show(data->edit_frame);
    gtk_widget_grab_focus(data->edit_frame);
}

static void on_add_option(GtkAction *act, PluginData *data)
{
    /* fill frame with empty data, set choices from data->edit_template */
    data->edit_mode = EDIT_MODE_ADD;
    gtk_frame_set_label(GTK_FRAME(data->edit_frame), _("Add option"));
    fill_edit_frame(data, NULL, data->edit_template, data->edit_options_copy);
    gtk_widget_show(data->edit_frame);
    gtk_widget_grab_focus(data->edit_frame);
}

#define free_options(acts) g_list_free_full(acts, (GDestroyNotify)option_free)
static void option_free(LXHotkeyAttr *attr)
{
    g_free(attr->name);
    g_list_free_full(attr->values, g_free);
    free_options(attr->subopts);
    g_free(attr->desc);
    g_slice_free(LXHotkeyAttr, attr);
}

static void on_remove(GtkAction *act, PluginData *data)
{
    LXHotkeyAttr *opt, *parent;
    GtkTreeModel *model;
    GtkTreeIter iter, parent_iter;

    if (!gtk_tree_selection_get_selected(gtk_tree_view_get_selection(data->edit_tree),
                                         &model, &iter))
        /* no item selected */
        return;
    /* find and remove option from data->edit_options_copy */
    gtk_tree_model_get(model, &iter, 2, &opt, -1);
    if (gtk_tree_model_iter_parent(model, &parent_iter, &iter))
    {
        gtk_tree_model_get(model, &parent_iter, 2, &parent, -1);
        parent->subopts = g_list_remove(parent->subopts, opt);
    }
    else
        data->edit_options_copy = g_list_remove(data->edit_options_copy, opt);
    option_free(opt);
    /* remove selected row from model */
    gtk_tree_store_remove(GTK_TREE_STORE(model), &iter);
    gtk_action_set_sensitive(data->edit_apply_button, TRUE);
}

static void start_edit(GtkTreeModel *model, GtkTreeIter *iter, PluginData *data)
{
    const LXHotkeyAttr *opt;
    const GList *tmpl_list;
    GList single = { .prev = NULL, .next = NULL };

    /* name - only current from selection */
    gtk_tree_model_get(model, iter, 2, &opt, -1);
    /* values - from template */
    tmpl_list = get_parent_template_list(model, iter, data);
    if (data->current_page == data->acts &&
        tmpl_list == data->edit_template) /* it's action */
        return;
    single.data = (gpointer)find_template_for_option(model, iter, tmpl_list);
    if (single.data == NULL)
    {
        g_warning("no template found for option '%s'", opt->name);
        return;
    }
    /* fill frame from selection */
    data->edit_mode = EDIT_MODE_EDIT;
    gtk_frame_set_label(GTK_FRAME(data->edit_frame), _("Change option"));
    fill_edit_frame(data, opt, &single, NULL);
    gtk_widget_show(data->edit_frame);
    gtk_widget_grab_focus(data->edit_frame);
}

static void on_edit(GtkAction *act, PluginData *data)
{
    GtkTreeModel *model;
    GtkTreeIter iter;

    if (!gtk_tree_selection_get_selected(gtk_tree_view_get_selection(data->edit_tree),
                                         &model, &iter))
        /* no item selected */
        return;
    start_edit(model, &iter, data);
}

static void on_add_suboption(GtkAction *act, PluginData *data)
{
    const LXHotkeyAttr *opt, *tmpl;
    const GList *tmpl_list;
    GtkTreeModel *model;
    GtkTreeIter iter;

    if (!gtk_tree_selection_get_selected(gtk_tree_view_get_selection(data->edit_tree),
                                         &model, &iter))
        /* no item selected */
        return;

    tmpl_list = get_parent_template_list(model, &iter, data);
    tmpl = find_template_for_option(model, &iter, tmpl_list);
    if (tmpl == NULL)
        /* no options found */
        return;
    tmpl_list = get_options_from_template(tmpl, data);
    gtk_tree_model_get(model, &iter, 2, &opt, -1);
    data->edit_mode = EDIT_MODE_OPTION;
    /* fill frame with empty data and set name choices from selection's subopts */
    if (tmpl->has_actions)
        gtk_frame_set_label(GTK_FRAME(data->edit_frame), _("Add action"));
    else
        gtk_frame_set_label(GTK_FRAME(data->edit_frame), _("Add option"));
    fill_edit_frame(data, NULL, tmpl_list, opt->subopts);
    gtk_widget_show(data->edit_frame);
    gtk_widget_grab_focus(data->edit_frame);
}

static const char edit_xml[] =
"<toolbar>"
    "<toolitem action='Cancel'/>"
    "<toolitem action='Save'/>"
    "<separator/>"
    "<toolitem action='AddAction'/>"
    "<toolitem action='AddOption'/>"
    "<toolitem action='Remove'/>"
    "<toolitem action='Change'/>"
    "<separator/>"
    "<toolitem action='AddSubOption'/>"
    /* "<separator/>"
    "<toolitem action='Help'/>" */
"</toolbar>";

static GtkActionEntry actions[] =
{
    { "Cancel", GTK_STOCK_CANCEL, NULL, NULL, N_("Discard changes"), G_CALLBACK(on_cancel) },
    { "Save", GTK_STOCK_APPLY, NULL, NULL, N_("Accept changes"), G_CALLBACK(on_save) },
    { "AddAction", GTK_STOCK_NEW, NULL, NULL, N_("Add an action"), G_CALLBACK(on_add_action) },
    { "AddOption", GTK_STOCK_ADD, NULL, NULL, N_("Add an option to this command"),
                G_CALLBACK(on_add_option) },
    { "Remove", GTK_STOCK_DELETE, NULL, "", N_("Remove selection"), G_CALLBACK(on_remove) },
    { "Change", GTK_STOCK_EDIT, NULL, NULL, N_("Change selected option"), G_CALLBACK(on_edit) },
    { "AddSubOption", GTK_STOCK_ADD, NULL, NULL, N_("Add an option to selection"),
                G_CALLBACK(on_add_suboption) }
};

/* Button for keybinding click - taken from LXPanel, simplified a bit */
static void on_focus_in_event(GtkButton *test, GdkEvent *event, PluginData *data)
{
    gdk_keyboard_grab(gtk_widget_get_window(GTK_WIDGET(test)), TRUE, GDK_CURRENT_TIME);
}

static void on_focus_out_event(GtkButton *test, GdkEvent *event, PluginData *data)
{
    gdk_keyboard_ungrab(GDK_CURRENT_TIME);
}

static gboolean on_key_event(GtkButton *test, GdkEventKey *event, PluginData *data)
{
    GdkModifierType state;
    char *text;
    const char *label;

    /* ignore Tab completely so user can leave focus */
    if (event->keyval == GDK_KEY_Tab)
        return FALSE;
    /* request mods directly, event->state isn't updated yet */
    gdk_window_get_pointer(gtk_widget_get_window(GTK_WIDGET(test)),
                           NULL, NULL, &state);
    /* special support for Win key, it doesn't work sometimes */
    if ((state & GDK_SUPER_MASK) == 0 && (state & GDK_MOD4_MASK) != 0)
        state |= GDK_SUPER_MASK;
    state &= gtk_accelerator_get_default_mod_mask();
    /* if mod key event then update test label and go */
    if (event->is_modifier)
    {
        if (state != 0)
        {
            text = gtk_accelerator_get_label(0, state);
            gtk_button_set_label(test, text);
            g_free(text);
        }
        /* if no modifiers currently then show original state */
        else
            gtk_button_set_label(test, g_object_get_data(G_OBJECT(test), "original_label"));
        return FALSE;
    }
    /* if not keypress query then ignore key press */
    if (event->type != GDK_KEY_PRESS)
        return FALSE;
    /* if Escape pressed then reset to last saved */
    if (state == 0 && event->keyval == GDK_KEY_Escape)
    {
        gtk_button_set_label(test, g_object_get_data(G_OBJECT(test), "original_label"));
        goto _done;
    }
    /* if BackSpace pressed then just clear the button */
    if (state == 0 && event->keyval == GDK_KEY_BackSpace)
    {
        gtk_button_set_label(test, "");
        g_object_set_data(G_OBJECT(test), "accelerator_name", NULL);
        g_object_set_data(G_OBJECT(test), "original_label", NULL);
_done:
        gtk_action_set_sensitive(data->edit_apply_button,
                                 ((label = gtk_button_get_label(GTK_BUTTON(data->edit_key1))) && label[0]) ||
                                 ((gtk_button_get_label(GTK_BUTTON(data->edit_key2))) && label[0]));
        if (data->edit_exec)
            gtk_widget_grab_focus(GTK_WIDGET(data->edit_exec));
        else
            gtk_widget_grab_focus(GTK_WIDGET(data->edit_tree));
        return FALSE;
    }
    /* update the label now */
    text = gtk_accelerator_get_label(event->keyval, state);
    gtk_button_set_label(test, text);
    /* drop single printable and printable with single Shift, Ctrl, Alt */
    if (event->length != 0 && (state == 0 || state == GDK_SHIFT_MASK ||
                               state == GDK_CONTROL_MASK || state == GDK_MOD1_MASK) &&
        /* but make an exception for Alt+Space, it should be acceptable */
        (event->keyval != GDK_KEY_space || state != GDK_MOD1_MASK))
    {
        GtkWidget* dlg;
        dlg = gtk_message_dialog_new(NULL, 0, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
                                     _("Key combination '%s' cannot be used as"
                                       " a global hotkey, sorry."), text);
        g_free(text);
        gtk_window_set_title(GTK_WINDOW(dlg), _("Error"));
        gtk_window_set_keep_above(GTK_WINDOW(dlg), TRUE);
        gtk_dialog_run(GTK_DIALOG(dlg));
        gtk_widget_destroy(dlg);
        gtk_button_set_label(test, g_object_get_data(G_OBJECT(test), "original_label"));
        gtk_action_set_sensitive(data->edit_apply_button,
                                 ((label = gtk_button_get_label(GTK_BUTTON(data->edit_key1))) && label[0]) ||
                                 ((gtk_button_get_label(GTK_BUTTON(data->edit_key2))) && label[0]));
        return FALSE;
    }
    /* save new value now */
    g_object_set_data_full(G_OBJECT(test), "original_label", text, g_free);
    text = gtk_accelerator_name(event->keyval, state);
    if (data->use_primary)
    {
        /* convert <Primary> to <Control> now */
        char *subtext = strstr(text, "<Primary>");
        if (subtext != NULL)
            strncpy(subtext, "<Control", 8);
    }
    g_object_set_data_full(G_OBJECT(test), "accelerator_name", text, g_free);
    gtk_action_set_sensitive(data->edit_apply_button, TRUE);
    /* change focus onto exec line or actions tree now */
    if (data->edit_exec)
        gtk_widget_grab_focus(GTK_WIDGET(data->edit_exec));
    else
        gtk_widget_grab_focus(GTK_WIDGET(data->edit_tree));
    return FALSE;
}

static GtkWidget *key_button_new(PluginData *data, const char *hotkey)
{
    GtkWidget *w;
    char *label;
    guint keyval = 0;
    GdkModifierType state = 0;

    if (hotkey)
    {
        if (data->use_primary)
        {
            /* convert <Control> to <Primary> now */
            char *newkey = g_strdup(hotkey);
            char *subkey = strstr(newkey, "<Control>");
            if (subkey != NULL)
                strncpy(subkey, "<Primary", 8);
            gtk_accelerator_parse(newkey, &keyval, &state);
            g_free(newkey);
        }
        else
            gtk_accelerator_parse(hotkey, &keyval, &state);
    }
    label = gtk_accelerator_get_label(keyval, state);
    w = gtk_button_new_with_label(label);
    g_object_set_data_full(G_OBJECT(w), "accelerator_name", g_strdup(hotkey), g_free);
    g_object_set_data_full(G_OBJECT(w), "original_label", label, g_free);
    g_signal_connect(w, "focus-in-event", G_CALLBACK(on_focus_in_event), data);
    g_signal_connect(w, "focus-out-event", G_CALLBACK(on_focus_out_event), data);
    g_signal_connect(w, "key-press-event", G_CALLBACK(on_key_event), data);
    g_signal_connect(w, "key-release-event", G_CALLBACK(on_key_event), data);
    return w;
}

#if !GLIB_CHECK_VERSION(2, 34, 0)
static GList *g_list_copy_deep(GList *list, GCopyFunc func, gpointer user_data)
{
    GList *copy = NULL;

    while (list)
    {
        copy = g_list_prepend(copy, func(list->data, user_data));
        list = list->next;
    }
    return g_list_reverse(copy);
}
#endif

/* used by edit_action() to be able to edit */
static GList *copy_options(GList *orig)
{
    GList *copy = NULL;

    /* copy contents recursively */
    while (orig)
    {
        LXHotkeyAttr *attr = g_slice_new(LXHotkeyAttr);
        LXHotkeyAttr *attr_orig = orig->data;

        attr->name = g_strdup(attr_orig->name);
        attr->values = g_list_copy_deep(attr_orig->values, (GCopyFunc)g_strdup, NULL);
        attr->subopts = copy_options(attr_orig->subopts);
        attr->desc = g_strdup(attr_orig->desc);
        attr->has_actions = FALSE;
        attr->has_value = FALSE;
        copy = g_list_prepend(copy, attr);
        orig = orig->next;
    }
    return g_list_reverse(copy);
}

static void add_options_to_tree(GtkTreeStore *store, GtkTreeIter *parent_iter,
                                GList *list)
{
    LXHotkeyAttr *opt;
    GtkTreeIter iter;
    const char *val;

    while (list)
    {
        opt = list->data;
        val = opt->values ? opt->values->data : NULL;
        gtk_tree_store_insert_with_values(store, &iter, parent_iter, -1,
                                          0, opt->name,
                                          1, val,
                                          2, opt,
                                          3, _(opt->name),
                                          4, (val && val[0]) ? _(val) : NULL, -1);
        if (opt->subopts)
            add_options_to_tree(store, &iter, opt->subopts);
        list = list->next;
    }
}

static void update_options_tree(PluginData *data)
{
    GtkTreeStore *store = gtk_tree_store_new(5, G_TYPE_STRING, /* option name */
                                                G_TYPE_STRING, /* option value */
                                                G_TYPE_POINTER, /* LXHotkeyAttr */
                                                G_TYPE_STRING, /* shown name */
                                                G_TYPE_STRING); /* shown value */

    add_options_to_tree(store, NULL, data->edit_options_copy);
    gtk_tree_view_set_model(data->edit_tree, GTK_TREE_MODEL(store));
    gtk_tree_view_expand_all(data->edit_tree);
    g_object_unref(store);
}

#define edit_is_active(data) (data->edit_mode != EDIT_MODE_NONE)

static void cancel_edit(PluginData *data)
{
    data->edit_mode = EDIT_MODE_NONE;
    gtk_widget_hide(data->edit_frame);
}

static void on_exec_changed(GtkEntry *exec, PluginData *data)
{
    const char *value;

    //FIXME: compare with original exec? is that too heavy?
    if (((value = gtk_button_get_label(GTK_BUTTON(data->edit_key1))) == NULL || value[0] == 0) &&
        ((value = gtk_button_get_label(GTK_BUTTON(data->edit_key2))) == NULL || value[0] == 0))
        gtk_action_set_sensitive(data->edit_apply_button, FALSE);
    else
        gtk_action_set_sensitive(data->edit_apply_button, TRUE);
}

static void on_row_activated(GtkTreeView *view, GtkTreePath *path,
                             GtkTreeViewColumn *column, PluginData *data)
{
    GtkTreeModel *model;
    GtkTreeIter iter;

    model = gtk_tree_view_get_model(view);
    if (!gtk_tree_model_get_iter(model, &iter, path))
        /* invalid path */
        return;
    start_edit(model, &iter, data);
}

static void on_selection_changed(GtkTreeSelection *selection, PluginData *data)
{
    if (edit_is_active(data))
        //FIXME: ask confirmation and revert, is that possible?
        cancel_edit(data);
    //update toolbar buttons visibility
    update_edit_toolbar(data);
}

static void on_option_changed(GtkComboBox *box, PluginData *data)
{
    const LXHotkeyAttr *opt, *tmpl;
    const GList *values;
    GtkTreeModel *model;
    GtkListStore *values_store;
    GtkTreeIter iter;
    int i, sel;
    gboolean is_action = FALSE;

    /* g_debug("on_option_changed"); */
    opt = NULL;
    if (data->edit_mode == EDIT_MODE_ADD)
        is_action = (data->current_page == data->acts);
    else if (gtk_tree_selection_get_selected(gtk_tree_view_get_selection(data->edit_tree),
                                             &model, &iter))
    {
        if (data->edit_mode == EDIT_MODE_EDIT)
        {
            gtk_tree_model_get(model, &iter, 2, &opt, -1);
            if (data->current_page == data->acts)
                is_action = (get_parent_template_list(model, &iter, data) == data->edit_template);
        }
        else /* EDIT_MODE_OPTION */
        {
            tmpl = find_template_for_option(model, &iter,
                                            get_parent_template_list(model, &iter, data));
            if (tmpl)
                is_action = tmpl->has_actions;
        }
    }
    if (!gtk_combo_box_get_active_iter(box, &iter))
        /* no item selected */
        return;
    gtk_tree_model_get(gtk_combo_box_get_model(box), &iter, 2, &tmpl, -1);
    if (tmpl->has_actions || is_action ||
        (tmpl->subopts != NULL && !tmpl->has_value))
    {
        /* either it's action, or option has suboptions instead of values */
        gtk_widget_hide(data->edit_value_label);
        gtk_widget_hide(GTK_WIDGET(data->edit_value));
        gtk_widget_hide(GTK_WIDGET(data->edit_values));
        gtk_widget_hide(data->edit_value_num);
        gtk_widget_hide(data->edit_value_num_label);
    }
    else if ((values = tmpl->values) != NULL)
    {
        values_store = GTK_LIST_STORE(gtk_list_store_new(2, G_TYPE_STRING, /* description */
                                                            G_TYPE_STRING)); /* value */
        for (sel = 0, i = 0; values; values = values->next, i++)
        {
            gtk_list_store_insert_with_values(values_store, NULL, i, 0, _(values->data),
                                                                     1, values->data, -1);
            if (opt && opt->values)
            {
                if (((char *)values->data)[0] == '#')
                {
                    size_t len = strspn(opt->values->data, "0123456789");
                    /* test if value is an integer number */
                    if (len == strlen(opt->values->data))
                        sel = i;
                }
                else if (((char *)values->data)[0] == '%')
                {
                    const char *str = opt->values->data;
                    size_t len = strspn(str, "0123456789");
                    /* test if value is either a fraction or a percent value */
                    if (len > 0 && (str[len] == '%' || str[len] == '/'))
                        sel = i;
                }
                else if (g_strcmp0(opt->values->data, values->data) == 0)
                    sel = i;
            }
        }
        gtk_combo_box_set_model(data->edit_values, GTK_TREE_MODEL(values_store));
        g_object_unref(values_store);
        gtk_combo_box_set_active(data->edit_values, sel);
        gtk_widget_show(data->edit_value_label);
        gtk_widget_show(GTK_WIDGET(data->edit_values));
        gtk_widget_hide(GTK_WIDGET(data->edit_value));
    }
    else
    {
        gtk_widget_show(data->edit_value_label);
        gtk_widget_hide(data->edit_value_num);
        gtk_widget_hide(data->edit_value_num_label);
        gtk_widget_hide(GTK_WIDGET(data->edit_values));
        gtk_widget_show(GTK_WIDGET(data->edit_value));
        if (opt && opt->values)
            gtk_entry_set_text(data->edit_value, opt->values->data);
        else
            gtk_entry_set_text(data->edit_value, "");
    }
}

static void on_value_changed(GtkComboBox *box, PluginData *data)
{
    LXHotkeyAttr *opt;
    const char *value;
    GtkTreeModel *model;
    GtkTreeIter iter;
    gdouble num = 0;
    long div;

    model = gtk_combo_box_get_model(box);
    if (!gtk_combo_box_get_active_iter(box, &iter))
        /* no value chosen */
        goto _general;
    gtk_tree_model_get(model, &iter, 1, &value, -1);
    if (!value)
        /* illegal really */
        goto _general;
    if (value[0] == '#')
    {
        /* if value is # then show data->edit_value_num */
        gtk_spin_button_set_range(GTK_SPIN_BUTTON(data->edit_value_num),
                                  -1000.0, +1000.0);
        if (data->edit_mode == EDIT_MODE_EDIT &&
            gtk_tree_selection_get_selected(gtk_tree_view_get_selection(data->edit_tree),
                                            &model, &iter))
        {
            gtk_tree_model_get(model, &iter, 2, &opt, -1);
            if (opt && opt->values)
                num = strtol(opt->values->data, NULL, 10);
        }
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(data->edit_value_num), num);
        gtk_widget_show(data->edit_value_num);
        gtk_widget_hide(data->edit_value_num_label);
    }
    else if (value[0] == '%')
    {
        /* if value is % then show data->edit_value_num with label "%" */
        gtk_spin_button_set_range(GTK_SPIN_BUTTON(data->edit_value_num),
                                  0.0, +100.0);
        if (data->edit_mode == EDIT_MODE_EDIT &&
            gtk_tree_selection_get_selected(gtk_tree_view_get_selection(data->edit_tree),
                                            &model, &iter))
        {
            gtk_tree_model_get(model, &iter, 2, &opt, -1);
            if (opt && opt->values)
            {
                value = opt->values->data;
                num = strtol(value, (char **)&value, 10);
                if (*value == '/')
                {
                    /* convert fraction into percent */
                    div = strtol(value + 1, NULL, 10);
                    div = MAX(div, 1);
                    num *= 100 / div;
                }
            }
        }
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(data->edit_value_num), num);
        gtk_widget_show(data->edit_value_num);
        gtk_label_set_text(GTK_LABEL(data->edit_value_num_label), "%");
        gtk_widget_show(data->edit_value_num_label);
    }
    else
    {
_general:
        /* else hide both data->edit_value_num and label */
        gtk_widget_hide(data->edit_value_num);
        gtk_widget_hide(data->edit_value_num_label);
    }
}

static void apply_options(PluginData *data, LXHotkeyAttr *opt)
{
    GtkTreeModel *model;
    GtkTreeIter iter;
    char *name, *_value = NULL;
    const char *value = NULL;
    gboolean changed = FALSE;

    /* process name */
    if (gtk_widget_get_visible(GTK_WIDGET(data->edit_actions)) &&
        gtk_combo_box_get_active_iter(data->edit_actions, &iter))
    {
        model = gtk_combo_box_get_model(data->edit_actions);
        gtk_tree_model_get(model, &iter, 1, &name, -1);
        if (g_strcmp0(opt->name, name) == 0)
            g_free(name);
        else
        {
            g_free(opt->name);
            opt->name = name;
            changed = TRUE;
        }
    }
    /* process value */
    if (gtk_widget_get_visible(data->edit_value_num))
    {
        gdouble num_val = gtk_spin_button_get_value(GTK_SPIN_BUTTON(data->edit_value_num));
        if (gtk_widget_get_visible(data->edit_value_num_label))
            value = _value = g_strdup_printf("%d%s", (int)num_val,
                                             gtk_label_get_text(GTK_LABEL(data->edit_value_num_label)));
        else
            value = _value = g_strdup_printf("%d", (int)num_val);
    }
    else if (gtk_widget_get_visible(GTK_WIDGET(data->edit_values)) &&
             gtk_combo_box_get_active_iter(data->edit_values, &iter))
    {
        model = gtk_combo_box_get_model(data->edit_values);
        gtk_tree_model_get(model, &iter, 1, &_value, -1);
        value = _value;
    }
    else if (gtk_widget_get_visible(GTK_WIDGET(data->edit_value)))
    {
        value = gtk_entry_get_text(data->edit_value);
    }
    if (opt->values && g_strcmp0(opt->values->data, value) == 0)
        g_free(_value);
    else
    {
        if (_value == NULL)
            value = g_strdup(value);
        if (opt->values == NULL)
            opt->values = g_list_prepend(NULL, (gpointer)value);
        else
        {
            g_free(opt->values->data);
            opt->values->data = (gpointer)value;
        }
        changed = TRUE;
    }
    /* process changed state */
    if (((value = gtk_button_get_label(GTK_BUTTON(data->edit_key1))) == NULL || value[0] == 0) &&
        ((value = gtk_button_get_label(GTK_BUTTON(data->edit_key2))) == NULL || value[0] == 0))
        gtk_action_set_sensitive(data->edit_apply_button, FALSE);
    else if (changed)
        gtk_action_set_sensitive(data->edit_apply_button, TRUE);
}

static void on_apply_button(GtkButton *btn, PluginData *data)
{
    LXHotkeyAttr *opt;
    GtkTreeModel *model;
    GtkTreeIter iter;
    const char *val;

    switch (data->edit_mode)
    {
    case EDIT_MODE_ADD:
        opt = g_slice_new0(LXHotkeyAttr);
        apply_options(data, opt);
        /* insert new option/action */
        data->edit_options_copy = g_list_append(data->edit_options_copy, opt);
        model = gtk_tree_view_get_model(data->edit_tree);
        /* update the tree */
        val = opt->values ? opt->values->data : NULL;
        gtk_tree_store_insert_with_values(GTK_TREE_STORE(model), NULL, NULL, -1,
                                          0, opt->name,
                                          1, val,
                                          2, opt,
                                          3, _(opt->name),
                                          4, (val && val[0]) ? _(val) : NULL, -1);
        /* update toolbar */
        update_edit_toolbar(data);
        break;
    case EDIT_MODE_EDIT:
        if (gtk_tree_selection_get_selected(gtk_tree_view_get_selection(data->edit_tree),
                                            &model, &iter))
        {
            gtk_tree_model_get(model, &iter, 2, &opt, -1);
            apply_options(data, opt);
            val = opt->values ? opt->values->data : NULL;
            gtk_tree_store_set(GTK_TREE_STORE(model), &iter,
                               1, val,
                               4, (val && val[0]) ? _(val) : NULL, -1);
            update_edit_toolbar(data);
        }
        break;
    case EDIT_MODE_OPTION:
        if (gtk_tree_selection_get_selected(gtk_tree_view_get_selection(data->edit_tree),
                                            &model, &iter))
        {
            LXHotkeyAttr *parent;
            gtk_tree_model_get(model, &iter, 2, &parent, -1);
            opt = g_slice_new0(LXHotkeyAttr);
            apply_options(data, opt);
            parent->subopts = g_list_append(parent->subopts, opt);
            model = gtk_tree_view_get_model(data->edit_tree);
            val = opt->values ? opt->values->data : NULL;
            gtk_tree_store_insert_with_values(GTK_TREE_STORE(model), NULL, &iter, -1,
                                              0, opt->name,
                                              1, val,
                                              2, opt,
                                              3, _(opt->name),
                                              4, (val && val[0]) ? _(val) : NULL, -1);
            gtk_tree_view_expand_all(data->edit_tree);
            update_edit_toolbar(data);
        }
        break;
    case EDIT_MODE_NONE:
    default:
        break;
    }
    cancel_edit(data);
}

static void on_cancel_button(GtkButton *btn, PluginData *data)
{
    cancel_edit(data);
}

/* free all allocated data */
void _edit_cleanup(PluginData *data)
{
    if (data->edit_window)
    {
        cancel_edit(data);
        g_object_remove_weak_pointer(G_OBJECT(data->edit_window), (gpointer)&data->edit_window);
        gtk_widget_destroy(GTK_WIDGET(data->edit_window));
        data->edit_window = NULL;
    }
    if (data->edit_options_copy)
    {
        free_options(data->edit_options_copy);
        data->edit_options_copy = NULL;
    }
}

void _edit_action(PluginData *data, GError **error)
{
    LXHotkeyGlobal *act = NULL;
    LXHotkeyApp *app = NULL;
    const char *accel1 = NULL, *accel2 = NULL;
    GtkBox *vbox, *xbox;
    GtkUIManager *ui;
    GtkActionGroup *act_grp;
    GtkAccelGroup *accel_grp;
    GtkWidget *widget, *align;
    GtkToolbar *toolbar;
    GtkCellRenderer *column;
    GtkTreeModel *model;
    GtkTreeIter iter;
    gboolean is_action = FALSE;

    if (data->edit_window)
    {
        /* OOPS, another edit is still opened */
        return;
    }
    /* do cleanup */
    _edit_cleanup(data);
    /* get a template */
    if (data->current_page == data->acts)
    {
        if (data->cb->get_wm_actions == NULL) /* not available for edit */
            return;
        if (data->cb->set_wm_key == NULL) /* not available for save */
            return;
        data->edit_template = data->cb->get_wm_actions(*data->config, NULL);
        //FIXME: test for error
        is_action = TRUE;
        if (gtk_tree_selection_get_selected(gtk_tree_view_get_selection(data->acts),
                                            &model, &iter))
            gtk_tree_model_get(model, &iter, 4, &act, -1);
        if (act)
        {
            /* if there is a selection then copy its options */
            data->edit_options_copy = copy_options(act->actions);
            accel1 = act->accel1;
            accel2 = act->accel2;
        }
    }
    else
    {
        if (data->cb->get_app_options == NULL) /* not available for edit */
            return;
        if (data->cb->set_app_key == NULL) /* not available for save */
            return;
        data->edit_template = data->cb->get_app_options(*data->config, NULL);
        //FIXME: test for error
        if (gtk_tree_selection_get_selected(gtk_tree_view_get_selection(data->apps),
                                            &model, &iter))
            gtk_tree_model_get(model, &iter, 3, &app, -1);
        if (app)
        {
            /* if there is a selection then copy its options */
            data->edit_options_copy = copy_options(app->options);
            accel1 = app->accel1;
            accel2 = app->accel2;
        }
    }

    /* create a window with a GtkVBox inside */
    data->edit_window = GTK_WINDOW(gtk_window_new(GTK_WINDOW_TOPLEVEL));
    gtk_window_set_default_size(data->edit_window, 240, 10);
    gtk_window_set_transient_for(data->edit_window,
                                 GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(data->notebook))));
    g_object_add_weak_pointer(G_OBJECT(data->edit_window), (gpointer)&data->edit_window);
    vbox = (GtkBox *)gtk_vbox_new(FALSE, 0);

    /* add the toolbar */
    ui = gtk_ui_manager_new();
    act_grp = gtk_action_group_new("Edit");
    gtk_action_group_set_translation_domain(act_grp, NULL);
    gtk_action_group_add_actions(act_grp, actions, G_N_ELEMENTS(actions), data);
    accel_grp = gtk_ui_manager_get_accel_group(ui);
    gtk_window_add_accel_group(GTK_WINDOW(data->edit_window), accel_grp);
    gtk_ui_manager_insert_action_group(ui, act_grp, 0);
    gtk_ui_manager_add_ui_from_string(ui, edit_xml, -1, NULL);
    g_object_unref(act_grp);
    widget = gtk_ui_manager_get_widget(ui, "/toolbar");
    toolbar = GTK_TOOLBAR(widget);
    //TODO: 'Change' -- also 2click and Enter
    gtk_toolbar_set_icon_size(toolbar, GTK_ICON_SIZE_SMALL_TOOLBAR);
    gtk_toolbar_set_style(toolbar, GTK_TOOLBAR_ICONS);
    gtk_toolbar_set_show_arrow(toolbar, FALSE);
    data->edit_apply_button = gtk_ui_manager_get_action(ui, "/toolbar/Save");
    data->add_option_button = gtk_ui_manager_get_action(ui, "/toolbar/AddOption");
    data->rm_option_button = gtk_ui_manager_get_action(ui, "/toolbar/Remove");
    data->edit_option_button = gtk_ui_manager_get_action(ui, "/toolbar/Change");
    data->add_suboption_button = gtk_ui_manager_get_action(ui, "/toolbar/AddSubOption");
    gtk_action_set_sensitive(data->edit_apply_button, FALSE);
    gtk_box_pack_start(vbox, widget, FALSE, TRUE, 0);

    /* add frames for accel1 and accel2 */
    xbox = (GtkBox *)gtk_hbox_new(TRUE, 0);
    widget = gtk_frame_new(_("Hotkey 1"));
    data->edit_key1 = key_button_new(data, accel1);
    gtk_container_add(GTK_CONTAINER(widget), data->edit_key1);
    gtk_box_pack_start(xbox, widget, TRUE, TRUE, 0);
    widget = gtk_frame_new(_("Hotkey 2"));
    data->edit_key2 = key_button_new(data, accel2);
    gtk_container_add(GTK_CONTAINER(widget), data->edit_key2);
    gtk_box_pack_start(xbox, widget, TRUE, TRUE, 0);
    gtk_box_pack_start(vbox, GTK_WIDGET(xbox), FALSE, TRUE, 0);

    /* add frame with all options */
    widget = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type(GTK_FRAME(widget), GTK_SHADOW_IN);
    xbox = (GtkBox *)gtk_vbox_new(FALSE, 0);
    if (is_action)
    {
        align = gtk_alignment_new(0.0, 0.0, 0.0, 0.0);
        gtk_container_add(GTK_CONTAINER(align), gtk_label_new(_("Actions:")));
        gtk_box_pack_start(xbox, align, FALSE, TRUE, 0);
        data->edit_exec = NULL;
    }
    else
    {
        /* for application add a GtkEntry for exec line */
        align = gtk_alignment_new(0.0, 0.0, 0.0, 0.0);
        gtk_container_add(GTK_CONTAINER(align), gtk_label_new(_("Command line:")));
        gtk_box_pack_start(xbox, align, FALSE, TRUE, 0);
        data->edit_exec = GTK_ENTRY(gtk_entry_new());
        g_signal_connect(data->edit_exec, "changed", G_CALLBACK(on_exec_changed), data);
        if (app && app->exec)
            gtk_entry_set_text(data->edit_exec, app->exec);
        align = gtk_alignment_new(0.0, 0.0, 1.0, 1.0);
        gtk_alignment_set_padding(GTK_ALIGNMENT(align), 0, 0, 4, 0);
        gtk_container_add(GTK_CONTAINER(align), GTK_WIDGET(data->edit_exec));
        gtk_box_pack_start(xbox, align, FALSE, TRUE, 0);
        align = gtk_alignment_new(0.0, 0.0, 0.0, 0.0);
        gtk_container_add(GTK_CONTAINER(align), gtk_label_new(_("Options:")));
        gtk_box_pack_start(xbox, align, FALSE, TRUE, 0);
    }
    data->edit_tree = GTK_TREE_VIEW(gtk_tree_view_new());
    gtk_box_pack_start(xbox, GTK_WIDGET(data->edit_tree), TRUE, TRUE, 0);
    gtk_container_add(GTK_CONTAINER(widget), GTK_WIDGET(xbox));
    gtk_box_pack_start(vbox, widget, FALSE, TRUE, 0);
    gtk_tree_view_insert_column_with_attributes(data->edit_tree, 0, NULL,
                                                gtk_cell_renderer_text_new(),
                                                "text", 3, NULL);
    gtk_tree_view_insert_column_with_attributes(data->edit_tree, 1, NULL,
                                                gtk_cell_renderer_text_new(),
                                                "text", 4, NULL);
    gtk_tree_view_set_headers_visible(data->edit_tree, FALSE);
    g_signal_connect(data->edit_tree, "row-activated", G_CALLBACK(on_row_activated), data);

    /* frame with fields for editing, hidden for now */
    data->edit_frame = gtk_frame_new(NULL);
    xbox = (GtkBox *)gtk_vbox_new(FALSE, 0);
    /* combobox for option/action name */
    align = gtk_alignment_new(0.0, 0.0, 0.0, 0.0);
    widget = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(widget), _("<b>Name:</b>"));
    gtk_container_add(GTK_CONTAINER(align), widget);
    gtk_box_pack_start(xbox, align, FALSE, TRUE, 0);
    align = gtk_alignment_new(0.0, 0.0, 1.0, 1.0);
    gtk_alignment_set_padding(GTK_ALIGNMENT(align), 0, 0, 4, 0);
    widget = gtk_hbox_new(FALSE, 0);
    data->edit_option_name = gtk_label_new(NULL);
    gtk_box_pack_start(GTK_BOX(widget), data->edit_option_name, FALSE, TRUE, 0);
    data->edit_actions = (GtkComboBox *)gtk_combo_box_new();
    g_signal_connect(data->edit_actions, "changed",
                     G_CALLBACK(on_option_changed), data);
    column = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(data->edit_actions), column, TRUE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(data->edit_actions), column,
                                   "text", 0, NULL);
    gtk_box_pack_start(GTK_BOX(widget), GTK_WIDGET(data->edit_actions), TRUE, TRUE, 0);
    gtk_container_add(GTK_CONTAINER(align), widget);
    gtk_box_pack_start(xbox, align, FALSE, TRUE, 0);
    /* entry or combobox for option value */
    data->edit_value_label = gtk_alignment_new(0.0, 0.0, 0.0, 0.0);
    widget = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(widget), _("<b>Value:</b>"));
    gtk_container_add(GTK_CONTAINER(data->edit_value_label), widget);
    gtk_box_pack_start(xbox, data->edit_value_label, FALSE, TRUE, 0);
    align = gtk_alignment_new(0.0, 0.0, 1.0, 1.0);
    gtk_alignment_set_padding(GTK_ALIGNMENT(align), 0, 0, 4, 0);
    widget = gtk_hbox_new(FALSE, 0);
    data->edit_value = (GtkEntry *)gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(widget), GTK_WIDGET(data->edit_value), TRUE, TRUE, 0);
    data->edit_values = (GtkComboBox *)gtk_combo_box_new();
    column = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(data->edit_values), column, TRUE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(data->edit_values), column,
                                   "text", 0, NULL);
    gtk_box_pack_start(GTK_BOX(widget), GTK_WIDGET(data->edit_values), TRUE, TRUE, 0);
    g_signal_connect(data->edit_values, "changed",
                     G_CALLBACK(on_value_changed), data);
    data->edit_value_num = gtk_spin_button_new_with_range(-1000.0, +1000.0, 1.0);
    gtk_box_pack_start(GTK_BOX(widget), GTK_WIDGET(data->edit_value_num), FALSE, TRUE, 0);
    data->edit_value_num_label = gtk_label_new(NULL);
    gtk_box_pack_start(GTK_BOX(widget), GTK_WIDGET(data->edit_value_num_label), FALSE, TRUE, 0);
    gtk_container_add(GTK_CONTAINER(align), widget);
    gtk_box_pack_start(xbox, align, FALSE, TRUE, 0);
    /* buttons 'Cancel' and 'Apply' */
    align = gtk_alignment_new(1.0, 0.0, 0.0, 0.0);
    gtk_box_pack_start(xbox, align, FALSE, TRUE, 0);
    widget = gtk_hbox_new(TRUE, 4);
    gtk_container_add(GTK_CONTAINER(align), widget);
    align = gtk_button_new_from_stock(GTK_STOCK_APPLY); /* reuse align */
    g_signal_connect(align, "clicked", G_CALLBACK(on_apply_button), data);
    gtk_box_pack_end(GTK_BOX(widget), align, FALSE, TRUE, 0);
    align = gtk_button_new_from_stock(GTK_STOCK_CANCEL); /* reuse align */
    g_signal_connect(align, "clicked", G_CALLBACK(on_cancel_button), data);
    gtk_box_pack_end(GTK_BOX(widget), align, FALSE, TRUE, 0);
    gtk_container_add(GTK_CONTAINER(data->edit_frame), GTK_WIDGET(xbox));
    gtk_box_pack_start(vbox, data->edit_frame, TRUE, TRUE, 0);

    gtk_widget_show_all(GTK_WIDGET(vbox));
    gtk_widget_hide(data->edit_frame);
    /* hide one of AddAction or AddOption */
    if (is_action)
    {
        /* AddOption is visible for exec only */
        gtk_action_set_visible(data->add_option_button, FALSE);
    }
    else
    {
        /* AddAction is visible for action only */
        GtkAction *act = gtk_ui_manager_get_action(ui, "/toolbar/AddAction");
        gtk_action_set_visible(act, FALSE);
    }
    gtk_container_add(GTK_CONTAINER(data->edit_window), GTK_WIDGET(vbox));
    g_signal_connect(gtk_tree_view_get_selection(data->edit_tree), "changed",
                     G_CALLBACK(on_selection_changed), data);
    update_options_tree(data);
    update_edit_toolbar(data);
    gtk_window_present(data->edit_window);
    gtk_widget_grab_focus(GTK_WIDGET(data->edit_tree));
}
