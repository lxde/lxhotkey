/*
 * Copyright (C) 2016 Andriy Grytsenko <andrej@rep.kiev.ua>
 * Copyright (C) 2020 durapensa <durapensa@gmail.com>
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

#include <X11/Xlib.h>
#include <X11/Xatom.h>

#include <libfm/fm-extra.h>
#include <glib/gi18n.h>

#include <fnmatch.h>

#define LXKEYS_OB_ERROR lxhotkey_ob_error_quark()
static GQuark lxhotkey_ob_error_quark(void)
{
    static GQuark q = 0;

    if G_UNLIKELY(q == 0)
        q = g_quark_from_static_string("lxhotkey-ob-error");

    return q;
}
enum LXHotkeyObError {
    LXKEYS_FILE_ERROR,
    LXKEYS_PARSE_ERROR
};


/* simple functions to manage LXHotkeyAttr data type */
static inline LXHotkeyAttr *lxhotkey_attr_new(void)
{
    return g_slice_new0(LXHotkeyAttr);
}

#define free_options(acts) g_list_free_full(acts, (GDestroyNotify)lkxeys_attr_free)

static void lkxeys_attr_free(LXHotkeyAttr *data)
{
    g_free(data->name);
    g_list_free_full(data->values, g_free);
    free_options(data->subopts);
    g_slice_free(LXHotkeyAttr, data);
}

static void lkxeys_action_free(LXHotkeyGlobal *data)
{
    free_options(data->actions);
    g_free(data->accel1);
    g_free(data->accel2);
    g_free(data);
}

static void lkxeys_app_free(LXHotkeyApp *data)
{
    g_free(data->exec);
    free_options(data->options);
    g_free(data->accel1);
    g_free(data->accel2);
    g_free(data);
}

/* convert from OB format (A-Return) into GDK format (<Alt>Return) */
static gchar *obkey_to_key(const gchar *obkey)
{
    GString *str = g_string_sized_new(16);

    while (*obkey) {
        if (obkey[1] == '-')
            switch(obkey[0]) {
            case 'S':
                g_string_append(str, "<Shift>");
                break;
            case 'C':
                g_string_append(str, "<Control>");
                break;
            case 'A':
                g_string_append(str, "<Alt>");
                break;
            case 'W':
                g_string_append(str, "<Super>");
                break;
            case 'M':
                g_string_append(str, "<Meta>");
                break;
            case 'H':
                g_string_append(str, "<Hyper>");
                break;
            default:
                goto _add_rest;
            }
        else
_add_rest:
            break;
        obkey += 2;
    }
    g_string_append(str, obkey);
    return g_string_free(str, FALSE);
}

/* convert from GDK format (<Alt>Return) into OB format (A-Return) */
static gchar *key_to_obkey(const gchar *key)
{
    GString *str = g_string_sized_new(16);
    gboolean in_lt = FALSE;

    while (*key) {
        if (in_lt) {
            if (*key++ == '>')
                in_lt = FALSE;
        } else if (*key == '<') {
            key++;
            in_lt = TRUE;
            if (strncmp(key, "Shift", 5) == 0) {
                g_string_append(str, "S-");
                key += 5;
            } else if (strncmp(key, "Contr", 5) == 0 ||
                       strncmp(key, "Ctr", 3) == 0) {
                g_string_append(str, "C-");
                key += 3;
            } else if (strncmp(key, "Alt", 3) == 0) {
                g_string_append(str, "A-");
                key += 3;
            } else if (strncmp(key, "Super", 5) == 0) {
                g_string_append(str, "W-");
                key += 5;
            } else if (strncmp(key, "Meta", 4) == 0) {
                g_string_append(str, "M-");
                key += 4;
            } else if (strncmp(key, "Hyper", 5) == 0) {
                g_string_append(str, "H-");
                key += 5;
            }
        } else
            g_string_append_c(str, *key++);
    }
    return g_string_free(str, FALSE);
}


static gboolean restart_openbox(GError **error)
{
    Display *dpy = XOpenDisplay(NULL);
    XEvent ce;
    gboolean ret = TRUE;

    ce.xclient.type = ClientMessage;
    ce.xclient.message_type = XInternAtom(dpy, "_OB_CONTROL", True);
    ce.xclient.display = dpy;
    ce.xclient.window = RootWindow(dpy, DefaultScreen(dpy));
    ce.xclient.format = 32;
    ce.xclient.data.l[0] = 1; /* reconfigure */
    ce.xclient.data.l[1] = 0;
    ce.xclient.data.l[2] = 0;
    ce.xclient.data.l[3] = 0;
    ce.xclient.data.l[4] = 0;
    if (ce.xclient.message_type == None ||
        XSendEvent(dpy, ce.xclient.window, False,
                   SubstructureNotifyMask | SubstructureRedirectMask, &ce) == 0) {
        g_set_error_literal(error, LXKEYS_OB_ERROR, LXKEYS_FILE_ERROR,
                            _("Failed to reconfigure Openbox."));
        ret = FALSE;
    }
    XCloseDisplay(dpy);
    return ret;
}


/*
 * Actions/options list supported by Openbox.
 *
 * This array is a bit tricky since it does not contain GList pointers, but
 * those pointers will be expanded on demand.
 */

#define TO_BE_CONVERTED(a) (GList *)(a)
#define TO_BE_PREVIOUS TO_BE_CONVERTED(1) /* reuse GList */
#define BOOLEAN_VALUES TO_BE_CONVERTED(2) /* reuse GList */

static char * values_enabled[] = { N_("yes"), N_("no"), NULL };

static LXHotkeyAttr options_startupnotify[] = {
    { N_("enabled"), BOOLEAN_VALUES, NULL, NULL, FALSE },
    { N_("wmclass"), NULL, NULL, NULL, FALSE },
    { N_("name"), NULL, NULL, NULL, FALSE },
    { N_("icon"), NULL, NULL, NULL, FALSE },
    { NULL }
};

static LXHotkeyAttr options_Execute[] = {
    { N_("command"), NULL, NULL, NULL, FALSE },
    { N_("prompt"), NULL, NULL, NULL, FALSE },
    { N_("startupnotify"), NULL, TO_BE_CONVERTED(options_startupnotify), NULL, FALSE },
    { NULL }
};

static char * values_x[] = { "#", "%", N_("center"), NULL };
static char * values_monitor[] = { N_("default"), N_("primary"), N_("mouse"),
                                   N_("active"), N_("all"), "#", NULL };

static LXHotkeyAttr options_position[] = {
    { "x", TO_BE_CONVERTED(values_x), NULL, NULL, FALSE },
    { "y", TO_BE_PREVIOUS, NULL, NULL, FALSE },
    { N_("monitor"), TO_BE_CONVERTED(values_monitor), NULL, NULL, FALSE },
    { NULL }
};

static LXHotkeyAttr options_ShowMenu[] = {
    { N_("menu"), NULL, NULL, NULL, FALSE },
    { N_("position"), NULL, TO_BE_CONVERTED(options_position), NULL, FALSE },
    { NULL }
};

static char * values_dialog[] = { N_("list"), N_("icons"), N_("none"), NULL };

static LXHotkeyAttr options_NextWindow[] = {
    { N_("dialog"), TO_BE_CONVERTED(values_dialog), NULL, NULL, FALSE },
    { N_("bar"), BOOLEAN_VALUES, NULL, NULL, FALSE },
    { N_("raise"), BOOLEAN_VALUES, NULL, NULL, FALSE },
    { N_("allDesktops"), BOOLEAN_VALUES, NULL, NULL, FALSE },
    { N_("panels"), BOOLEAN_VALUES, NULL, NULL, FALSE },
    { N_("desktop"), BOOLEAN_VALUES, NULL, NULL, FALSE },
    { N_("linear"), BOOLEAN_VALUES, NULL, NULL, FALSE },
    { N_("interactive"), BOOLEAN_VALUES, NULL, NULL, FALSE },
    /* TRANSLATORS: finalactions means actions when done */
    { N_("finalactions"), NULL, NULL, NULL, TRUE },
    { NULL }
};

static char * values_direction[] = { N_("north"), N_("northeast"), N_("east"),
                                     N_("southeast"), N_("south"), N_("southwest"),
                                     N_("west"), N_("northwest"), NULL };

static LXHotkeyAttr options_DirectionalCycleWindows[] = {
    { N_("direction"), TO_BE_CONVERTED(values_direction), NULL, NULL, FALSE },
    { N_("dialog"), BOOLEAN_VALUES, NULL, NULL, FALSE },
    { N_("bar"), BOOLEAN_VALUES, NULL, NULL, FALSE },
    { N_("raise"), BOOLEAN_VALUES, NULL, NULL, FALSE },
    { N_("panels"), BOOLEAN_VALUES, NULL, NULL, FALSE },
    { N_("desktops"), BOOLEAN_VALUES, NULL, NULL, FALSE },
    { N_("finalactions"), NULL, NULL, NULL, TRUE },
    { NULL }
};

/* TRANSLATORS: these values are in regard to desktop */
static char * values_to[] = { "#",  N_("current"), N_("next"), N_("previous"),
                              N_("last"), N_("north"), N_("up"), N_("south"),
                              N_("down"), N_("west"), N_("left"), N_("east"),
                              N_("right"), NULL };

static LXHotkeyAttr options_GoToDesktop[] = {
    { N_("to"), TO_BE_CONVERTED(values_to), NULL, NULL, FALSE },
    { N_("wrap"), BOOLEAN_VALUES, NULL, NULL, FALSE },
    { NULL }
};

static char * values_where[] = { N_("current"), N_("last"), NULL };

static LXHotkeyAttr options_AddDesktop[] = {
    { N_("where"), TO_BE_CONVERTED(values_where), NULL, NULL, FALSE },
    { NULL }
};

static LXHotkeyAttr options_ToggleShowDesktop[] = {
    { N_("strict"), BOOLEAN_VALUES, NULL, NULL, FALSE },
    { NULL }
};

static LXHotkeyAttr options_Restart[] = {
    { N_("command"), NULL, NULL, NULL, FALSE },
    { NULL }
};

static LXHotkeyAttr options_Exit[] = {
    { N_("prompt"), BOOLEAN_VALUES, NULL, NULL, FALSE },
    { NULL }
};

static char * values_directionM[] = { N_("both"), N_("horizontal"), N_("vertical"), NULL };

static LXHotkeyAttr options_ToggleMaximize[] = {
    { N_("direction"), TO_BE_CONVERTED(values_directionM), NULL, NULL, FALSE },
    { NULL }
};

static char * values_toS[] = { "#", N_("current"), N_("next"), N_("previous"),
                               N_("last"), N_("north"), N_("up"), N_("south"),
                               N_("down"), N_("west"), N_("left"), N_("east"),
                               N_("right"), NULL };

static LXHotkeyAttr options_SendToDesktop[] = {
    { N_("to"), TO_BE_CONVERTED(values_toS), NULL, NULL, FALSE },
    { N_("wrap"), BOOLEAN_VALUES, NULL, NULL, FALSE },
    { N_("follow"), BOOLEAN_VALUES, NULL, NULL, FALSE },
    { NULL }
};

static char * values_edge[] = { N_("top"), N_("left"), N_("right"), N_("bottom"),
                                N_("topleft"), N_("topright"), N_("bottomleft"),
                                N_("bottomright"), NULL };

static LXHotkeyAttr options_Resize[] = {
    { N_("edge"), TO_BE_CONVERTED(values_edge), NULL, NULL, FALSE },
    { NULL }
};

static char * values_xM[] = { N_("#"), N_("current"), N_("center"), NULL };
/* TRANSLATORS: % in config means some fraction value, usually measured in percents */
static char * values_width[] = { "#", N_("%"), "current", NULL };
static char * values_monitorM[] = { "#", N_("current"), N_("all"), N_("next"), N_("prev"), NULL };

static LXHotkeyAttr options_MoveResizeTo[] = {
    { "x", TO_BE_CONVERTED(values_xM), NULL, NULL, FALSE },
    { "y", TO_BE_PREVIOUS, NULL, NULL, FALSE },
    { N_("width"), TO_BE_CONVERTED(values_width), NULL, NULL, FALSE },
    { N_("height"), TO_BE_PREVIOUS, NULL, NULL, FALSE },
    { N_("monitor"), TO_BE_CONVERTED(values_monitorM), NULL, NULL, FALSE },
    { NULL }
};

/* TRANSLATORS: # in config means either pixels, or monitor number */
static char * values_xR[] = { N_("#"), NULL };

static LXHotkeyAttr options_MoveRelative[] = {
    { "x", TO_BE_CONVERTED(values_xR), NULL, NULL, FALSE },
    { "y", TO_BE_PREVIOUS, NULL, NULL, FALSE },
    { NULL }
};

static char * values_xRR[] = { "#", NULL };

static LXHotkeyAttr options_ResizeRelative[] = {
    { N_("left"), TO_BE_CONVERTED(values_xRR), NULL, NULL, FALSE },
    { N_("right"), TO_BE_PREVIOUS, NULL, NULL, FALSE },
    { N_("top"), TO_BE_PREVIOUS, NULL, NULL, FALSE },
    { N_("bottom"), TO_BE_PREVIOUS, NULL, NULL, FALSE },
    { NULL }
};

static char * values_directionE[] = { N_("north"), N_("south"), N_("west"), N_("east"), NULL };

static LXHotkeyAttr options_MoveToEdge[] = {
    { N_("direction"), TO_BE_CONVERTED(values_directionE), NULL, NULL, FALSE },
    { NULL }
};

static char * values_layer[] = { N_("top"), N_("normal"), N_("bottom"), NULL };

static LXHotkeyAttr options_SendToLayer[] = {
    { N_("layer"), TO_BE_CONVERTED(values_layer), NULL, NULL, FALSE },
    { NULL }
};

static LXHotkeyAttr list_actions[] = {
    /* global actions */
    { N_("Execute"), NULL, TO_BE_CONVERTED(options_Execute), NULL, FALSE },
    { N_("ShowMenu"), NULL, TO_BE_CONVERTED(options_ShowMenu), NULL, FALSE },
    { N_("NextWindow"), NULL, TO_BE_CONVERTED(options_NextWindow), NULL, FALSE },
    { N_("PreviousWindow"), NULL, TO_BE_PREVIOUS, NULL, FALSE },
    { N_("DirectionalCycleWindows"), NULL, TO_BE_CONVERTED(options_DirectionalCycleWindows), NULL, FALSE },
    { N_("DirectionalTargetWindow"), NULL, TO_BE_PREVIOUS, NULL, FALSE },
    { N_("GoToDesktop"), NULL, TO_BE_CONVERTED(options_GoToDesktop), NULL, FALSE },
    { N_("AddDesktop"), NULL, TO_BE_CONVERTED(options_AddDesktop), NULL, FALSE },
    { N_("RemoveDesktop"), NULL, TO_BE_PREVIOUS, NULL, FALSE },
    { N_("ToggleShowDesktop"), NULL, TO_BE_CONVERTED(options_ToggleShowDesktop), NULL, FALSE },
    { N_("ToggleDockAutohide"), NULL, NULL, NULL, FALSE },
    { N_("Reconfigure"), NULL, NULL, NULL, FALSE },
    { N_("Restart"), NULL, TO_BE_CONVERTED(options_Restart), NULL, FALSE },
    { N_("Exit"), NULL, TO_BE_CONVERTED(options_Exit), NULL, FALSE },
    /* windows actions */
    { N_("Focus"), NULL, NULL, NULL, FALSE },
    { N_("Raise"), NULL, NULL, NULL, FALSE },
    { N_("Lower"), NULL, NULL, NULL, FALSE },
    { N_("RaiseLower"), NULL, NULL, NULL, FALSE },
    { N_("Unfocus"), NULL, NULL, NULL, FALSE },
    { N_("FocusToBottom"), NULL, NULL, NULL, FALSE },
    { N_("Iconify"), NULL, NULL, NULL, FALSE },
    { N_("Close"), NULL, NULL, NULL, FALSE },
    { N_("ToggleShade"), NULL, NULL, NULL, FALSE },
    { N_("Shade"), NULL, NULL, NULL, FALSE },
    { N_("Unshade"), NULL, NULL, NULL, FALSE },
    { N_("ToggleOmnipresent"), NULL, NULL, NULL, FALSE },
    { N_("ToggleMaximize"), NULL, TO_BE_CONVERTED(options_ToggleMaximize), NULL, FALSE },
    { N_("Maximize"), NULL, TO_BE_PREVIOUS, NULL, FALSE },
    { N_("Unmaximize"), NULL, TO_BE_PREVIOUS, NULL, FALSE },
    { N_("ToggleFullscreen"), NULL, NULL, NULL, FALSE },
    { N_("ToggleDecorations"), NULL, NULL, NULL, FALSE },
    { N_("Decorate"), NULL, NULL, NULL, FALSE },
    { N_("Undecorate"), NULL, NULL, NULL, FALSE },
    { N_("SendToDesktop"), NULL, TO_BE_CONVERTED(options_SendToDesktop), NULL, FALSE },
    { N_("Move"), NULL, NULL, NULL, FALSE },
    { N_("Resize"), NULL, TO_BE_CONVERTED(options_Resize), NULL, FALSE },
    { N_("MoveResizeTo"), NULL, TO_BE_CONVERTED(options_MoveResizeTo), NULL, FALSE },
    { N_("MoveRelative"), NULL, TO_BE_CONVERTED(options_MoveRelative), NULL, FALSE },
    { N_("ResizeRelative"), NULL, TO_BE_CONVERTED(options_ResizeRelative), NULL, FALSE },
    { N_("MoveToEdge"), NULL, TO_BE_CONVERTED(options_MoveToEdge), NULL, FALSE },
    { N_("GrowToEdge"), NULL, TO_BE_PREVIOUS, NULL, FALSE },
    { N_("ShrinkToEdge"), NULL, TO_BE_PREVIOUS, NULL, FALSE },
    { N_("GrowToFill"), NULL, NULL, NULL, FALSE },
    { N_("ToggleAlwaysOnTop"), NULL, NULL, NULL, FALSE },
    { N_("ToggleAlwaysOnBottom"), NULL, NULL, NULL, FALSE },
    { N_("SendToLayer"), NULL, TO_BE_CONVERTED(options_SendToLayer), NULL, FALSE },
    // FIXME: support for If/ForEach/Stop ?
    { NULL }
};

static GList *boolean_values = NULL;
static GList *available_wm_actions = NULL;
static GList *available_app_options = NULL;

static GList *convert_values(gpointer data)
{
    char ** array;
    GList *list = NULL;

    for (array = data; array[0] != NULL; array++) {
        /* g_debug("creating GList for string '%s'", array[0]); */
        list = g_list_prepend(list, array[0]);
    }
    return g_list_reverse(list);
}

static GList *convert_options(gpointer data)
{
    LXHotkeyAttr *array, *last = NULL;
    GList *list = NULL;

    for (array = data; array->name != NULL; array++) {
        list = g_list_prepend(list, array);
        if (last && array->values == TO_BE_PREVIOUS)
            array->values = last->values;
        else if (array->values == BOOLEAN_VALUES) {
            if (boolean_values == NULL)
                boolean_values = convert_values(values_enabled);
            array->values = boolean_values;
        } else if (array->values != NULL)
            array->values = convert_values(array->values);
        if (last && array->subopts == TO_BE_PREVIOUS)
            array->subopts = last->subopts;
        else if (array->subopts != NULL) {
            if (array->subopts == TO_BE_CONVERTED(options_Execute))
                array->subopts = available_app_options = convert_options(array->subopts);
            else
                array->subopts = convert_options(array->subopts);
        }
        last = array;
    }
    return g_list_reverse(list);
}


typedef struct {
    FmXmlFileItem *parent; /* R/O */
    GList *list; /* contains LXHotkeyAttr items for finished actions */
} ObActionsList;

typedef struct {
    char *path;
    FmXmlFile *xml;
    FmXmlFileItem *keyboard; /* the <keyboard> section */
    GList *actions; /* no-exec actions, in reverse order */
    GList *execs; /* exec-only actions, in reverse order */
    GList *stack; /* only for build - elements are ObActionsList */
    GList *added_tags; /* only for edit */
} ObXmlFile;

static FmXmlFileTag ObXmlFile_keyboard; /* section that we work on */
static FmXmlFileTag ObXmlFile_keybind; /* subsection, for each binding */
static FmXmlFileTag ObXmlFile_action; /* may be multiple for a binding */
static FmXmlFileTag ObXmlFile_command; /* for <action name="Execute"> */
static FmXmlFileTag ObXmlFile_execute; /* obsolete alias for command */

static inline void clear_stack(ObXmlFile *cfg)
{
    while (cfg->stack != NULL) {
        free_options(((ObActionsList *)cfg->stack->data)->list);
        g_free(cfg->stack->data);
        cfg->stack = g_list_delete_link(cfg->stack, cfg->stack);
    }
}

static gboolean tag_handler_keyboard(FmXmlFileItem *item, GList *children,
                                     char * const *attribute_names,
                                     char * const *attribute_values,
                                     guint n_attributes, gint line, gint pos,
                                     GError **error, gpointer user_data)
{
    ObXmlFile *cfg = user_data;

    if (cfg->keyboard) {
        /* FIXME: merge duplicate section? */
        g_set_error_literal(error, LXKEYS_OB_ERROR, LXKEYS_PARSE_ERROR,
                            _("Duplicate <keyboard> section in the rc.xml file."));
        return FALSE;
    }
    cfg->keyboard = item;
    return TRUE;
}

static gboolean tag_handler_keybind(FmXmlFileItem *item, GList *children,
                                    char * const *attribute_names,
                                    char * const *attribute_values,
                                    guint n_attributes, gint line, gint pos,
                                    GError **error, gpointer user_data)
{
    ObXmlFile *cfg = user_data;
    ObActionsList *oblist;
    GList *actions, *l;
    gchar *key;
    const char *exec_line = NULL;
    LXHotkeyAttr *action;
    LXHotkeyApp *app = NULL;
    LXHotkeyGlobal *act;
    guint i;

    if (!cfg->stack) { /* empty keybind tag, just ignore it and remove */
        g_warning("Openbox config: empty keybind tag in rc.xml, going to remove it");
        fm_xml_file_item_destroy(item);
        return TRUE;
    }
    oblist = cfg->stack->data;
    if (oblist->parent != item) { /* corruption! */
        g_set_error_literal(error, LXKEYS_OB_ERROR, LXKEYS_PARSE_ERROR,
                            _("Internal error."));
        return FALSE;
    }
    /* just remove top stack item, all actions are already there */
    actions = oblist->list;
    g_free(oblist);
    cfg->stack = g_list_delete_link(cfg->stack, cfg->stack);
    /* and clear junk if there were actions there - e.g. from mouse section */
    clear_stack(cfg);
    action = actions->data;
    /* decide where to put: execs or actions */
    if (children && !children->next && /* exactly one child which is an action */
        fm_xml_file_item_get_tag(children->data) == ObXmlFile_action) {
        if (strcmp(action->name, "Execute") == 0) { /* and action is Execute */
            FmXmlFileItem *exec = fm_xml_file_item_find_child(children->data,
                                                              ObXmlFile_command);
            if (!exec)
                exec = fm_xml_file_item_find_child(children->data, ObXmlFile_execute);
            /* if exec is NULL then it's invalid action after all */
            if (exec)
            {
                /* not empty exec line was verified in the handler */
                exec_line = fm_xml_file_item_get_data(fm_xml_file_item_find_child(exec, FM_XML_FILE_TEXT), NULL);
                for (l = cfg->execs; l; l = l->next)
                    /* if exec line is equal to one gathered already */
                    if (strcmp(((LXHotkeyApp *)l->data)->exec, exec_line) == 0 &&
                        /* and it has no secondary keybinding */
                        ((LXHotkeyApp *)l->data)->accel2 == NULL &&
                        /* and options are also equal */
                        options_equal(((LXHotkeyApp *)l->data)->options, action->subopts))
                    {
                        /* then just add this keybinding to found one */
                        app = (LXHotkeyApp *)l->data;
                        break;
                    }
            }
        }
    }
    /* find a "key" attribute and save its value as accel1 */
    for (i = 0; i < n_attributes; i++)
        if (g_strcmp0(attribute_names[i], "key") == 0)
            break;
    if (i == n_attributes) { /* no name in XML! */
        g_set_error_literal(error, LXKEYS_OB_ERROR, LXKEYS_PARSE_ERROR,
                            _("rc.xml error: no key is set for a keybind."));
        free_options(actions);
        return FALSE;
    } else
        key = obkey_to_key(attribute_values[i]);
    /* if that exec already exists then reuse it and set as accel2 */
    if (app) {
        app->accel2 = key;
        app->data2 = item;
    /* otherwise create LXHotkeyApp or LXHotkeyGlobal and add it to the list */
    } else if (exec_line) {
        app = g_new0(LXHotkeyApp, 1);
        app->accel1 = key;
        app->exec = g_strdup(exec_line);
        app->data1 = item;
        app->options = action->subopts;
        /* remove exec line from options, it should be in XML but not in LXHotkeyApp */
        for (l = app->options; l; ) {
            LXHotkeyAttr *opt = l->data;
            l = l->next;
            if (strcmp(opt->name, "command") == 0 || strcmp(opt->name, "execute") == 0) {
                app->options = g_list_remove(app->options, opt);
                lkxeys_attr_free(opt);
            }
        }
        action->subopts = NULL;
        cfg->execs = g_list_prepend(cfg->execs, app);
    } else {
        for (l = cfg->actions; l; l = l->next)
            /* if the same actions list was gathered already */
            if (options_equal((act = l->data)->actions, actions)
                /* and it has no secondary keybinding */
                && act->accel2 == NULL)
                break;
        if (l == NULL) {
            act = g_new0(LXHotkeyGlobal, 1);
            act->accel1 = key;
            act->data1 = item;
            act->actions = actions;
            actions = NULL;
            cfg->actions = g_list_prepend(cfg->actions, act);
        } else { /* actions exist in list so reuse it adding a second keybinding */
            act->accel2 = key;
            act->data2 = item;
        }
    }
    free_options(actions);
    return TRUE;
}

/* collect all children of FmXmlFileItem into LXHotkeyAttr list
   removing from stack if found there
   since actions cannot be mixed with options, just
   ignore anything not collected into any ObActionsList */
static GList *resolve_item(GList **stack, GList *children, GList **value,
                           GError **error)
{
    GList *child, *l, *items = NULL;
    ObActionsList *act; /* stack item */
    FmXmlFileItem *item; /* child item */
    LXHotkeyAttr *data;

    for (child = children; child; child = child->next) {
        item = child->data;
        if (fm_xml_file_item_get_tag(item) == FM_XML_FILE_TEXT) { /* value! */
            *value = g_list_prepend(*value,
                                    g_strdup(fm_xml_file_item_get_data(item, NULL)));
            continue;
        }
        if (fm_xml_file_item_get_tag(item) == ObXmlFile_action) { /* stray action? */
            g_set_error_literal(error, LXKEYS_OB_ERROR, LXKEYS_PARSE_ERROR,
                                _("Invalid rc.xml: action with a sub-action."));
            free_options(items);
            return NULL;
        }
        data = lxhotkey_attr_new();
        data->name = g_strdup(fm_xml_file_item_get_tag_name(item));
        /* find if item is in stack, then remove from there and use ready list */
        for (l = *stack; l; l = l->next) {
            act = l->data;
            if (act->parent == item) {
                *stack = g_list_delete_link(*stack, l);
                data->subopts = act->list;
                data->has_actions = TRUE;
                g_free(act); /* release the ObActionsList */
                break;
            }
        }
        /* otherwise collect children by recursive call */
        if (l == NULL) {
            GError *err = NULL;

            l = fm_xml_file_item_get_children(item);
            data->subopts = resolve_item(stack, l, &data->values, &err);
            g_list_free(l);
            if (err) {
                g_propagate_error(error, err);
                free_options(items);
                lkxeys_attr_free(data);
                return NULL;
            }
        }
        /* add the item to the list */
        items = g_list_prepend(items, data);
    }
    return g_list_reverse(items);
}

static gboolean is_on_stack(GList *stack, FmXmlFileItem *item)
{
    while (stack) {
        if (((ObActionsList *)stack->data)->parent == item)
            return TRUE;
        stack = stack->next;
    }
    return FALSE;
}

/* push new item on the stack, return top item */
static ObActionsList *put_on_stack_top(ObXmlFile *cfg, FmXmlFileItem *parent)
{
    ObActionsList *oblist = g_new0(ObActionsList, 1);

    oblist->parent = parent;
    cfg->stack = g_list_prepend(cfg->stack, oblist);
    return oblist;
}

/* (parent/children) stack (changes)
<k><a1><f><a2/> = (f/-) f (+f)
             +<a3><x><a4/> => (x/-) f x:4 (+x)
             +<a5/> => (x/-) f x:45 ()
             +</x></a3> => (f/x:45) f:3-x:45 (-x)
             +</f></a1> => (k/f:3) k:1-f:3 (-f +k)
             +<a6><h><a7><t><a8/> => (t/-) k:1 t:8 (+t)
             +</t></a7> => (h/t:8) k:1 h:7-t:8 (-t +h)
             +</h></a6> => (k/h:7) k:16-h:7 (-h)
             +<a9><b><c><d><a0><e><g><az/> => (g/-) k:16 g:z (+g)
             +</g></e></a0> => (d/e-g:z) k:16 d:0-e-g:z (-g +d)
             +</d></c></b></a9> => (k/b-c-d:0) k:169-b-c-d:0 (-d)
             +<a3><l><m><ay/> => (m/-) k:169 m:y (+m)
             +</m><n><a2><o/></a2> => (n/o) k:169 m:y n:2-o (+n)
             +</n></l></a3> => (k/l-{m:y|n:2}) k:1693-l-{m:y|n:2} (-m -n)
             +</k> => (-/1693) - (-k)
*/

static gboolean tag_handler_action(FmXmlFileItem *item, GList *children,
                                   char * const *attribute_names,
                                   char * const *attribute_values,
                                   guint n_attributes, gint line, gint pos,
                                   GError **error, gpointer user_data)
{
    /* if parent is on top of stack then it's another action of the same parent */
    /* if parent exists deeper on stack after resolving then it's curruption */
    /* if parent doesn't exist on stack but some of children is then that child
       is finished, replace with this parent on stack after resolving children */
    /* if parent doesn't exist on stack neither any child is then it got
       deeper so just add it on stack */
    ObXmlFile *cfg = user_data;
    LXHotkeyAttr *data;
    ObActionsList *oblist;
    FmXmlFileItem *parent;
    GError *err = NULL;
    guint i;

    /* if section keyboard already finished then ignore this */
    if (cfg->keyboard) {
        /* see notes in tag_handler_keyboard() as well */
        return TRUE;
    }

    /* create a LXHotkeyAttr */
    data = lxhotkey_attr_new();
    //data->has_actions = FALSE; /* action can have only options, not sub-actions! */
    /* resolve all children of this action, clearing from stack */
    data->subopts = resolve_item(&cfg->stack, children, &data->values, &err);
    if (err) {
        g_propagate_error(error, err);
        lkxeys_attr_free(data);
        return FALSE;
    }
    /* find a "name" attribute and set it as name */
    for (i = 0; i < n_attributes; i++)
        if (g_strcmp0(attribute_names[i], "name") == 0)
            break;
    if (i == n_attributes) { /* no name in XML! */
        g_set_error_literal(error, LXKEYS_OB_ERROR, LXKEYS_PARSE_ERROR,
                            _("rc.xml error: no name is set for action."));
        lkxeys_attr_free(data);
        return FALSE;
    } else
        data->name = g_strdup(attribute_values[i]);
    /* add this action to the parent's list */
    parent = fm_xml_file_item_get_parent(item);
    if (!is_on_stack(cfg->stack, parent))
        oblist = put_on_stack_top(cfg, parent);
    else if ((oblist = cfg->stack->data)->parent != parent) { /* corruption */
        g_set_error_literal(error, LXKEYS_OB_ERROR, LXKEYS_PARSE_ERROR,
                            _("Internal error."));
        lkxeys_attr_free(data);
        return FALSE;
    }
    oblist->list = g_list_append(oblist->list, data);
    return TRUE;
}

static gboolean tag_handler_command(FmXmlFileItem *item, GList *children,
                                    char * const *attribute_names,
                                    char * const *attribute_values,
                                    guint n_attributes, gint line, gint pos,
                                    GError **error, gpointer user_data)
{
    FmXmlFileItem *text = fm_xml_file_item_find_child(item, FM_XML_FILE_TEXT);

    if (text == NULL) {
        /* check if value is not empty */
        g_set_error(error, LXKEYS_OB_ERROR, LXKEYS_PARSE_ERROR,
                    _("rc.xml error: empty tag <%s> is prohibited."),
                    fm_xml_file_item_get_tag_name(item));
        return FALSE;
    }
    return TRUE;
}


static void obcfg_free(gpointer config)
{
    ObXmlFile *cfg = (ObXmlFile *)config;

    g_free(cfg->path);
    g_object_unref(cfg->xml);
    g_list_free_full(cfg->actions, (GDestroyNotify)lkxeys_action_free);
    g_list_free_full(cfg->execs, (GDestroyNotify)lkxeys_app_free);
    clear_stack(cfg);
    g_list_free(cfg->added_tags);
    g_free(cfg);
}

static gpointer obcfg_load(gpointer config, GError **error)
{
    ObXmlFile *cfg = (ObXmlFile *)config;
    gchar *contents = NULL;
    GError *err = NULL;
    gsize len;

    if (config) {
        /* just discard any changes if any exist */
        FmXmlFile *old_xml = cfg->xml;

        cfg->xml = fm_xml_file_new(old_xml);
        g_object_unref(old_xml);
        g_list_free_full(cfg->actions, (GDestroyNotify)lkxeys_action_free);
        g_list_free_full(cfg->execs, (GDestroyNotify)lkxeys_app_free);
        cfg->actions = NULL;
        cfg->execs = NULL;
        cfg->keyboard = NULL;
    } else {
        const char *session;

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
        session = g_getenv("DESKTOP_SESSION");
        if (session == NULL)
            session = g_getenv("GDMSESSION");
        if (session == NULL)
            session = g_getenv("XDG_CURRENT_DESKTOP");
        if (g_strcmp0(session, "Lubuntu") == 0)
            cfg->path = g_build_filename(g_get_user_config_dir(), "openbox",
                                         "lubuntu-rc.xml", NULL);
        else if (g_strcmp0(session, "LXDE") == 0)
            cfg->path = g_build_filename(g_get_user_config_dir(), "openbox",
                                         "lxde-rc.xml", NULL);
        else if (g_strcmp0(session, "LXDE-pi") == 0)
            cfg->path = g_build_filename(g_get_user_config_dir(), "openbox",
                                         "lxde-pi-rc.xml", NULL);
        else
            cfg->path = g_build_filename(g_get_user_config_dir(), "openbox",
                                         "rc.xml", NULL);
    }

    /* try to load ~/.config/openbox/$xml */
    if (!g_file_get_contents(cfg->path, &contents, &len, NULL)) {
        /* if it does not exist then try to load $XDG_SYSTEM_CONFDIR/openbox/rc.xml */
        const gchar * const *dirs;
        char *path = NULL;

        for (dirs = g_get_system_config_dirs(); dirs[0]; dirs++) {
            path = g_build_filename(dirs[0], "openbox", "rc.xml", NULL);
            if (g_file_get_contents(path, &contents, &len, NULL))
                break;
            g_free(path);
            path = NULL;
        }
        if (path == NULL) { /* failed to load */
            g_set_error_literal(error, LXKEYS_OB_ERROR, LXKEYS_FILE_ERROR,
                                _("Could not find the rc.xml file anywhere."));
            obcfg_free(cfg);
            return NULL;
        }
        g_free(path);
    }
    /* parse the found rc.xml file */
    if (!fm_xml_file_parse_data(cfg->xml, contents, len, &err, cfg)
        || fm_xml_file_finish_parse(cfg->xml, &err) == NULL) {
        g_propagate_error(error, err);
        g_free(contents);
        obcfg_free(cfg);
        return NULL;
    }
    g_free(contents);
    return cfg;
}

static gboolean obcfg_save(gpointer config, GError **error)
{
    ObXmlFile *cfg = (ObXmlFile *)config;
    char *contents;
    gsize len;
    gboolean ret = FALSE;

    /* save as ~/.config/openbox/$xml */
    contents = fm_xml_file_to_data(cfg->xml, &len, error);
    if (contents) {
        /* workaround on libfm-extra bug on save data without DTD */
        if (contents[0] == '\n')
            ret = g_file_set_contents(cfg->path, contents+1, len-1, error);
        else
            ret = g_file_set_contents(cfg->path, contents, len, error);
        g_free(contents);
    }
    if (ret)
        ret = restart_openbox(error);
    return ret;
}

static GList *obcfg_get_wm_keys(gpointer config, const char *mask, GError **error)
{
    ObXmlFile *cfg = (ObXmlFile *)config;
    GList *list = NULL, *l;
    LXHotkeyGlobal *data;

    if (cfg == NULL)
        g_set_error_literal(error, LXKEYS_OB_ERROR, LXKEYS_FILE_ERROR,
                            _("No WM configuration is available."));
    else for (l = cfg->actions; l; l = l->next) {
        data = l->data;
        if (mask == NULL || fnmatch(mask, data->accel1, 0) == 0
            || (data->accel2 != NULL && fnmatch(mask, data->accel2, 0) == 0))
            list = g_list_prepend(list, data);
    }
    return list;
}

static gboolean tag_null_handler(FmXmlFileItem *item, GList *children,
                                 char * const *attribute_names,
                                 char * const *attribute_values,
                                 guint n_attributes, gint line, gint pos,
                                 GError **error, gpointer user_data)
{
    return TRUE;
}

/* if opts==NULL then don't copy any LXHotkeyAttr below it */
static FmXmlFileItem *make_new_xml_item(ObXmlFile *cfg, LXHotkeyAttr *opt,
                                        GList **opts, gboolean is_action)
{
    FmXmlFileItem *item, *sub;
    GList *l;
    LXHotkeyAttr *act = NULL;

    if (is_action) {
        item = fm_xml_file_item_new(ObXmlFile_action);
        fm_xml_file_item_set_attribute(item, "name", opt->name);
    } else {
        FmXmlFileTag tag = FM_XML_FILE_TAG_NOT_HANDLED;

        /* find a tag in list by opt->name */
        for (l = cfg->added_tags; l; l = l->next)
            if (g_strcmp0(fm_xml_file_get_tag_name(cfg->xml, GPOINTER_TO_UINT(l->data)),
                          opt->name) == 0)
                break;
        if (l == NULL) {
            /* if not found then add to list */
            tag = fm_xml_file_set_handler(cfg->xml, opt->name, &tag_null_handler, FALSE, NULL);
            cfg->added_tags = g_list_prepend(cfg->added_tags, GUINT_TO_POINTER(tag));
        } else
            tag = GPOINTER_TO_UINT(l->data);
        item = fm_xml_file_item_new(tag);
        if (opt->values)
            fm_xml_file_item_append_text(item, opt->values->data, -1, FALSE);
    }
    if (opts != NULL) {
        /* make a copy if requested */
        act = lxhotkey_attr_new();
        act->name = g_strdup(opt->name);
        if (opt->values)
            act->values = g_list_prepend(NULL, g_strdup(opt->values->data));
        act->has_actions = opt->has_actions;
        *opts = g_list_append(*opts, act);
    }
    for (l = opt->subopts; l; l = l->next) {
        sub = make_new_xml_item(cfg, l->data, act ? &act->subopts : NULL,
                                opt->has_actions);
        fm_xml_file_item_append_child(item, sub);
    }
    return item;
}

/* if opts==NULL then don't make any LXHotkeyAttr below it */
static FmXmlFileItem *make_new_xml_binding(ObXmlFile *cfg, GList *actions,
                                           const gchar *accel, GList **opts,
                                           const gchar *exec)
{
    FmXmlFileItem *binding = fm_xml_file_item_new(ObXmlFile_keybind);
    FmXmlFileItem *item, *opt;
    char *obkey = key_to_obkey(accel);

    fm_xml_file_item_set_attribute(binding, "key", obkey);
    g_free(obkey);
    fm_xml_file_item_append_child(cfg->keyboard, binding);
    if (exec) {
        /* make <action name='Execute'><command>exec</command>..opts..</action>
           instead of ..<acts>.. */
        item = fm_xml_file_item_new(ObXmlFile_action);
        fm_xml_file_item_set_attribute(item, "name", "Execute");
        fm_xml_file_item_append_child(binding, item);
        opt = fm_xml_file_item_new(ObXmlFile_command);
        fm_xml_file_item_append_text(opt, exec, -1, FALSE);
        fm_xml_file_item_append_child(item, opt);
    }
    else
        item = binding;
    for (; actions; actions = actions->next) {
        opt = make_new_xml_item(cfg, actions->data, opts, (exec == NULL));
        fm_xml_file_item_append_child(item, opt);
    }
    return binding;
}

static inline void replace_key(FmXmlFileItem *item, const char *key, char **kptr)
{
    char *obkey = key_to_obkey(key);

    fm_xml_file_item_set_attribute(item, "key", obkey);
    g_free(obkey);
    g_free(*kptr);
    *kptr = g_strdup(key);
}

static gboolean obcfg_set_wm_key(gpointer config, LXHotkeyGlobal *data, GError **error)
{
    ObXmlFile *cfg = (ObXmlFile *)config;
    GList *l, *ll;
    LXHotkeyGlobal *act = NULL;

    if (cfg == NULL) {
        g_set_error_literal(error, LXKEYS_OB_ERROR, LXKEYS_FILE_ERROR,
                            _("No WM configuration is available."));
        return FALSE;
    } else if (data->actions == NULL) {
        g_set_error_literal(error, LXKEYS_OB_ERROR, LXKEYS_PARSE_ERROR,
                            _("Keybinding should activate at least one action."));
        return FALSE;
    }
    /* find if that action(s) is present */
    for (ll = cfg->actions; ll; ll = ll->next)
        if (options_equal((act = ll->data)->actions, data->actions))
            break;
    /* find if those keys are already bound elsewhere */
    for (l = cfg->actions; l; l = l->next) {
        if (l == ll)
            /* it's our action */
            continue;
        if (data->accel1) {
            if (strcmp(data->accel1, ((LXHotkeyGlobal *)l->data)->accel1) == 0 ||
                g_strcmp0(data->accel1, ((LXHotkeyGlobal *)l->data)->accel2) == 0)
                goto _accel1_bound;
        }
        if (data->accel2) {
            if (g_strcmp0(data->accel2, ((LXHotkeyGlobal *)l->data)->accel1) == 0 ||
                g_strcmp0(data->accel2, ((LXHotkeyGlobal *)l->data)->accel2) == 0)
                goto _accel2_bound;
        }
    }
    for (l = cfg->execs; l; l = l->next) {
        if (data->accel1) {
            if (strcmp(data->accel1, ((LXHotkeyApp *)l->data)->accel1) == 0 ||
                g_strcmp0(data->accel1, ((LXHotkeyApp *)l->data)->accel2) == 0) {
_accel1_bound:
                g_set_error(error, LXKEYS_OB_ERROR, LXKEYS_FILE_ERROR,
                            _("Hotkey '%s' is already bound to an action."),
                            data->accel1);
                return FALSE;
            }
        }
        if (data->accel2) {
            if (g_strcmp0(data->accel2, ((LXHotkeyApp *)l->data)->accel1) == 0 ||
                g_strcmp0(data->accel2, ((LXHotkeyApp *)l->data)->accel2) == 0) {
_accel2_bound:
                g_set_error(error, LXKEYS_OB_ERROR, LXKEYS_FILE_ERROR,
                            _("Hotkey '%s' is already bound to an action."),
                            data->accel2);
                return FALSE;
            }
        }
    }
    /* if found then either change keys or remove the keybinding */
    if (ll != NULL) {
        if (data->accel1 == NULL) {
            /* removal requested */
            if (act->data1)
                fm_xml_file_item_destroy(act->data1);
            if (act->data2)
                fm_xml_file_item_destroy(act->data2);
            lkxeys_action_free(act);
            cfg->actions = g_list_delete_link(cfg->actions, ll);
        } else {
            if (data->accel2 == NULL) {
                /* new data contains only one binding */
                if (g_strcmp0(act->accel1, data->accel1) == 0) {
                    /* accel1 not changed, just clear accel2 */
                    if (act->data2)
                        fm_xml_file_item_destroy(act->data2);
                    g_free(act->accel2);
                    act->accel2 = NULL;
                } else if (g_strcmp0(act->accel2, data->accel1) == 0) {
                    /* accel1 was removed */
                    if (act->data1)
                        fm_xml_file_item_destroy(act->data1);
                    g_free(act->accel1);
                    act->accel1 = act->accel2;
                    act->accel2 = NULL;
                } else {
                    /* full change */
                    replace_key(act->data1, data->accel1, &act->accel1);
                    if (act->data2)
                        fm_xml_file_item_destroy(act->data2);
                    g_free(act->accel2);
                    act->accel2 = NULL;
                }
            } else if (act->accel2 == NULL) {
                /* new data has two bindings and old data has 1 */
                if (g_strcmp0(act->accel1, data->accel1) == 0) {
                    /* add data->accel2 */
                    act->data2 = make_new_xml_binding(cfg, data->actions, data->accel2, NULL, NULL);
                    act->accel2 = g_strdup(data->accel2);
                } else if (g_strcmp0(act->accel1, data->accel2) == 0) {
                    /* add data->accel1 */
                    act->data2 = make_new_xml_binding(cfg, data->actions, data->accel1, NULL, NULL);
                    act->accel2 = g_strdup(data->accel1);
                } else {
                    /* replace key act->accel1 and add data->accel2 */
                    replace_key(act->data1, data->accel1, &act->accel1);
                    act->data2 = make_new_xml_binding(cfg, data->actions, data->accel2, NULL, NULL);
                    act->accel2 = g_strdup(data->accel2);
                }
            } else {
                /* both keys are present in old and new data */
                if (g_strcmp0(act->accel1, data->accel1) == 0) {
                    if (g_strcmp0(act->accel2, data->accel2) != 0)
                        /* just accel2 is changed */
                        replace_key(act->data2, data->accel2, &act->accel2);
                    /* else nothing changed */
                } else if (g_strcmp0(act->accel1, data->accel2) == 0) {
                    if (g_strcmp0(act->accel2, data->accel1) != 0)
                        /* replace accel2 with data->accel1 */
                        replace_key(act->data2, data->accel1, &act->accel2);
                    /* else keys just swapped */
                } else if (g_strcmp0(act->accel2, data->accel2) == 0) {
                    /* just accel1 is changed */
                    replace_key(act->data1, data->accel1, &act->accel1);
                } else if (g_strcmp0(act->accel2, data->accel1) == 0) {
                    /* replace accel1 with data->accel2 */
                    replace_key(act->data1, data->accel2, &act->accel1);
                } else {
                    /* both keys changed */
                    replace_key(act->data1, data->accel1, &act->accel1);
                    replace_key(act->data2, data->accel2, &act->accel2);
                }
            }
        }
    /* if not found then add a new keybinding */
    } else if (data->accel1) {
        act = g_new0(LXHotkeyGlobal, 1);
        act->data1 = make_new_xml_binding(cfg, data->actions, data->accel1, &act->actions, NULL);
        act->accel1 = g_strdup(data->accel1);
        /* do the same for accel2 if requested */
        if (data->accel2) {
            act->data2 = make_new_xml_binding(cfg, data->actions, data->accel2, NULL, NULL);
            act->accel2 = g_strdup(data->accel2);
        }
        cfg->actions = g_list_prepend(cfg->actions, act);
    }
    return TRUE;
}

static GList *obcfg_get_app_keys(gpointer config, const char *mask, GError **error)
{
    ObXmlFile *cfg = (ObXmlFile *)config;
    GList *list = NULL, *l;
    LXHotkeyApp *data;

    if (cfg == NULL)
        g_set_error_literal(error, LXKEYS_OB_ERROR, LXKEYS_FILE_ERROR,
                            _("No WM configuration is available."));
    else for (l = cfg->execs; l; l = l->next) {
        data = l->data;
        if (mask == NULL || fnmatch(mask, data->accel1, 0) == 0
            || (data->accel2 != NULL && fnmatch(mask, data->accel2, 0) == 0))
            list = g_list_prepend(list, data);
    }
    return list;
}

static gboolean obcfg_set_app_key(gpointer config, LXHotkeyApp *data, GError **error)
{
    ObXmlFile *cfg = (ObXmlFile *)config;
    GList *l, *ll;
    LXHotkeyApp *app = NULL;

    if (cfg == NULL) {
        g_set_error_literal(error, LXKEYS_OB_ERROR, LXKEYS_FILE_ERROR,
                            _("No WM configuration is available."));
        return FALSE;
    } else if (data->exec == NULL || data->exec[0] == '\0') {
        g_set_error_literal(error, LXKEYS_OB_ERROR, LXKEYS_PARSE_ERROR,
                            _("The exec line cannot be empty."));
        return FALSE;
    }
    /* find if that action(s) is present */
    for (ll = cfg->execs; ll; ll = ll->next)
        if (g_strcmp0((app = ll->data)->exec, data->exec) == 0
            && options_equal(app->options, data->options))
            break;
    /* find if those keys are already bound elsewhere */
    for (l = cfg->actions; l; l = l->next) {
        if (data->accel1) {
            if (strcmp(data->accel1, ((LXHotkeyGlobal *)l->data)->accel1) == 0 ||
                g_strcmp0(data->accel1, ((LXHotkeyGlobal *)l->data)->accel2) == 0)
                goto _accel1_bound;
        }
        if (data->accel2) {
            if (g_strcmp0(data->accel2, ((LXHotkeyGlobal *)l->data)->accel1) == 0 ||
                g_strcmp0(data->accel2, ((LXHotkeyGlobal *)l->data)->accel2) == 0)
                goto _accel2_bound;
        }
    }
    for (l = cfg->execs; l; l = l->next) {
        if (l == ll)
            /* it's our action */
            continue;
        if (data->accel1) {
            if (strcmp(data->accel1, ((LXHotkeyApp *)l->data)->accel1) == 0 ||
                g_strcmp0(data->accel1, ((LXHotkeyApp *)l->data)->accel2) == 0) {
_accel1_bound:
                g_set_error(error, LXKEYS_OB_ERROR, LXKEYS_FILE_ERROR,
                            _("Hotkey '%s' is already bound to an action."),
                            data->accel1);
                return FALSE;
            }
        }
        if (data->accel2) {
            if (g_strcmp0(data->accel2, ((LXHotkeyApp *)l->data)->accel1) == 0 ||
                g_strcmp0(data->accel2, ((LXHotkeyApp *)l->data)->accel2) == 0) {
_accel2_bound:
                g_set_error(error, LXKEYS_OB_ERROR, LXKEYS_FILE_ERROR,
                            _("Hotkey '%s' is already bound to an action."),
                            data->accel2);
                return FALSE;
            }
        }
    }
    /* if found then either change keys or remove the keybinding */
    if (ll != NULL) {
        if (data->accel1 == NULL) {
            /* removal requested */
            if (app->data1)
                fm_xml_file_item_destroy(app->data1);
            if (app->data2)
                fm_xml_file_item_destroy(app->data2);
            lkxeys_app_free(app);
            cfg->execs = g_list_delete_link(cfg->execs, ll);
        } else {
            if (data->accel2 == NULL) {
                /* new data contains only one binding */
                if (g_strcmp0(app->accel1, data->accel1) == 0) {
                    /* accel1 not changed, just clear accel2 */
                    if (app->data2)
                        fm_xml_file_item_destroy(app->data2);
                    g_free(app->accel2);
                    app->accel2 = NULL;
                } else if (g_strcmp0(app->accel2, data->accel1) == 0) {
                    /* accel1 was removed */
                    if (app->data1)
                        fm_xml_file_item_destroy(app->data1);
                    g_free(app->accel1);
                    app->accel1 = app->accel2;
                    app->accel2 = NULL;
                } else {
                    /* full change */
                    replace_key(app->data1, data->accel1, &app->accel1);
                    if (app->data2)
                        fm_xml_file_item_destroy(app->data2);
                    g_free(app->accel2);
                    app->accel2 = NULL;
                }
            } else if (app->accel2 == NULL) {
                /* new data has two bindings and old data has 1 */
                if (g_strcmp0(app->accel1, data->accel1) == 0) {
                    /* add data->accel2 */
                    app->data2 = make_new_xml_binding(cfg, data->options, data->accel2, NULL, data->exec);
                    app->accel2 = g_strdup(data->accel2);
                } else if (g_strcmp0(app->accel1, data->accel2) == 0) {
                    /* add data->accel1 */
                    app->data2 = make_new_xml_binding(cfg, data->options, data->accel1, NULL, data->exec);
                    app->accel2 = g_strdup(data->accel1);
                } else {
                    /* replace key app->accel1 and add data->accel2 */
                    replace_key(app->data1, data->accel1, &app->accel1);
                    app->data2 = make_new_xml_binding(cfg, data->options, data->accel2, NULL, data->exec);
                    app->accel2 = g_strdup(data->accel2);
                }
            } else {
                /* both keys are present in old and new data */
                if (g_strcmp0(app->accel1, data->accel1) == 0) {
                    if (g_strcmp0(app->accel2, data->accel2) != 0)
                        /* just accel2 is changed */
                        replace_key(app->data2, data->accel2, &app->accel2);
                    /* else nothing changed */
                } else if (g_strcmp0(app->accel1, data->accel2) == 0) {
                    if (g_strcmp0(app->accel2, data->accel1) != 0)
                        /* replace accel2 with data->accel1 */
                        replace_key(app->data2, data->accel1, &app->accel2);
                    /* else keys just swapped */
                } else if (g_strcmp0(app->accel2, data->accel2) == 0) {
                    /* just accel1 is changed */
                    replace_key(app->data1, data->accel1, &app->accel1);
                } else if (g_strcmp0(app->accel2, data->accel1) == 0) {
                    /* replace accel1 with data->accel2 */
                    replace_key(app->data1, data->accel2, &app->accel1);
                } else {
                    /* both keys changed */
                    replace_key(app->data1, data->accel1, &app->accel1);
                    replace_key(app->data2, data->accel2, &app->accel2);
                }
            }
        }
    /* if not found then add a new keybinding */
    } else if (data->accel1) {
        app = g_new0(LXHotkeyApp, 1);
        app->exec = g_strdup(data->exec);
        app->data1 = make_new_xml_binding(cfg, data->options, data->accel1, &app->options, data->exec);
        app->accel1 = g_strdup(data->accel1);
        /* do the same for accel2 if requested */
        if (data->accel2) {
            app->data2 = make_new_xml_binding(cfg, data->options, data->accel2, NULL, data->exec);
            app->accel2 = g_strdup(data->accel2);
        }
        cfg->execs = g_list_prepend(cfg->execs, app);
    }
    return TRUE;
}

static GList *obcfg_get_wm_actions(gpointer config, GError **error)
{
    if (!available_wm_actions)
        available_wm_actions = convert_options(list_actions);
    return available_wm_actions;
}


static GList *obcfg_get_app_options(gpointer config, GError **error)
{
    if (!available_wm_actions)
    {
        GList *l, *opts = NULL;
        LXHotkeyAttr *opt;

        available_wm_actions = convert_options(list_actions);
        for (l = available_app_options; l; l = l->next)
        {
            opt = l->data;
            if (strcmp(opt->name, "command") != 0)
                /* remove exec line from available options, it's App->exec */
                opts = g_list_prepend(opts, l->data);
        }
        available_app_options = g_list_reverse(opts);
    }
    return available_app_options;
}


FM_DEFINE_MODULE(lxhotkey, Openbox)

LXHotkeyPluginInit fm_module_init_lxhotkey = {
    .load = obcfg_load,
    .save = obcfg_save,
    .free = obcfg_free,
    .get_wm_keys = obcfg_get_wm_keys,
    .set_wm_key = obcfg_set_wm_key,
    .get_wm_actions = obcfg_get_wm_actions,
    .get_app_keys = obcfg_get_app_keys,
    .set_app_key = obcfg_set_app_key,
    .get_app_options = obcfg_get_app_options
};
