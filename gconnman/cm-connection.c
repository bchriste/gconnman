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


/*
 * ConnMan provides Connection information across DBus with the following
 * returned via GetProperties:
 *
 * ... todo ... fill out rest of table...
 *
 * This file implements base methods for parsing the DBus data
 * received for a Connection, allocating a Connection structure and providing
 * hooks to be signalled on Connection changes property changes.
 */
#include <string.h>

#include "gconnman-internal.h"

G_DEFINE_TYPE (CmConnection, connection, G_TYPE_OBJECT);
#define CONNECTION_ERROR connection_error_quark ()

#define CM_CONNECTION_GET_PRIVATE(obj)                         \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj),                  \
                                CM_TYPE_CONNECTION,            \
                                CmConnectionPrivate))

struct _CmConnectionPrivate
{
  CmManager *manager;
  gchar *path;
  CmConnectionType type;
  DBusGProxy *proxy;
  gboolean signals_added;

  gchar *interface;
  guint strength;
  gboolean default_connection;
  CmDevice *device;
  CmNetwork *network;
  gchar *ipv4_method;
  gchar *ipv4_address;
  gchar *ipv4_gateway;
  gchar *ipv4_broadcast;
  gchar *ipv4_nameserver;
  gchar *ipv4_netmask;
};

static void connection_property_change_handler_proxy (DBusGProxy *, const gchar *,
						  GValue *, gpointer);
enum
{
  SIGNAL_UPDATE,
  SIGNAL_INTERFACE_CHANGED,
  SIGNAL_STRENGTH_CHANGED,
  SIGNAL_DEFAULT_CHANGED,
  SIGNAL_TYPE_CHANGED,
  SIGNAL_IPV4_METHOD_CHANGED,
  SIGNAL_IPV4_ADDRESS_CHANGED,
  SIGNAL_IPV4_GATEWAY_CHANGED,
  SIGNAL_IPV4_BROADCAST_CHANGED,
  SIGNAL_IPV4_NAMESERVER_CHANGED,
  SIGNAL_IPV4_NETMASK_CHANGED,
  SIGNAL_DEVICE_CHANGED,
  SIGNAL_NETWORK_CHANGED,
  SIGNAL_LAST
};

static gint connection_signals[SIGNAL_LAST];

static GQuark
connection_error_quark (void)
{
  return g_quark_from_static_string ("connection-error-quark");
}

static void
connection_emit_updated (CmConnection *connection)
{
  g_signal_emit (connection, connection_signals[SIGNAL_UPDATE], 0 /* detail */);
}

static void
connection_update_property (const gchar *key, GValue *value, CmConnection *connection)
{
  CmConnectionPrivate *priv = connection->priv;
  gchar *tmp;

  // FIXME: use intern strings??
  if (!strcmp ("Interface", key))
  {
    g_free (priv->interface);
    priv->interface = g_value_dup_string (value);
    g_signal_emit (connection, connection_signals[SIGNAL_INTERFACE_CHANGED], 0);
  }
  else if (!strcmp ("Strength", key))
  {
    priv->strength= g_value_get_uchar (value);
    g_signal_emit (connection, connection_signals[SIGNAL_STRENGTH_CHANGED], 0);
  }
  else if (!strcmp ("Default", key))
  {
    priv->default_connection = g_value_get_boolean (value);
    g_signal_emit (connection, connection_signals[SIGNAL_DEFAULT_CHANGED], 0);
  }
  else if (!strcmp ("Type", key))
  {
    const gchar *type;
    type = g_value_get_string (value);
    if (!strcmp (type, "wifi"))
      priv->type = CONNECTION_WIFI;
    else if (!strcmp (type, "wimax"))
      priv->type = CONNECTION_WIMAX;
    else if (!strcmp (type, "bluetooth"))
      priv->type = CONNECTION_BLUETOOTH;
    else if (!strcmp (type, "cellular"))
      priv->type = CONNECTION_CELLULAR;
    else if (!strcmp (type, "ethernet"))
      priv->type = CONNECTION_ETHERNET;
    else
    {
      g_debug ("Unknown connection type on %s: %s\n",
               cm_connection_get_interface (connection), type);
      priv->type = CONNECTION_UNKNOWN;
    }
    g_signal_emit (connection, connection_signals[SIGNAL_TYPE_CHANGED], 0);
  }
  else if (!strcmp ("IPv4.Method", key))
  {
    g_free (priv->ipv4_method);
    priv->ipv4_method = g_value_dup_string (value);
    g_signal_emit (connection, connection_signals[SIGNAL_IPV4_METHOD_CHANGED], 0);
  }
  else if (!strcmp ("IPv4.Address", key))
  {
    g_free (priv->ipv4_address);
    priv->ipv4_address = g_value_dup_string (value);
    g_signal_emit (connection, connection_signals[SIGNAL_IPV4_ADDRESS_CHANGED], 0);
  }
  else if (!strcmp ("IPv4.Gateway", key))
  {
    g_free (priv->ipv4_gateway);
    priv->ipv4_gateway = g_value_dup_string (value);
    g_signal_emit (connection, connection_signals[SIGNAL_IPV4_GATEWAY_CHANGED], 0);

  }
  else if (!strcmp ("IPv4.Broadcast", key))
  {
    g_free (priv->ipv4_broadcast);
    priv->ipv4_broadcast = g_value_dup_string (value);
    g_signal_emit (connection, connection_signals[SIGNAL_IPV4_BROADCAST_CHANGED], 0);
  }
  else if (!strcmp ("IPv4.Nameserver", key))
  {
    g_free (priv->ipv4_nameserver);
    priv->ipv4_nameserver = g_value_dup_string (value);
    g_signal_emit (connection, connection_signals[SIGNAL_IPV4_NAMESERVER_CHANGED], 0);
  }
  else if (!strcmp ("IPv4.Netmask", key))
  {
    g_free (priv->ipv4_netmask);
    priv->ipv4_netmask = g_value_dup_string (value);
    g_signal_emit (connection, connection_signals[SIGNAL_IPV4_NETMASK_CHANGED], 0);
  }
  else if (!strcmp ("Device", key))
  {
    gchar *path = g_value_get_boxed (value);

    priv->device = cm_manager_find_device (priv->manager, path);

    if (!priv->device)
    {
      g_debug ("Device not found by manager %s: %s\n", path, __FUNCTION__);
    }
    else
    {
      g_signal_emit (connection, connection_signals[SIGNAL_DEVICE_CHANGED], 0);
    }
  }
  else if (!strcmp ("Network", key))
  {
    GError *error = NULL;
    gchar *path = g_value_get_boxed (value);

    if (priv->network)
    {
      g_object_unref (priv->network);
      priv->network = NULL;
    }

    priv->network = internal_network_new (priv->proxy, priv->device, path,
                                          priv->manager, &error);

    if (!priv->network)
    {
      g_debug ("network_new failed in %s: %s\n", __FUNCTION__,
               error->message);
      g_error_free (error);
    }
    else
    {
      g_signal_emit (connection, connection_signals[SIGNAL_NETWORK_CHANGED], 0);
    }
  }
  else
  {
    tmp = g_strdup_value_contents (value);
    g_debug ("Unhandled Connection property on %s: %s = %s\n",
             cm_connection_get_interface (connection), key, tmp);
    g_free (tmp);
  }
}

static void
connection_property_change_handler_proxy (DBusGProxy *proxy,
				          const gchar *key,
				          GValue *value,
				          gpointer data)
{
  CmConnection *connection = data;

  connection_update_property (key, value, connection);

  connection_emit_updated (connection);
}

static void
connection_get_properties_call_notify (DBusGProxy *proxy,
				   DBusGProxyCall *call,
				   gpointer data)
{
  CmConnection *connection = data;
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

  g_hash_table_foreach (properties, (GHFunc)connection_update_property, connection);
  g_hash_table_unref (properties);
  connection_emit_updated (connection);
}

CmConnection *
internal_connection_new (DBusGProxy *proxy, const gchar *path, CmManager *manager, GError **error)
{
  CmConnection *connection;
  CmConnectionPrivate *priv;
  DBusGProxyCall *call;

  connection = g_object_new (CM_TYPE_CONNECTION, NULL);
  if (!connection)
  {
    g_set_error (error, CONNECTION_ERROR, CONNECTION_ERROR_NO_MEMORY,
                 "Unable to allocate CmConnection.");
    return NULL;
  }

  priv = connection->priv;
  priv->type = CONNECTION_UNKNOWN;
  priv->manager = manager;

  priv->path = g_strdup (path);
  if (!priv->path)
  {
    g_set_error (error, CONNECTION_ERROR, CONNECTION_ERROR_NO_MEMORY,
                 "Unable to allocate connection path.");
    g_object_unref (connection);
    return NULL;
  }

  priv->proxy = dbus_g_proxy_new_from_proxy (
    proxy, CONNMAN_CONNECTION_INTERFACE, path);
  if (!priv->proxy)
  {
    g_set_error (error, CONNECTION_ERROR, CONNECTION_ERROR_CONNMAN_INTERFACE,
                 "No interface for %s/%s from Connman.",
                 CONNMAN_CONNECTION_INTERFACE, path);
    g_object_unref (connection);
    return NULL;
  }

  dbus_g_proxy_add_signal (
    priv->proxy, "PropertyChanged",
    G_TYPE_STRING, G_TYPE_VALUE, G_TYPE_INVALID);

  dbus_g_proxy_connect_signal (
    priv->proxy, "PropertyChanged",
    G_CALLBACK (connection_property_change_handler_proxy),
    connection, NULL);

  call = dbus_g_proxy_begin_call (priv->proxy, "GetProperties",
                                  connection_get_properties_call_notify,
                                  connection, NULL, G_TYPE_INVALID);
  if (!call)
  {
    g_set_error (error, CONNECTION_ERROR, CONNECTION_ERROR_CONNMAN_GET_PROPERTIES,
                 "Invocation of GetProperties failed.");
    g_object_unref (connection);
    return NULL;
  }

  return connection;
}

const gchar *
cm_connection_get_interface (CmConnection *connection)
{
  CmConnectionPrivate *priv = connection->priv;
  return priv->interface;
}

gboolean
cm_connection_get_default (CmConnection *connection)
{
  CmConnectionPrivate *priv = connection->priv;
  return priv->default_connection;
}

guint
cm_connection_get_strength (CmConnection *connection)
{
  CmConnectionPrivate *priv = connection->priv;
  return priv->strength;
}

const gchar *
cm_connection_get_path (CmConnection *connection)
{
  CmConnectionPrivate *priv= connection->priv;
  return priv->path;
}

gboolean
cm_connection_is_same (const CmConnection *connection, const gchar *path)
{
  CmConnectionPrivate *priv= connection->priv;
  return !strcmp (priv->path, path);
}

CmConnectionType
cm_connection_get_type (const CmConnection *connection)
{
  CmConnectionPrivate *priv = connection->priv;
  return priv->type;
}

CmDevice *
cm_connection_get_device (CmConnection *connection)
{
  CmConnectionPrivate *priv = connection->priv;
  return priv->device;
}

CmNetwork *
cm_connection_get_network (CmConnection *connection)
{
  CmConnectionPrivate *priv = connection->priv;
  return priv->network;
}

gchar *
cm_connection_get_ipv4_method (CmConnection *connection)
{
  CmConnectionPrivate *priv = connection->priv;
  return priv->ipv4_method;
}

gchar *
cm_connection_get_ipv4_address (CmConnection *connection)
{
  CmConnectionPrivate *priv = connection->priv;
  return priv->ipv4_address;
}

gchar *
cm_connection_get_ipv4_gateway (CmConnection *connection)
{
  CmConnectionPrivate *priv = connection->priv;
  return priv->ipv4_gateway;
}

gchar *
cm_connection_get_ipv4_broadcast (CmConnection *connection)
{
  CmConnectionPrivate *priv = connection->priv;
  return priv->ipv4_broadcast;
}

gchar *
cm_connection_get_ipv4_nameserver (CmConnection *connection)
{
  CmConnectionPrivate *priv = connection->priv;
  return priv->ipv4_nameserver;
}

gchar *
cm_connection_get_ipv4_netmask (CmConnection *connection)
{
  CmConnectionPrivate *priv = connection->priv;
  return priv->ipv4_netmask;
}

/*****************************************************************************
 *
 *
 * GObject class_init, init, and finalize methods
 *
 *
 *****************************************************************************/

static void
connection_dispose (GObject *object)
{
  CmConnection *connection = CM_CONNECTION (object);
  CmConnectionPrivate *priv = connection->priv;

  if (priv->proxy)
  {
    dbus_g_proxy_disconnect_signal (
    priv->proxy, "PropertyChanged",
    G_CALLBACK (connection_property_change_handler_proxy),
    connection);

    g_object_unref (priv->proxy);
    priv->proxy = NULL;
  }

  if (priv->network)
  {
    g_object_unref (priv->network);
    priv->network = NULL;
  }

  if (priv->device)
  {
    g_object_unref (priv->device);
    priv->device = NULL;
  }

  priv->manager = NULL;

  G_OBJECT_CLASS (connection_parent_class)->finalize (object);
}

static void
connection_finalize (GObject *object)
{
  CmConnection *connection = CM_CONNECTION (object);
  CmConnectionPrivate *priv = connection->priv;

  g_free (priv->interface);
  g_free (priv->ipv4_method);
  g_free (priv->ipv4_address);
  g_free (priv->ipv4_gateway);
  g_free (priv->ipv4_broadcast);
  g_free (priv->ipv4_nameserver);
  g_free (priv->ipv4_netmask);

  G_OBJECT_CLASS (connection_parent_class)->finalize (object);
}

static void
connection_init (CmConnection *self)
{
  self->priv = CM_CONNECTION_GET_PRIVATE (self);
  self->priv->default_connection = FALSE;
  self->priv->type = CONNECTION_UNKNOWN;
  self->priv->strength = 0;
  self->priv->interface = NULL;
  self->priv->device = NULL;
  self->priv->network = NULL;
  self->priv->ipv4_method = NULL;
  self->priv->ipv4_address = NULL;
  self->priv->ipv4_gateway = NULL;
  self->priv->ipv4_broadcast = NULL;
  self->priv->ipv4_nameserver = NULL;
  self->priv->ipv4_netmask = NULL;
  self->priv->manager = NULL;
}

static void
connection_class_init (CmConnectionClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = connection_finalize;
  gobject_class->dispose = connection_dispose;

  connection_signals[SIGNAL_UPDATE] = g_signal_new (
    "connection-updated",
    G_TYPE_FROM_CLASS (gobject_class),
    G_SIGNAL_RUN_LAST,
    0,
    NULL, NULL,
    g_cclosure_marshal_VOID__VOID,
    G_TYPE_NONE, 0);
  connection_signals[SIGNAL_INTERFACE_CHANGED] = g_signal_new (
    "interface-changed",
    G_TYPE_FROM_CLASS (gobject_class),
    G_SIGNAL_RUN_LAST,
    0,
    NULL, NULL,
    g_cclosure_marshal_VOID__VOID,
    G_TYPE_NONE, 0);
  connection_signals[SIGNAL_STRENGTH_CHANGED] = g_signal_new (
    "strength-changed",
    G_TYPE_FROM_CLASS (gobject_class),
    G_SIGNAL_RUN_LAST,
    0,
    NULL, NULL,
    g_cclosure_marshal_VOID__VOID,
    G_TYPE_NONE, 0);
  connection_signals[SIGNAL_DEFAULT_CHANGED] = g_signal_new (
    "default-changed",
    G_TYPE_FROM_CLASS (gobject_class),
    G_SIGNAL_RUN_LAST,
    0,
    NULL, NULL,
    g_cclosure_marshal_VOID__VOID,
    G_TYPE_NONE, 0);
  connection_signals[SIGNAL_TYPE_CHANGED] = g_signal_new (
    "type-changed",
    G_TYPE_FROM_CLASS (gobject_class),
    G_SIGNAL_RUN_LAST,
    0,
    NULL, NULL,
    g_cclosure_marshal_VOID__VOID,
    G_TYPE_NONE, 0);
  connection_signals[SIGNAL_IPV4_METHOD_CHANGED] = g_signal_new (
    "ipv4-method-changed",
    G_TYPE_FROM_CLASS (gobject_class),
    G_SIGNAL_RUN_LAST,
    0,
    NULL, NULL,
    g_cclosure_marshal_VOID__VOID,
    G_TYPE_NONE, 0);
  connection_signals[SIGNAL_IPV4_ADDRESS_CHANGED] = g_signal_new (
    "ipv4-address-changed",
    G_TYPE_FROM_CLASS (gobject_class),
    G_SIGNAL_RUN_LAST,
    0,
    NULL, NULL,
    g_cclosure_marshal_VOID__VOID,
    G_TYPE_NONE, 0);
  connection_signals[SIGNAL_IPV4_GATEWAY_CHANGED] = g_signal_new (
    "ipv4-gateway-changed",
    G_TYPE_FROM_CLASS (gobject_class),
    G_SIGNAL_RUN_LAST,
    0,
    NULL, NULL,
    g_cclosure_marshal_VOID__VOID,
    G_TYPE_NONE, 0);
  connection_signals[SIGNAL_IPV4_BROADCAST_CHANGED] = g_signal_new (
    "ipv4-broadcast-changed",
    G_TYPE_FROM_CLASS (gobject_class),
    G_SIGNAL_RUN_LAST,
    0,
    NULL, NULL,
    g_cclosure_marshal_VOID__VOID,
    G_TYPE_NONE, 0);
  connection_signals[SIGNAL_IPV4_NAMESERVER_CHANGED] = g_signal_new (
    "ipv4-nameserver-changed",
    G_TYPE_FROM_CLASS (gobject_class),
    G_SIGNAL_RUN_LAST,
    0,
    NULL, NULL,
    g_cclosure_marshal_VOID__VOID,
    G_TYPE_NONE, 0);
  connection_signals[SIGNAL_IPV4_NETMASK_CHANGED] = g_signal_new (
    "ipv4-netmask-changed",
    G_TYPE_FROM_CLASS (gobject_class),
    G_SIGNAL_RUN_LAST,
    0,
    NULL, NULL,
    g_cclosure_marshal_VOID__VOID,
    G_TYPE_NONE, 0);
  connection_signals[SIGNAL_DEVICE_CHANGED] = g_signal_new (
    "device-changed",
    G_TYPE_FROM_CLASS (gobject_class),
    G_SIGNAL_RUN_LAST,
    0,
    NULL, NULL,
    g_cclosure_marshal_VOID__VOID,
    G_TYPE_NONE, 0);
  connection_signals[SIGNAL_NETWORK_CHANGED] = g_signal_new (
    "network-changed",
    G_TYPE_FROM_CLASS (gobject_class),
    G_SIGNAL_RUN_LAST,
    0,
    NULL, NULL,
    g_cclosure_marshal_VOID__VOID,
    G_TYPE_NONE, 0);

  g_type_class_add_private (gobject_class, sizeof (CmConnectionPrivate));
}

const gchar *
cm_connection_type_to_string (CmConnectionType type)
{
  switch (type)
  {
  case CONNECTION_WIFI:
    return "Wireless";

  case CONNECTION_WIMAX:
    return "WiMAX";

  case CONNECTION_BLUETOOTH:
    return "Bluetooth";

  case CONNECTION_CELLULAR:
    return "Cellular";

  case CONNECTION_ETHERNET:
    return "Ethernet";

  case CONNECTION_UNKNOWN:
  default:
    break;
  }

  return "Unknown";
}
