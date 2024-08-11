/*
 * idle-plugin.c
 *
 * Copyright (C) 2022 Ivaylo Dimitrov <ivo.g.dimitrov.75@gmail.com>
 *
 * This library is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library. If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include "config.h"

#include <glib/gi18n-lib.h>
#include <hildon/hildon.h>
#include <hildon-uri.h>
#include <libaccounts/account-plugin.h>
#include <librtcom-accounts-widgets/rtcom-account-plugin.h>
#include <librtcom-accounts-widgets/rtcom-dialog-context.h>
#include <librtcom-accounts-widgets/rtcom-edit.h>
#include <librtcom-accounts-widgets/rtcom-login.h>
#include <librtcom-accounts-widgets/rtcom-param-string.h>

#define USE_SERVER_NAME 0
typedef struct _MatrixPluginClass MatrixPluginClass;
typedef struct _MatrixPlugin MatrixPlugin;

struct _MatrixPlugin
{
  RtcomAccountPlugin parent_instance;
};

struct _MatrixPluginClass
{
  RtcomAccountPluginClass parent_class;
};

struct _MatrixPluginPrivate
{
  GList *accounts;
};

typedef struct _MatrixPluginPrivate MatrixPluginPrivate;

#define PRIVATE(plugin) \
  (MatrixPluginPrivate *)matrix_plugin_get_instance_private(\
  (MatrixPlugin *)(plugin))

ACCOUNT_DEFINE_PLUGIN_WITH_PRIVATE(MatrixPlugin,
                                   matrix_plugin,
                                   RTCOM_TYPE_ACCOUNT_PLUGIN);

static void
matrix_plugin_init(MatrixPlugin *self)
{
  RtcomAccountService *service;

  RTCOM_ACCOUNT_PLUGIN(self)->name = "matrix";
  RTCOM_ACCOUNT_PLUGIN(self)->capabilities =
      RTCOM_PLUGIN_CAPABILITY_ALLOW_MULTIPLE |
      RTCOM_PLUGIN_CAPABILITY_PASSWORD;
  service = rtcom_account_plugin_add_service(RTCOM_ACCOUNT_PLUGIN(self),
                                             "tank/matrix");
#if USE_SERVER_NAME
  rtcom_account_service_set_account_domains(service, "");
#else
  (void)service;
#endif
}

struct _matrix_account
{
  AccountItem *item;
  RtcomDialogContext *context;
};

typedef struct _matrix_account matrix_account;

static gboolean
on_store_settings(RtcomAccountItem *item, GError **error, matrix_account *ma)
{
  const GValue *account = g_hash_table_lookup(item->new_params, "account");

  if (account)
  {
    gchar **arr = g_strsplit(g_value_get_string(account), ":", 2);

    if (arr[0])
      rtcom_account_item_store_param_string(item, "user", g_value_get_string(account));

    if (arr[1])
      rtcom_account_item_store_param_string(item, "server", arr[1]);

    g_strfreev(arr);
  }

  return TRUE;
}

static void
matrix_plugin_context_init(RtcomAccountPlugin *plugin,
                           RtcomDialogContext *context)
{
  MatrixPluginPrivate *priv = PRIVATE(plugin);
  matrix_account *ma = g_slice_new(matrix_account);
  gboolean editing;
  AccountItem *account;
  GtkWidget *page;
  static const gchar *invalid_chars_re = "['\"<>&;#\\s]"; /* FIXME UPDATE */

  editing = account_edit_context_get_editing(ACCOUNT_EDIT_CONTEXT(context));
  account = account_edit_context_get_account(ACCOUNT_EDIT_CONTEXT(context));

  ma->item = account;
  ma->context = context;
  priv->accounts = g_list_prepend(priv->accounts, ma);

  g_object_add_weak_pointer(G_OBJECT(ma->context), (gpointer *)&ma->context);

  g_signal_connect_after(account, "store-settings",
                         G_CALLBACK(on_store_settings), ma);

  if (editing)
  {
    page =
      g_object_new(
        RTCOM_TYPE_EDIT,
        "user-server-separator" , ":",
        "username-must-have-at-separator", TRUE,
        "username-field", "account",
        "username-invalid-chars-re", invalid_chars_re,
        "items-mask", RTCOM_ACCOUNT_PLUGIN(plugin)->capabilities,
        "account", account,
        NULL);

    rtcom_edit_append_widget(
      RTCOM_EDIT(page),
        g_object_new(GTK_TYPE_LABEL,
                     "label", _("accounts_fi_device_name"),
                     "xalign", 0.0,
                     NULL),
        g_object_new (RTCOM_TYPE_PARAM_STRING,
                      "field", "device",
                      "required", TRUE,
                      NULL));
  }
  else
  {
    page =
      g_object_new(
        RTCOM_TYPE_LOGIN,
        "user-server-separator" , ":",
        "username-placeholder", "@username:homeserver.tld",
        "username-must-have-at-separator", TRUE,
        "username-field", "account",
        "username-invalid-chars-re", invalid_chars_re,
        "items-mask", RTCOM_ACCOUNT_PLUGIN(plugin)->capabilities,
        "account", account,
        NULL);

    rtcom_login_append_widget(
      RTCOM_LOGIN(page),
        g_object_new(GTK_TYPE_LABEL,
                     "label", _("accounts_fi_device_name"),
                     "xalign", 0.0,
                     NULL),
        g_object_new (RTCOM_TYPE_PARAM_STRING,
                      "text", "Maemo",
                      "field", "device",
                      "required", TRUE,
                      NULL));
  }

  rtcom_dialog_context_set_start_page(context, page);
}

static void
matrix_account_destroy(gpointer data)
{
  matrix_account *ma = data;

  if (ma->context)
  {
    g_signal_handlers_disconnect_matched(
      ma->item,
      G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_FUNC,
      0, 0, NULL, on_store_settings, ma);
  }

  g_slice_free(matrix_account, ma);
}

static void
matrix_plugin_finalize(GObject *object)
{
  MatrixPluginPrivate *priv = PRIVATE(object);

  g_list_free_full(priv->accounts, matrix_account_destroy);

  G_OBJECT_CLASS(matrix_plugin_parent_class)->finalize(object);
}

static void
matrix_plugin_class_init(MatrixPluginClass *klass)
{
  G_OBJECT_CLASS(klass)->finalize = matrix_plugin_finalize;
  RTCOM_ACCOUNT_PLUGIN_CLASS(klass)->context_init = matrix_plugin_context_init;
}

