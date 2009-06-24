/*
 * Gconnman - a GObject wrapper for the Connman D-Bus API
 * Copyright © 2009, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU Lesser General Public License,
 * version 2.1, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA
 *
 * Written by:	James Ketrenos <jketreno@linux.intel.com>
 *		Joshua Lock <josh@linux.intel.com>
 *
 */

#include <glib.h>
#include <string.h> /* strcmp */
#include <stdlib.h> /* system */

#include "connman-marshal.h"
#include "gconnman-internal.h"

G_DEFINE_TYPE (CmManager, manager, G_TYPE_OBJECT);

#define MANAGER_ERROR manager_error_quark ()

#define CM_MANAGER_GET_PRIVATE(obj)                        \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj),                  \
                                CM_TYPE_MANAGER,           \
                                CmManagerPrivate))

struct _CmManagerPrivate
{
  DBusGConnection *connection;
  DBusGProxy *proxy;
  gboolean offline_mode;
  GList *devices;
  GList *services;
  GList *connections;
  gchar *state;
  gchar *policy;

  GValue pending_property_value;
  gchar *pending_property_name;
};

static void manager_property_change_handler_proxy (DBusGProxy *, const gchar *,
                                                   GValue *, gpointer);
enum
{
  SIGNAL_UPDATE,
  SIGNAL_STATE_CHANGED,
  SIGNAL_OFFLINE_MODE_CHANGED,
  SIGNAL_DEVICES_CHANGED,
  SIGNAL_SERVICES_CHANGED,
  SIGNAL_CONNECTIONS_CHANGED,
  SIGNAL_POLICY_CHANGED,
  SIGNAL_LAST
};

static gint manager_signals[SIGNAL_LAST];

static GQuark
manager_error_quark (void)
{
  return g_quark_from_static_string ("manager-error-quark");
}

static void
manager_emit_updated (CmManager *manager)
{
  g_signal_emit (manager, manager_signals[SIGNAL_UPDATE], 0 /* detail */);
}

CmDevice *
cm_manager_find_device (CmManager *manager, const gchar *opath)
{
  CmManagerPrivate *priv = manager->priv;
  CmDevice *device = NULL;
  GList *iter;
  const gchar *dpath;

  for (iter = priv->devices; iter != NULL; iter = iter->next)
  {
    device = iter->data;
    dpath = cm_device_get_path (device);

    if (g_strcmp0 (opath, dpath) == 0)
      return device;
  }

  return NULL;
}

CmService *
cm_manager_find_service (CmManager *manager, const gchar *opath)
{
  CmManagerPrivate *priv = manager->priv;
  CmService *service = NULL;
  GList *iter;
  const gchar *spath;

  for (iter = priv->services; iter != NULL; iter = iter->next)
  {
    service = iter->data;
    spath = cm_service_get_path (service);

    if (g_strcmp0 (opath, spath) == 0)
      return service;
  }

  return NULL;
}

CmConnection *
cm_manager_find_connection (CmManager *manager, const gchar *opath)
{
  CmManagerPrivate *priv = manager->priv;
  CmConnection *connection = NULL;
  GList *iter;
  const gchar *cpath;

  for (iter = priv->connections; iter != NULL; iter = iter->next)
  {
    connection = iter->data;
    cpath = cm_connection_get_path (connection);

    if (g_strcmp0 (opath, cpath) == 0)
      return connection;
  }

  return NULL;
}

static void
manager_update_property (const gchar *key, GValue *value, CmManager *manager)
{
  CmManagerPrivate *priv = manager->priv;
  gchar *tmp;

  if (!strcmp ("Devices", key))
  {
    GPtrArray *devices = g_value_get_boxed (value);
    gint i;
    const gchar *path = NULL;
    GList *iter;

    /* First remove stale devices */
    for (iter = priv->devices; iter != NULL; iter = iter->next)
    {
      CmDevice *dev = iter->data;
      gboolean found = FALSE;

      for (i = 0; i < devices->len && !found; i++)
      {
        path = g_ptr_array_index (devices, i);

        if (g_strcmp0 (path, cm_device_get_path (dev)) == 0)
        {
          found = TRUE;
        }
      }

      /* device not in retrieved list, delete from our list */
      if (!found)
      {
        priv->devices = g_list_delete_link (priv->devices, iter);
      }
    }

    /* iterate retrieved list, add any new items to our list */
    for (i = 0; i < devices->len; i++)
    {
      path = g_ptr_array_index (devices, i);
      CmDevice *device;
      GError *error = NULL;

      device = cm_manager_find_device (manager, path);
      if (!device)
      {
        device = internal_device_new (priv->proxy, path, &error);
        if (!device)
        {
          g_debug ("device_new failed in %s: %s\n", __FUNCTION__,
                   error->message);
          g_error_free (error);
          continue;
        }
        else
        {
          priv->devices = g_list_append (priv->devices, device);
        }
      }
    }

    g_signal_emit (manager, manager_signals[SIGNAL_DEVICES_CHANGED], 0);
  }
  else if (!strcmp ("Connections", key))
  {
    GPtrArray *connections = g_value_get_boxed (value);
    gint i;
    const gchar *path = NULL;
    GList *iter;

    /* First remove stale connections */
    for (iter = priv->connections; iter != NULL; iter = iter->next)
    {
      CmConnection *con = iter->data;
      gboolean found = FALSE;

      for (i = 0; i < connections->len && !found; i++)
      {
        path = g_ptr_array_index (connections, i);

        if (g_strcmp0 (path, cm_connection_get_path (con)) == 0)
        {
          found = TRUE;
        }
      }

      /* connection not in retrieved list, delete from our list */
      if (!found)
      {
        priv->connections = g_list_delete_link (priv->connections, iter);
      }
    }

    /* iterate retrieved list, add any new items to our list */
    for (i = 0; i < connections->len; i++)
    {
      path = g_ptr_array_index (connections, i);
      CmConnection *connection;
      GError *error = NULL;

      connection = cm_manager_find_connection (manager, path);
      if (!connection)
      {
        connection = internal_connection_new (priv->proxy, path, &error);
        if (!connection)
        {
          g_debug ("connection_new failed in %s: %s\n", __FUNCTION__,
                   error->message);
          g_error_free (error);
          continue;
        }
        else
        {
          priv->connections = g_list_append (priv->connections, connection);
        }
      }
    }

    g_signal_emit (manager, manager_signals[SIGNAL_CONNECTIONS_CHANGED], 0);
  }
  else if (!strcmp ("Services", key))
  {
    GPtrArray *services = g_value_get_boxed (value);
    gint i;
    const gchar *path = NULL;
    GList *iter;

    /* First remove stale services */
    for (iter = priv->services; iter != NULL; iter = iter->next)
    {
      CmService *serv = iter->data;
      gboolean found = FALSE;

      for (i = 0; i < services->len && !found; i++)
      {
        path = g_ptr_array_index (services, i);

        if (g_strcmp0 (path, cm_service_get_path (serv)) == 0)
        {
          found = TRUE;
        }
      }

      /* service not in retrieved list, delete from our list */
      if (!found)
      {
        priv->services = g_list_delete_link (priv->services, iter);
      }
    }

    /* iterate retrieved list, add any new items to our list */
    for (i = 0; i < services->len; i++)
    {
      path = g_ptr_array_index (services, i);
      CmService *service;
      GError *error = NULL;

      service = cm_manager_find_service (manager, path);
      if (!service)
      {
        service = internal_service_new (priv->proxy, path, i, &error);
        if (!service)
        {
          g_debug ("service_new failed in %s: %s\n", __FUNCTION__,
                   error->message);
          g_error_free (error);
          continue;
        }
        else
        {
          priv->services = g_list_append (priv->services, service);
        }
      }
      else
      {
        /* services are sorted so update the order */
        cm_service_set_order (service, i);
      }
    }

    priv->services = g_list_sort (priv->services,
                                  (GCompareFunc) cm_service_compare);

    g_signal_emit (manager, manager_signals[SIGNAL_SERVICES_CHANGED], 0);
  }
  else if (!strcmp ("Profiles", key))
  {
  }
  else if (!strcmp ("ActiveProfile", key))
  {
    gchar *profile = g_value_get_boxed (value);
    /* FIXME: Finish this property */
  }
  else if (!strcmp ("OfflineMode", key))
  {
    priv->offline_mode = g_value_get_boolean (value);
    g_signal_emit (manager, manager_signals[SIGNAL_OFFLINE_MODE_CHANGED], 0);
  }
  else if (!strcmp ("State", key))
  {
    g_free (priv->state);
    priv->state = g_value_dup_string (value);
    g_signal_emit (manager, manager_signals[SIGNAL_STATE_CHANGED], 0);
  }
  else if (!strcmp ("Policy", key))
  {
    g_free (priv->policy);
    priv->policy = g_value_dup_string (value);
    g_signal_emit (manager, manager_signals[SIGNAL_POLICY_CHANGED], 0);
  }
  else
  {
    tmp = g_strdup_value_contents (value);
    g_debug ("Unhandled property on Manager: %s = %s\n",
             key, tmp);
    g_free (tmp);
  }
}

static void
manager_get_properties_call_notify (DBusGProxy *proxy,
                                    DBusGProxyCall *call,
                                    gpointer data)
{
  CmManager *manager = data;
  GError *error = NULL;
  GHashTable *properties = NULL;

  if (!dbus_g_proxy_end_call (
        proxy, call, &error,
        /* OUT values */
        dbus_g_type_get_map ("GHashTable", G_TYPE_STRING, G_TYPE_VALUE),
        &properties, G_TYPE_INVALID))
  {
    g_debug ("Error calling dbus_g_proxy_end_call in %s: %s\n",
             __FUNCTION__, error->message);
    g_error_free (error);
    return;
  }

  g_hash_table_foreach (properties, (GHFunc)manager_update_property, manager);
  g_hash_table_unref (properties);
  manager_emit_updated (manager);
}

gboolean
cm_manager_refresh (CmManager *manager)
{
  CmManagerPrivate *priv = manager->priv;
  DBusGProxyCall *call;

  /* Remove all the prior devices */
  while (priv->devices)
  {
    g_object_unref (priv->devices->data);
    priv->devices = g_list_delete_link (priv->devices, priv->devices);
  }

  /* Remove all the prior connections */
  while (priv->connections)
  {
    g_object_unref (priv->connections->data);
    priv->connections = g_list_delete_link (priv->connections, priv->connections);
  }

  /* Remove all the prior services */
  while (priv->services)
  {
    g_object_unref (priv->services->data);
    priv->services = g_list_delete_link (priv->services, priv->services);
  }

  call = dbus_g_proxy_begin_call (priv->proxy, "GetProperties",
                                  manager_get_properties_call_notify, manager,
                                  NULL, G_TYPE_INVALID);

  if (!call)
  {
    return FALSE;
  }

  return TRUE;
}


static void
manager_property_change_handler_proxy (DBusGProxy *proxy,
				      const gchar *key,
				      GValue *value,
				      gpointer data)
{
  CmManager *manager = data;

  manager_update_property (key, value, manager);

  manager_emit_updated (manager);
}

gboolean
manager_set_dbus_connection (CmManager *manager, GError **error)
{
  static gboolean dbus_init = FALSE;
  CmManagerPrivate *priv = manager->priv;

  if (!dbus_init)
  {
    dbus_init = TRUE;
    if (!g_thread_supported ())
    {
      g_thread_init (NULL);
    }
    dbus_g_thread_init ();

    /* Register the data types needed for marshalling the PropertyChanged
     * signal */
    dbus_g_object_register_marshaller (connman_marshal_VOID__STRING_BOXED,
                                       /* Return type */
                                       G_TYPE_NONE,
                                       /* Arguments */
                                       G_TYPE_STRING,
                                       G_TYPE_VALUE,
                                       /* EOL */
                                       G_TYPE_INVALID);
  }

  priv->connection = dbus_g_bus_get (DBUS_BUS_SYSTEM, error);
  if (!priv->connection)
    return FALSE;

  priv->proxy = dbus_g_proxy_new_for_name(
    priv->connection,
    CONNMAN_SERVICE, CONNMAN_MANAGER_PATH, CONNMAN_MANAGER_INTERFACE);
  if (!priv->proxy)
  {
    g_set_error (error, MANAGER_ERROR, MANAGER_ERROR_NO_CONNMAN,
                 "Unable to obtain proxy for %s:%s/%s",
                 CONNMAN_SERVICE, CONNMAN_MANAGER_PATH,
                 CONNMAN_MANAGER_INTERFACE);
    g_object_unref (priv->connection);
    priv->connection = NULL;
    return FALSE;
  }

  dbus_g_proxy_add_signal (
    priv->proxy, "PropertyChanged",
    G_TYPE_STRING, G_TYPE_VALUE, G_TYPE_INVALID);

  dbus_g_proxy_connect_signal (
    priv->proxy, "PropertyChanged",
    G_CALLBACK (manager_property_change_handler_proxy),
    manager, NULL);

  return TRUE;
}

CmManager *
cm_manager_new (GError **error)
{
  CmManager *manager = g_object_new (CM_TYPE_MANAGER, NULL);
  if (manager_set_dbus_connection (manager, error))
    return manager;
  g_object_unref (manager);
  return NULL;
}

static void
manager_set_property_call_notify (DBusGProxy *proxy,
                                 DBusGProxyCall *call,
                                 gpointer data)
{
  CmManager *manager = data;
  CmManagerPrivate *priv = manager->priv;
  GError *error = NULL;

  if (!dbus_g_proxy_end_call (proxy, call, &error, G_TYPE_INVALID))
  {
    g_debug ("Error calling dbus_g_proxy_end_call in %s on Manager: %s\n",
             __FUNCTION__, error->message);
    g_error_free (error);
  }
  else
  {
    manager_update_property (priv->pending_property_name,
                             &priv->pending_property_value,
                             manager);
    manager_emit_updated (manager);
  }

  g_free (priv->pending_property_name);
  g_value_unset (&priv->pending_property_value);
  priv->pending_property_name = NULL;
}

gboolean
manager_set_property (CmManager *manager, const gchar *property, GValue *value)
{
  CmManagerPrivate *priv = manager->priv;
  GError *error = NULL;
  DBusGProxyCall *call;

  priv->pending_property_name = g_strdup (property);
  g_value_init (&priv->pending_property_value, G_VALUE_TYPE (value));
  g_value_copy (value, &priv->pending_property_value);

  call = dbus_g_proxy_begin_call (priv->proxy, "SetProperty",
                                  manager_set_property_call_notify, manager,
                                  NULL, G_TYPE_STRING, property, G_TYPE_VALUE,
                                  value, G_TYPE_INVALID);

  if (!call)
  {
    g_debug ("SetProperty failed %s\n", error ? error->message : "Unknown");
    g_error_free (error);
    return FALSE;
  }

  return TRUE;
}

const GList *
cm_manager_get_devices (CmManager *manager)
{
  CmManagerPrivate *priv = manager->priv;
  return priv->devices;
}

const GList *
cm_manager_get_connections (CmManager *manager)
{
  CmManagerPrivate *priv = manager->priv;
  return priv->connections;
}

const GList *
cm_manager_get_services (CmManager *manager)
{
  CmManagerPrivate *priv = manager->priv;
  return priv->services;
}

gboolean
cm_manager_get_offline_mode (CmManager *manager)
{
  CmManagerPrivate *priv = manager->priv;
  return priv->offline_mode;
}

gboolean
cm_manager_set_offline_mode (CmManager *manager, gboolean offline)
{
  GValue value = { 0 };
  gboolean ret;

  g_value_init (&value, G_TYPE_BOOLEAN);
  g_value_set_boolean (&value, offline);
  ret = manager_set_property (manager, "OfflineMode", &value);
  g_value_unset (&value);

  return ret;
}

/*
 * The list of services is sorted by connman so the active service
 * should always be the first item in our list
 */
CmService *
cm_manager_get_active_service (CmManager *manager)
{
  CmManagerPrivate *priv = manager->priv;
  CmService *active = NULL;

  if (priv->services)
  {
    active = (CmService *) priv->services->data;
    if (!cm_service_get_connected (active))
    {
      active = NULL;
    }
  }
  else
  {
    active = NULL;
  }

  return active;
}

/*
 * Return the first connection marked as default, or NULL
 */
CmConnection *
cm_manager_get_active_connection (CmManager *manager)
{
  CmManagerPrivate *priv = manager->priv;
  GList *connections = priv->connections;
  CmConnection *conn = NULL;

  while (connections)
  {
    conn = (CmConnection *) connections->data;
    if (cm_connection_get_default (conn))
    {
        return conn;
    }
    connections = connections->next;
  }

  return NULL;
}

const gchar *
cm_manager_get_state (CmManager *manager)
{
  CmManagerPrivate *priv = manager->priv;
  return priv->state;
}

const gchar *
cm_manager_get_policy (CmManager *manager)
{
  CmManagerPrivate *priv = manager->priv;
  return priv->policy;
}

gboolean
cm_manager_set_policy (CmManager *manager, gchar *policy)
{
  GValue value = { 0 };
  gboolean ret;

  g_value_init (&value, G_TYPE_STRING);
  g_value_set_string (&value, policy);
  ret = manager_set_property (manager, "Policy", &value);
  g_value_unset (&value);

  return ret;
}

/*****************************************************************************
 *
 *
 * GObject class_init, init, and finalize methods
 *
 *
 *****************************************************************************/

static void
manager_dispose (GObject *object)
{
  CmManager *manager = CM_MANAGER (object);
  CmManagerPrivate *priv = manager->priv;

  while (priv->devices)
  {
    g_object_unref (priv->devices->data);
    priv->devices = g_list_delete_link (priv->devices, priv->devices);
  }

  while (priv->connections)
  {
    g_object_unref (priv->connections->data);
    priv->connections = g_list_delete_link (priv->connections, priv->connections);
  }

  while (priv->services)
  {
    g_object_unref (priv->services->data);
    priv->services = g_list_delete_link (priv->services, priv->services);
  }

  if (priv->proxy)
  {
    dbus_g_proxy_disconnect_signal (
    priv->proxy, "PropertyChanged",
    G_CALLBACK (manager_property_change_handler_proxy),
    manager);

    g_object_unref (priv->proxy);
    priv->proxy = NULL;
  }

  if (priv->connection)
    dbus_g_connection_unref (priv->connection);

  G_OBJECT_CLASS (manager_parent_class)->dispose (object);
}

static void
manager_finalize (GObject *object)
{
  CmManager *manager = CM_MANAGER (object);
  CmManagerPrivate *priv = manager->priv;

  g_free (priv->state);
  g_free (priv->policy);
  g_free (priv->pending_property_name);

  G_OBJECT_CLASS (manager_parent_class)->finalize (object);
}

static void
manager_init (CmManager *self)
{
  self->priv = CM_MANAGER_GET_PRIVATE (self);
  self->priv->state = NULL;
  self->priv->policy = NULL;
  self->priv->offline_mode = FALSE;
  self->priv->pending_property_name = NULL;
  self->priv->services = NULL;
  self->priv->devices = NULL;
  self->priv->connections = NULL;
}

static void
manager_class_init (CmManagerClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = manager_finalize;
  gobject_class->dispose = manager_dispose;

  manager_signals[SIGNAL_UPDATE] = g_signal_new (
    "manager-updated",
    G_TYPE_FROM_CLASS (gobject_class),
    G_SIGNAL_RUN_LAST,
    0,
    NULL, NULL,
    g_cclosure_marshal_VOID__VOID,
    G_TYPE_NONE, 0);
  manager_signals[SIGNAL_STATE_CHANGED] = g_signal_new (
    "state-changed",
    G_TYPE_FROM_CLASS (gobject_class),
    G_SIGNAL_RUN_LAST,
    0,
    NULL, NULL,
    g_cclosure_marshal_VOID__VOID,
    G_TYPE_NONE, 0);
  manager_signals[SIGNAL_OFFLINE_MODE_CHANGED] = g_signal_new (
    "offline-mode-changed",
    G_TYPE_FROM_CLASS (gobject_class),
    G_SIGNAL_RUN_LAST,
    0,
    NULL, NULL,
    g_cclosure_marshal_VOID__VOID,
    G_TYPE_NONE, 0);
  manager_signals[SIGNAL_DEVICES_CHANGED] = g_signal_new (
    "devices-changed",
    G_TYPE_FROM_CLASS (gobject_class),
    G_SIGNAL_RUN_LAST,
    0,
    NULL, NULL,
    g_cclosure_marshal_VOID__VOID,
    G_TYPE_NONE, 0);
  manager_signals[SIGNAL_CONNECTIONS_CHANGED] = g_signal_new (
    "connections-changed",
    G_TYPE_FROM_CLASS (gobject_class),
    G_SIGNAL_RUN_LAST,
    0,
    NULL, NULL,
    g_cclosure_marshal_VOID__VOID,
    G_TYPE_NONE, 0);
  manager_signals[SIGNAL_SERVICES_CHANGED] = g_signal_new (
    "services-changed",
    G_TYPE_FROM_CLASS (gobject_class),
    G_SIGNAL_RUN_LAST,
    0,
    NULL, NULL,
    g_cclosure_marshal_VOID__VOID,
    G_TYPE_NONE, 0);
  manager_signals[SIGNAL_POLICY_CHANGED] = g_signal_new (
    "policy-changed",
    G_TYPE_FROM_CLASS (gobject_class),
    G_SIGNAL_RUN_LAST,
    0,
    NULL, NULL,
    g_cclosure_marshal_VOID__VOID,
    G_TYPE_NONE, 0);

  g_type_class_add_private (gobject_class, sizeof (CmManagerPrivate));
}

