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
 * ConnMan provides WiFi Network information across DBus with the following
 * returned via GetProperties:
 *
 * Name          - user readable ASCIIZ text string
 * WiFi.SSID     - {,32} byte binary SSID [ can contain embedded \0 ]
 * Strength      - int [unknown unit]
 * Priority      - int [unknown unit]
 * Connected     - active network
 * WiFi.Mode     - ad-hoc, managed
 * WiFi.Security - wpa, wpa2, wep, none
 *
 * This file implements base methods for parsing the DBus data
 * received for a Network, allocating a Network structure and providing
 * hooks to be signalled on Network changes property changes.
 *
 * The Network objects are typically contained within a Device object.
 */
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <glib.h>

#include "gconnman-internal.h"

#define CM_NETWORK_ERROR network_error_quark ()

G_DEFINE_TYPE (CmNetwork, network, G_TYPE_OBJECT);

#define CM_NETWORK_GET_PRIVATE(obj)                                        \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj), CM_TYPE_NETWORK, CmNetworkPrivate))

struct _CmNetworkPrivate
{
  CmDevice *device;
  CmManager *manager;
  gchar *path;
  DBusGProxy *proxy;
  guchar *ssid;
  gint ssid_len;
  gchar *ssid_printable;
  guchar strength;
  guchar priority;
  gboolean connected;
  gchar *name;
  gchar *mode;
  gchar *security;
  gchar *passphrase;
  gchar *address;
  uint frequency;
  uint channel;
  CmNetworkInfoMask flags;
  time_t last_update;

  GValue pending_property_value;
  gchar *pending_property_name;
};

enum
{
  SIGNAL_UPDATE,
  SIGNAL_LAST
};

static gint network_signals[SIGNAL_LAST];

static GQuark
network_error_quark (void)
{
  return g_quark_from_static_string ("network-error-quark");
}

static void
network_emit_updated (CmNetwork *network)
{
  g_signal_emit (network, network_signals[SIGNAL_UPDATE], 0 /* detail */);
}


static inline gulong network_timer_elapsed_ms(const struct timeval *t1,
                                              const struct timeval *t2)
{
  return ((t2->tv_sec - t1->tv_sec) * 1000) +
    (t2->tv_usec - t1->tv_usec) / 1000;
}

static void network_property_change_handler_proxy (DBusGProxy *, const gchar *,
						   GValue *, gpointer);

static gchar *
network_printable_ssid_new (const guchar *ssid, int len)
{
  gchar *ret = g_new0 (gchar, len + 1);
  gint i;
  for (i = 0; i < len; i++)
  {
    if (!isprint (ssid[i]))
      ret[i] = '.';
    else
      ret[i] = ssid[i];
  }

  ret[len] = '\0';
  return ret;
}

static void
network_update_timestamp (CmNetwork *network)
{
  CmNetworkPrivate *priv = network->priv;
  priv->last_update = time (NULL);
}

static void
network_update_property (const gchar *key, GValue *value, CmNetwork *network)
{
  CmNetworkPrivate *priv = network->priv;
  gchar *tmp;

  network_update_timestamp (network);

  if (!strcmp ("WiFi.SSID", key))
  {
    GArray *ssid_bytes;
    gint i;

    g_free (priv->ssid);
    g_free (priv->ssid_printable);

    if (!G_VALUE_HOLDS_BOXED (value))
    {
      tmp = g_strdup_value_contents (value);
      g_free (tmp);
    }
    else
    {
      ssid_bytes = g_value_get_boxed (value);

      priv->ssid_len = ssid_bytes->len;
      priv->ssid = g_new0 (guchar, ssid_bytes->len);
      for (i = 0; i < priv->ssid_len; i++)
        priv->ssid[i] = g_array_index (ssid_bytes, guchar, i);
      priv->ssid_printable = network_printable_ssid_new (
        priv->ssid, priv->ssid_len);
      priv->flags |= NETWORK_INFO_SSID;
    }
  }
  else if (!strcmp ("Strength", key))
  {
    priv->strength = g_value_get_uchar (value);
    priv->flags |= NETWORK_INFO_STRENGTH;
  }
  else if (!strcmp ("Priority", key))
  {
    priv->priority = g_value_get_uchar (value);
    priv->flags |= NETWORK_INFO_PRIORITY;
  }
  else if (!strcmp ("Connected", key))
  {
    priv->connected = g_value_get_boolean (value);
    priv->flags |= NETWORK_INFO_CONNECTED;
  }
  else if (!strcmp ("WiFi.Mode", key))
  {
    g_free (priv->mode);
    priv->mode = g_value_dup_string (value);
    priv->flags |= NETWORK_INFO_MODE;
  }
  else if (!strcmp ("WiFi.Security", key))
  {
    g_free (priv->security);
    priv->security = g_value_dup_string (value);
    priv->flags |= NETWORK_INFO_SECURITY;
  }
  else if (!strcmp ("WiFi.Passphrase", key))
  {
    gchar *passphrase = g_value_dup_string (value);
    g_free (priv->passphrase);
    if (strlen (passphrase))
    {
      priv->passphrase = passphrase;
      priv->flags |= NETWORK_INFO_PASSPHRASE;
    }
    else
    {
      priv->passphrase = NULL;
      priv->flags &= ~NETWORK_INFO_PASSPHRASE;
    }
  }
  else if (!strcmp ("WiFi.Channel", key))
  {
    priv->channel = g_value_get_uint (value);
    priv->flags |= NETWORK_INFO_CHANNEL;
  }
  else if (!strcmp ("Name", key))
  {
    g_free (priv->name);
    priv->name = g_value_dup_string (value);
    priv->flags |= NETWORK_INFO_NAME;
  }
  else if (!strcmp ("Address", key))
  {
    g_free (priv->address);
    priv->address = g_value_dup_string (value);
    priv->flags |= NETWORK_INFO_ADDRESS;
  }
  else if (!strcmp ("Frequency", key))
  {
    priv->frequency = g_value_get_uint (value);
    priv->flags |= NETWORK_INFO_FREQUENCY;
  }
  else if (!strcmp ("Device", key))
  {
    gchar *path = g_value_get_boxed (value);
    priv->device = cm_manager_find_device (priv->manager, path);
  }
  else
  {
    tmp = g_strdup_value_contents (value);
    g_debug ("Unhandled property on %s: %s = %s\n",
             cm_network_get_name (network), key, tmp);
    g_free (tmp);
  }
  network_emit_updated (network);
}

static void
network_proxy_call_destroy (CmNetwork *network, DBusGProxyCall **proxy_call)
{
  CmNetworkPrivate *priv = network->priv;
  if (*proxy_call == NULL)
    return;
  dbus_g_proxy_cancel_call (priv->proxy, *proxy_call);
  *proxy_call = NULL;
}


static void
network_property_change_handler_proxy (DBusGProxy *proxy,
				       const gchar *key,
				       GValue *value,
				       gpointer data)
{
  CmNetwork *network = data;

  network_update_property (key, value, network);

  network_emit_updated (network);
}

static void
network_get_properties_call_notify (DBusGProxy *proxy,
                                    DBusGProxyCall *call,
                                    gpointer data)
{
  CmNetwork *network = data;
  GError *error = NULL;
  GHashTable *properties = NULL;
  gint count;

  if (!dbus_g_proxy_end_call (proxy, call, &error,
                              /* OUT values */
                              dbus_g_type_get_map ("GHashTable",
                                                   G_TYPE_STRING,
                                                   G_TYPE_VALUE),
                              &properties, G_TYPE_INVALID))
  {
    g_print ("Error calling dbus_g_proxy_end_call in %s on %s: %s\n",
             __FUNCTION__, cm_network_get_name (network), error->message);
    g_error_free (error);
    return;
  }

  count = g_hash_table_size (properties);

  g_hash_table_foreach (properties, (GHFunc)network_update_property, network);
  g_hash_table_unref (properties);
}

CmNetwork *
internal_network_new (DBusGProxy *proxy,
                      CmDevice *device, const gchar *path,
                      CmManager *manager, GError **error)
{
  CmNetwork *network;
  CmNetworkPrivate *priv;
  DBusGProxyCall *call;

  network = g_object_new (CM_TYPE_NETWORK, NULL);
  if (!network)
  {
    g_set_error (error, NETWORK_ERROR, NETWORK_ERROR_NO_MEMORY,
                 "Unable to allocate CmNetwork.");
    return NULL;
  }

  priv = network->priv;
  priv->device = device;
  priv->manager = manager;

  priv->path = g_strdup (path);
  if (!priv->path)
  {
    g_set_error (error, NETWORK_ERROR, NETWORK_ERROR_NO_MEMORY,
                 "Unable to allocate CmNetwork.");
    return NULL;
  }

  priv->proxy = dbus_g_proxy_new_from_proxy (
    proxy, CONNMAN_NETWORK_INTERFACE, path);
  if (!priv->proxy)
  {
    g_set_error (error, NETWORK_ERROR, NETWORK_ERROR_CONNMAN_INTERFACE,
                 "No interface for %s/%s from Connman.",
                 CONNMAN_NETWORK_INTERFACE, path);
    g_object_unref (network);
    return NULL;
  }

  dbus_g_proxy_add_signal (
    priv->proxy, "PropertyChanged",
    G_TYPE_STRING, G_TYPE_VALUE, G_TYPE_INVALID);

  dbus_g_proxy_connect_signal (
    priv->proxy, "PropertyChanged",
    G_CALLBACK (network_property_change_handler_proxy),
    network, NULL);

  call = dbus_g_proxy_begin_call (priv->proxy, "GetProperties",
                                  network_get_properties_call_notify,
                                  network, NULL,
                                  G_TYPE_INVALID);
  if (!call)
  {
    g_set_error (error, NETWORK_ERROR, NETWORK_ERROR_CONNMAN_GET_PROPERTIES,
                 "Invocation of GetProperties failed.");
    g_object_unref (network);
    return NULL;
  }

  return network;
}

gboolean
cm_network_is_connected (const CmNetwork *network)
{
  CmNetworkPrivate *priv = network->priv;
  return priv->connected;
}

const gchar *
cm_network_get_name (const CmNetwork *network)
{
  CmNetworkPrivate *priv = network->priv;
  return priv->name ? priv->name :
    (priv->ssid_printable ? priv->ssid_printable : priv->path);
}

static void
network_disconnect_call_notify (DBusGProxy *proxy,
                                DBusGProxyCall *call,
                                gpointer data)
{
  CmNetwork *network = data;
  GError *error = NULL;

  if (!dbus_g_proxy_end_call (proxy, call, &error, G_TYPE_INVALID))
  {
    g_debug ("Error calling dbus_g_proxy_end_call in %s on %s: %s\n",
             __FUNCTION__, cm_network_get_name (network), error->message);
    g_error_free (error);
  }
}

gboolean
cm_network_disconnect (CmNetwork *network)
{
  CmNetworkPrivate *priv = network->priv;
  GError *error = NULL;
  DBusGProxyCall *call;

  if (!priv->connected)
    return TRUE;

  call = dbus_g_proxy_begin_call (priv->proxy, "Disconnect",
                                  network_disconnect_call_notify, network,
                                  NULL, G_TYPE_INVALID);

  if (!call)
  {
    g_debug ("Disconnect failed: %s\n", error ? error->message : "Unknown");
    g_error_free (error);
    return FALSE;
  }

  return TRUE;
}

static void
network_connect_call_notify (DBusGProxy *proxy,
                             DBusGProxyCall *call,
                             gpointer data)
{
  CmNetwork *network = data;
  GError *error = NULL;

  if (!dbus_g_proxy_end_call (proxy, call, &error, G_TYPE_INVALID))
  {
    g_debug ("Error calling dbus_g_proxy_end_call in %s on %s: %s\n",
             __FUNCTION__, cm_network_get_name (network), error->message);
    g_error_free (error);
  }
}

gboolean
cm_network_connect (CmNetwork *network)
{
  CmNetworkPrivate *priv = network->priv;
  GError *error = NULL;
  DBusGProxyCall *call;

  if (priv->connected)
    return TRUE;

  if (cm_network_is_secure (network) && !cm_network_has_passphrase (network))
    return FALSE;

  call = dbus_g_proxy_begin_call (priv->proxy, "Connect",
                                  network_connect_call_notify, network, NULL,
                                  G_TYPE_INVALID);

  if (!call)
  {
    g_debug ("Connect failed: %s\n", error ? error->message : "Unknown");
    g_error_free (error);
    return FALSE;
  }

  return TRUE;
}

gboolean
cm_network_is_same (const CmNetwork *network, const gchar *path)
{
  CmNetworkPrivate *priv = network->priv;
  return !strcmp (priv->path, path);
}

gulong
cm_network_get_timestamp (const CmNetwork *network)
{
  CmNetworkPrivate *priv = network->priv;
  return priv->last_update;
}

gboolean
cm_network_has_passphrase (const CmNetwork *network)
{
  CmNetworkPrivate *priv = network->priv;
  if (!(priv->flags & NETWORK_INFO_PASSPHRASE))
    return FALSE;
  return TRUE;
}

gboolean
cm_network_is_secure (const CmNetwork *network)
{
  CmNetworkPrivate *priv = network->priv;
  if (!(priv->flags & NETWORK_INFO_SECURITY))
    return FALSE;
  return strcmp ("none", priv->security) ? 1 : 0;
}

guchar
cm_network_get_strength (const CmNetwork *network)
{
  CmNetworkPrivate *priv = network->priv;
  return priv->strength;
}

guchar
cm_network_get_priority (const CmNetwork *network)
{
  CmNetworkPrivate *priv = network->priv;
  return priv->priority;
}

guint
cm_network_get_channel (CmNetwork *network)
{
  CmNetworkPrivate *priv = network->priv;
  return priv->channel;
}

guint
cm_network_get_frequency (CmNetwork *network)
{
  CmNetworkPrivate *priv = network->priv;
  return priv->frequency;
}

static void
network_set_property_call_notify (DBusGProxy *proxy,
				  DBusGProxyCall *call,
				  gpointer data)
{
  CmNetwork *network = data;
  CmNetworkPrivate *priv = network->priv;
  GError *error = NULL;

  if (!dbus_g_proxy_end_call (proxy, call, &error, G_TYPE_INVALID))
  {
    g_debug ("Error calling dbus_g_proxy_end_call in %s on %s: %s\n",
             __FUNCTION__, cm_network_get_name (network), error->message);
    g_error_free (error);
  }
  else
  {
    network_update_property (priv->pending_property_name,
                             &priv->pending_property_value,
                             network);
    network_emit_updated (network);
  }

  g_free (priv->pending_property_name);
  g_value_unset (&priv->pending_property_value);
}

gboolean
cm_network_set_property (CmNetwork *network, const gchar *property,
                         GValue *value)
{
  CmNetworkPrivate *priv = network->priv;
  GError *error = NULL;
  DBusGProxyCall *call;

  g_value_init (&priv->pending_property_value, G_VALUE_TYPE (value));
  g_value_copy (value, &priv->pending_property_value);

  call = dbus_g_proxy_begin_call (priv->proxy, "SetProperty",
                                  network_set_property_call_notify, network,
                                  NULL, G_TYPE_STRING, property,
                                  G_TYPE_VALUE, value, G_TYPE_INVALID);

  if (!call)
  {
    g_debug ("SetProperty failed: %s\n", error ? error->message : "Unknown");
    g_error_free (error);
    return FALSE;
  }

  return TRUE;
}

gboolean
cm_network_set_passphrase (CmNetwork *network, const gchar *passphrase)
{
  GValue value = { 0 };
  gboolean ret;
  g_value_init (&value, G_TYPE_STRING);
  g_value_set_static_string (&value, passphrase);
  ret = cm_network_set_property (network, "WiFi.Passphrase", &value);
  g_value_unset (&value);
  return ret;
}

gint
cm_network_get_passphrase_length (const CmNetwork *network)
{
  CmNetworkPrivate *priv = network->priv;
  if (!cm_network_has_passphrase (network))
      return -1;
  return strlen (priv->passphrase);
}

CmDevice *
cm_network_get_device (CmNetwork *network)
{
  CmNetworkPrivate *priv = network->priv;
  return priv->device;
}

gchar *
cm_network_get_mode (CmNetwork *network)
{
  CmNetworkPrivate *priv = network->priv;
  return priv->mode;
}

gchar *
cm_network_get_address (CmNetwork *network)
{
  CmNetworkPrivate *priv = network->priv;
  return priv->mode;
}


/*****************************************************************************
 *
 *
 * GObject class_init, init, and finalize methods
 *
 *
 *****************************************************************************/

static void
network_dispose (GObject *object)
{
  CmNetwork *network = CM_NETWORK (object);
  CmNetworkPrivate *priv = network->priv;

  if (priv->proxy)
  {
    dbus_g_proxy_disconnect_signal (
      priv->proxy, "PropertyChanged",
      G_CALLBACK (network_property_change_handler_proxy),
      network);

    priv->proxy = NULL;
  }
  if (priv->pending_property_name)
  {
    g_free (priv->pending_property_name);
    g_value_unset (&priv->pending_property_value);
  }

  priv->manager = NULL;

  G_OBJECT_CLASS (network_parent_class)->dispose (object);
}

static void
network_finalize (GObject *object)
{
  CmNetwork *network = CM_NETWORK (object);
  CmNetworkPrivate *priv = network->priv;

  g_free (priv->name);
  g_free (priv->ssid);
  g_free (priv->ssid_printable);
  g_free (priv->path);
  g_free (priv->security);
  g_free (priv->passphrase);
  g_free (priv->address);
  g_free (priv->mode);

  G_OBJECT_CLASS (network_parent_class)->finalize (object);
}

static void
network_init (CmNetwork *self)
{
  self->priv = CM_NETWORK_GET_PRIVATE (self);
  self->priv->name = NULL;
  self->priv->ssid = NULL;
  self->priv->ssid_printable = NULL;
  self->priv->path = NULL;
  self->priv->security = NULL;
  self->priv->passphrase = NULL;
  self->priv->address = NULL;
  self->priv->address = NULL;
  self->priv->channel = 0;
  self->priv->frequency = 0;
  self->priv->manager = NULL;
}

static void
network_class_init (CmNetworkClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = network_dispose;
  gobject_class->finalize = network_finalize;

  network_signals[SIGNAL_UPDATE] = g_signal_new (
    "network-updated",
    G_TYPE_FROM_CLASS (gobject_class),
    G_SIGNAL_RUN_LAST,
    0,
    NULL, NULL,
    g_cclosure_marshal_VOID__VOID,
    G_TYPE_NONE, 0);

  g_type_class_add_private (gobject_class, sizeof (CmNetworkPrivate));
}

