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

#include "debug.h"
#include "gconnman-internal.h"

G_DEFINE_TYPE (CmConnection, connection, G_TYPE_OBJECT);
#define CONNECTION_ERROR connection_error_quark ()

#define CM_CONNECTION_GET_PRIVATE(obj)                         \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj),                  \
                                CM_TYPE_CONNECTION,            \
                                CmConnectionPrivate))

struct _CmConnectionPrivate
{
  gchar *path;
  CmConnectionType type;
  DBusGProxy *proxy;
  gboolean signals_added;

  gchar *interface;
  guint strength;
  gboolean default_connection;

  DBusGProxyCall *get_properties_proxy_call;

  GValue pending_property_value;
  gchar *pending_property_name;
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
connection_proxy_call_destroy (CmConnection *connection, DBusGProxyCall **proxy_call)
{
  CmConnectionPrivate *priv = connection->priv;
  if (*proxy_call == NULL)
    return;
  dbus_g_proxy_cancel_call (priv->proxy, *proxy_call);
  *proxy_call = NULL;
}

static void
connection_update_property (const gchar *key, GValue *value, CmConnection *connection)
{
  CmConnectionPrivate *priv = connection->priv;
  gchar *tmp;

  if (!strcmp ("Interface", key))
  {
    g_free (priv->interface);
    priv->interface = g_strdup (g_value_get_string (value));
    g_signal_emit (connection, connection_signals[SIGNAL_INTERFACE_CHANGED], 0);
    return;
  }

  if (!strcmp ("Strength", key))
  {
    priv->strength= g_value_get_uint (value);
    g_signal_emit (connection, connection_signals[SIGNAL_STRENGTH_CHANGED], 0);
    return;
  }

  if (!strcmp ("Default", key))
  {
    priv->default_connection = g_value_get_boolean (value);
    g_signal_emit (connection, connection_signals[SIGNAL_DEFAULT_CHANGED], 0);
    return;
  }

  if (!strcmp ("Type", key))
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
      g_print ("Unknown connection type on %s: %s\n",
               cm_connection_get_interface (connection), type);
      priv->type = CONNECTION_UNKNOWN;
    }
    g_signal_emit (connection, connection_signals[SIGNAL_TYPE_CHANGED], 0);
    return;
  }

  tmp = g_strdup_value_contents (value);
  g_print ("Unhandled property on %s: %s = %s\n",
           cm_connection_get_interface (connection), key, tmp);
  g_free (tmp);
}

static void
connection_property_change_handler_proxy (DBusGProxy *proxy,
				          const gchar *key,
				          GValue *value,
				          gpointer data)
{
  CmConnection *connection = data;
  gchar *tmp = g_strdup_value_contents (value);
  g_print ("PropertyChange on %s: %s = %s\n",
           cm_connection_get_interface (connection), key, tmp);
  g_free (tmp);

  connection_update_property (key, value, connection);

  connection_emit_updated (connection);
}

static void
connection_get_properties_call_notify (DBusGProxy *proxy,
				   DBusGProxyCall *call,
				   gpointer data)
{
  CmConnection *connection = data;
  CmConnectionPrivate *priv = connection->priv;
  GError *error = NULL;
  GHashTable *properties = NULL;

  if (priv->get_properties_proxy_call != call)
    g_print ("%s Call mismatch!\n", __FUNCTION__);

  if (!dbus_g_proxy_end_call (
	proxy, call, &error,
	/* OUT values */
	dbus_g_type_get_map ("GHashTable", G_TYPE_STRING, G_TYPE_VALUE),
	&properties, G_TYPE_INVALID))
  {
    g_print ("Error calling dbus_g_proxy_end_call in %s: %s\n",
             __FUNCTION__, error->message);
    g_clear_error (&error);
    return;
  }

  g_hash_table_foreach (properties, (GHFunc)connection_update_property, connection);
  g_hash_table_unref (properties);
  connection_emit_updated (connection);

  priv->get_properties_proxy_call = NULL;
}

CmConnection *
internal_connection_new (DBusGProxy *proxy, const gchar *path, GError **error)
{
  CmConnection *connection;
  CmConnectionPrivate *priv;

  connection = g_object_new (CM_TYPE_CONNECTION, NULL);
  if (!connection)
  {
    g_set_error (error, CONNECTION_ERROR, CONNECTION_ERROR_NO_MEMORY,
                 "Unable to allocate CmConnection.");
    return NULL;
  }

  priv = connection->priv;
  priv->type = CONNECTION_UNKNOWN;

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

  priv->get_properties_proxy_call = dbus_g_proxy_begin_call (
    priv->proxy, "GetProperties",
    connection_get_properties_call_notify, connection, NULL,
    G_TYPE_INVALID);
  if (!priv->get_properties_proxy_call)
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

/*****************************************************************************
 *
 *
 * GObject class_init, init, and finalize methods
 *
 *
 *****************************************************************************/

static void
connection_finalize (GObject *object)
{
  CmConnection *connection = CM_CONNECTION (object);
  CmConnectionPrivate *priv = connection->priv;

  dbus_g_proxy_disconnect_signal (
    priv->proxy, "PropertyChanged",
    G_CALLBACK (connection_property_change_handler_proxy),
    connection);

  connection_proxy_call_destroy (connection, &priv->get_properties_proxy_call);

  g_free (priv->interface);

  if (priv->proxy)
    g_object_unref (priv->proxy);

  G_OBJECT_CLASS (connection_parent_class)->finalize (object);
}

static void
connection_init (CmConnection *self)
{
  self->priv = CM_CONNECTION_GET_PRIVATE (self);
  self->priv->default_connection = FALSE;
  self->priv->type = CONNECTION_UNKNOWN;
  self->priv->strength = 0;
}

static void
connection_class_init (CmConnectionClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = connection_finalize;

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
    return "WiMax";

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