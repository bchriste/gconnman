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
 * ConnMan provides Device information across DBus with the following
 * returned via GetProperties:
 *
 * Name          - user readable ASCIIZ text string
 * Scanning      - gboolean whether this device is in an active scan
 *
 * ... todo ... fill out rest of table...
 *
 * This file implements base methods for parsing the DBus data
 * received for a Network, allocating a Network structure and providing
 * hooks to be signalled on Network changes property changes.
 *
 * The Network objects are typically contained within a Device object.
 */
#include <string.h>

#include "debug.h"
#include "gconnman-internal.h"

G_DEFINE_TYPE (CmDevice, device, G_TYPE_OBJECT);
#define DEVICE_ERROR device_error_quark ()

#define CM_DEVICE_GET_PRIVATE(obj)                         \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj),                  \
                                CM_TYPE_DEVICE,            \
                                CmDevicePrivate))

struct _CmDevicePrivate
{
  gchar *path;
  CmDeviceType type;
  DBusGProxy *proxy;
  GList *networks;
  gboolean scanning;
  gboolean signals_added;
  gulong last_stamp;
  gchar *name;
  gchar *iface;

  guchar priority;
  gchar *policy;
  gboolean powered;
  gchar *ipv4_method;
  gchar *address;
  guint scan_interval;

  DBusGProxyCall *get_properties_proxy_call;
  DBusGProxyCall *propose_scan_proxy_call;
  DBusGProxyCall *join_network_proxy_call;
  DBusGProxyCall *set_property_proxy_call;

  GValue pending_property_value;
  gchar *pending_property_name;
};

static void device_property_change_handler_proxy (DBusGProxy *, const gchar *,
						  GValue *, gpointer);
enum
{
  SIGNAL_UPDATE,
  SIGNAL_LAST
};

static gint device_signals[SIGNAL_LAST];

static GQuark
device_error_quark (void)
{
  return g_quark_from_static_string ("device-error-quark");
}

static void
device_emit_updated (CmDevice *device)
{
  g_signal_emit (device, device_signals[SIGNAL_UPDATE], 0 /* detail */);
}

static void
device_proxy_call_destroy (CmDevice *device, DBusGProxyCall **proxy_call)
{
  CmDevicePrivate *priv = device->priv;
  if (*proxy_call == NULL)
    return;
  dbus_g_proxy_cancel_call (priv->proxy, *proxy_call);
  *proxy_call = NULL;
}


static CmNetwork *
device_find_network (CmDevice *device, const gchar *path)
{
  CmDevicePrivate *priv = device->priv;
  GList *tmp = priv->networks;
  while (tmp)
  {
    CmNetwork *network = tmp->data;
    if (cm_network_is_same (network, path))
      return network;
    tmp = tmp->next;
  }
  return NULL;
}

static void
device_update_property (const gchar *key, GValue *value, CmDevice *device)
{
  CmDevicePrivate *priv = device->priv;
  gchar *tmp;

  if (!strcmp ("Networks", key))
  {
    GPtrArray *networks = g_value_get_boxed (value);
    gint i;
    const gchar *path;

    for (i = 0; i < networks->len; i++)
    {
      path = g_ptr_array_index (networks, i);
      CmNetwork *network = device_find_network (device, path);
      if (!network)
      {
	GError *error = NULL;
	g_print ("New network found: %s\n", path);
	network = internal_network_new (priv->proxy, device, path, &error);
	if (!network)
	{
	  g_print ("network_new failed in %s: %s\n", __FUNCTION__,
		   error->message);
          g_clear_error (&error);
	  continue;
	}
	priv->networks = g_list_append (priv->networks, network);
      }
    }
    return;
  }

  if (!strcmp ("Scanning", key))
  {
    priv->scanning = g_value_get_boolean (value);
    return;
  }

  if (!strcmp ("Name", key))
  {
    g_free (priv->name);
    priv->name = g_strdup (g_value_get_string (value));
    return;
  }

  if (!strcmp ("Interface", key))
  {
    g_free (priv->iface);
    priv->iface = g_strdup (g_value_get_string (value));
    return;
  }

  if (!strcmp ("Type", key))
  {
    const gchar *type;
    type = g_value_get_string (value);
    if (!strcmp (type, "wifi"))
      priv->type = DEVICE_WIFI;
    else if (!strcmp (type, "wimax"))
      priv->type = DEVICE_WIMAX;
    else if (!strcmp (type, "bluetooth"))
      priv->type = DEVICE_BLUETOOTH;
    else if (!strcmp (type, "cellular"))
      priv->type = DEVICE_CELLULAR;
    else if (!strcmp (type, "ethernet"))
      priv->type = DEVICE_ETHERNET;
    else
    {
      g_print ("Unknown device type on %s: %s\n",
               cm_device_get_name (device), type);
      priv->type = DEVICE_UNKNOWN;
    }
    return;
  }

  if (!strcmp ("Priority", key))
  {
    priv->priority = g_value_get_uchar (value);
    return;
  }

  if (!strcmp ("Policy", key))
  {
    g_free (priv->policy);
    priv->policy = g_strdup (g_value_get_string (value));
    return;
  }

  if (!strcmp ("Powered", key))
  {
    priv->powered = g_value_get_boolean (value);
    return;
  }

  if (!strcmp ("IPv4.Method", key))
  {
    g_free (priv->ipv4_method);
    priv->ipv4_method = g_strdup (g_value_get_string (value));
    return;
  }

  if (!strcmp ("ScanInterval", key))
  {
    priv->scan_interval = g_value_get_uint (value);
    return;
  }

  if (!strcmp ("Address", key))
  {
    g_free (priv->address);
    priv->address = g_strdup (g_value_get_string (value));
    return;
  }

  tmp = g_strdup_value_contents (value);
  g_print ("Unhandled property on %s: %s = %s\n",
           cm_device_get_name (device), key, tmp);
  g_free (tmp);
}


static void
device_property_change_handler_proxy (DBusGProxy *proxy,
				      const gchar *key,
				      GValue *value,
				      gpointer data)
{
  CmDevice *device = data;
  gchar *tmp = g_strdup_value_contents (value);
  g_print ("PropertyChange on %s: %s = %s\n",
           cm_device_get_name (device), key, tmp);
  g_free (tmp);

  device_update_property (key, value, device);

  device_emit_updated (device);
}

static void
device_get_properties_call_notify (DBusGProxy *proxy,
				   DBusGProxyCall *call,
				   gpointer data)
{
  CmDevice *device = data;
  CmDevicePrivate *priv = device->priv;
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

  g_hash_table_foreach (properties, (GHFunc)device_update_property, device);
  g_hash_table_unref (properties);
  device_emit_updated (device);

  priv->get_properties_proxy_call = NULL;
}

CmDevice *
internal_device_new (DBusGProxy *proxy, const gchar *path, GError **error)
{
  CmDevice *device;
  CmDevicePrivate *priv;

  device = g_object_new (CM_TYPE_DEVICE, NULL);
  if (!device)
  {
    g_set_error (error, DEVICE_ERROR, DEVICE_ERROR_NO_MEMORY,
                 "Unable to allocate CmDevice.");
    return NULL;
  }

  priv = device->priv;
  priv->type = DEVICE_UNKNOWN;

  priv->path = g_strdup (path);
  if (!priv->path)
  {
    g_set_error (error, DEVICE_ERROR, DEVICE_ERROR_NO_MEMORY,
                 "Unable to allocate device path.");
    g_object_unref (device);
    return NULL;
  }

  priv->proxy = dbus_g_proxy_new_from_proxy (
    proxy, CONNMAN_DEVICE_INTERFACE, path);
  if (!priv->proxy)
  {
    g_set_error (error, DEVICE_ERROR, DEVICE_ERROR_CONNMAN_INTERFACE,
                 "No interface for %s/%s from Connman.",
                 CONNMAN_DEVICE_INTERFACE, path);
    g_object_unref (device);
    return NULL;
  }

  dbus_g_proxy_add_signal (
    priv->proxy, "PropertyChanged",
    G_TYPE_STRING, G_TYPE_VALUE, G_TYPE_INVALID);

  dbus_g_proxy_connect_signal (
    priv->proxy, "PropertyChanged",
    G_CALLBACK (device_property_change_handler_proxy),
    device, NULL);

  priv->get_properties_proxy_call = dbus_g_proxy_begin_call (
    priv->proxy, "GetProperties",
    device_get_properties_call_notify, device, NULL,
    G_TYPE_INVALID);
  if (!priv->get_properties_proxy_call)
  {
    g_set_error (error, DEVICE_ERROR, DEVICE_ERROR_CONNMAN_GET_PROPERTIES,
                 "Invocation of GetProperties failed.");
    g_object_unref (device);
    return NULL;
  }

  return device;
}

GList *
cm_device_get_networks (CmDevice *device)
{
  CmDevicePrivate *priv= device->priv;
  return g_list_copy (priv->networks);
}

const gchar *
cm_device_get_path (CmDevice *device)
{
  CmDevicePrivate *priv= device->priv;
  return priv->path;
}

void
cm_device_print (const CmDevice *device)
{
  CmDevicePrivate *priv= device->priv;
  g_print ("%s: Scanning = %s, Networks = %d\n",
	   cm_device_get_name (device), priv->scanning ? "TRUE" : "FALSE",
	   g_list_length (priv->networks));
}

const gchar *
cm_device_get_name (const CmDevice *device)
{
  CmDevicePrivate *priv= device->priv;
  return priv->name ? priv->name :
    (priv->iface ? priv->iface :priv->path);
}

gboolean
cm_device_is_scanning (const CmDevice *device)
{
  CmDevicePrivate *priv= device->priv;
  return priv->scanning;
}

static void
device_propose_scan_call_notify (DBusGProxy *proxy,
                                 DBusGProxyCall *call,
                                 gpointer data)
{
  CmDevice *device = data;
  CmDevicePrivate *priv= device->priv;
  GError *error = NULL;

  if (priv->propose_scan_proxy_call != call)
    g_print ("%s Call mismatch!\n", __FUNCTION__);

  priv->propose_scan_proxy_call = NULL;

  if (!dbus_g_proxy_end_call (proxy, call, &error, G_TYPE_INVALID))
  {
    g_print ("Error calling dbus_g_proxy_end_call in %s: %s\n",
             __FUNCTION__, error->message);
    g_clear_error (&error);
  }

  ASYNC_DEBUG ("Device::ProposeScan invocation complete.\n");
}

gboolean
cm_device_scan (CmDevice *device)
{
  CmDevicePrivate *priv= device->priv;
  switch (priv->type)
  {
  case DEVICE_WIFI:
  case DEVICE_WIMAX:
  case DEVICE_BLUETOOTH:
  case DEVICE_CELLULAR:
    break;

  case DEVICE_UNKNOWN:
  case DEVICE_ETHERNET:
    g_print ("Not scanning on %s\n", cm_device_get_name (device));
    return FALSE;
  }

  if (priv->propose_scan_proxy_call)
  {
    g_print ("Not scanning on %s - pending call.\n", cm_device_get_name (device));
    return FALSE;
  }

  ASYNC_DEBUG ("Device::ProposeScan invocation starting.\n");

  priv->propose_scan_proxy_call = dbus_g_proxy_begin_call (
    priv->proxy, "ProposeScan",
    device_propose_scan_call_notify, device, NULL,
    G_TYPE_INVALID);
  if (!priv->propose_scan_proxy_call)
  {
    g_print ("Net scanning on %s - ProposeScan failed.\n",
	     cm_device_get_name (device));
    return FALSE;
  }

  return TRUE;
}

static void
device_set_property_call_notify (DBusGProxy *proxy,
                                 DBusGProxyCall *call,
                                 gpointer data)
{
  CmDevice *device = data;
  CmDevicePrivate *priv = device->priv;
  GError *error = NULL;

  if (priv->set_property_proxy_call != call)
    g_print ("%s Call mismatch!\n", __FUNCTION__);

  priv->set_property_proxy_call = NULL;

  if (!dbus_g_proxy_end_call (proxy, call, &error, G_TYPE_INVALID))
  {
    g_print ("Error calling dbus_g_proxy_end_call in %s on %s: %s\n",
             __FUNCTION__, cm_device_get_name (device), error->message);
    g_clear_error (&error);
  }
  else
  {
    device_update_property (priv->pending_property_name,
                            &priv->pending_property_value,
                            device);
    device_emit_updated (device);
  }

  g_free (priv->pending_property_name);
  g_value_unset (&priv->pending_property_value);
  priv->pending_property_name = NULL;

  g_print ("%s:%s\n", __FUNCTION__, cm_device_get_name (device));
}

gboolean
device_set_property (CmDevice *device, const gchar *property, GValue *value)
{
  CmDevicePrivate *priv = device->priv;
  GError *error = NULL;

  g_print ("%s:%s\n", __FUNCTION__, cm_device_get_name (device));

  if (priv->set_property_proxy_call)
    return FALSE;

  priv->pending_property_name = g_strdup (property);
  g_value_init (&priv->pending_property_value, G_VALUE_TYPE (value));
  g_value_copy (value, &priv->pending_property_value);

  priv->set_property_proxy_call = dbus_g_proxy_begin_call (
    priv->proxy, "SetProperty",
    device_set_property_call_notify, device, NULL,
    G_TYPE_STRING, property,
    G_TYPE_VALUE, value,
    G_TYPE_INVALID);

  if (!priv->set_property_proxy_call)
  {
    g_print ("SetProperty failed: %s\n", error ? error->message : "Unknown");
    g_clear_error (&error);
    return FALSE;
  }

  return TRUE;
}

gboolean
cm_device_is_same (const CmDevice *device, const gchar *path)
{
  CmDevicePrivate *priv= device->priv;
  return !strcmp (priv->path, path);
}


CmDeviceType
cm_device_get_type (const CmDevice *device)
{
  CmDevicePrivate *priv = device->priv;
  return priv->type;
}

gboolean
cm_device_set_powered (CmDevice *device, gboolean powered)
{
  GValue value = { 0 };
  gboolean ret;
  g_value_init (&value, G_TYPE_BOOLEAN);
  g_value_set_boolean (&value, powered);
  ret = device_set_property (device, "Powered", &value);
  g_value_unset (&value);
  return ret;
}

gchar *
cm_device_get_address (CmDevice *device)
{
  CmDevicePrivate *priv = device->priv;
  return priv->address;
}

guint
cm_device_get_scan_interval (CmDevice *device)
{
  CmDevicePrivate *priv = device->priv;
  return priv->scan_interval;
}

gboolean
cm_device_set_scan_interval (CmDevice *device, guint interval)
{
  GValue value = { 0 };
  gboolean ret;

  g_value_init (&value, G_TYPE_UINT);
  g_value_set_uint (&value, interval);
  ret = device_set_property (device, "ScanInterval", &value);
  g_value_unset (&value);

  return ret;
}

gboolean
cm_device_get_powered (const CmDevice *device)
{
  CmDevicePrivate *priv = device->priv;
  return priv->powered;
}

static void
device_join_network_call_notify (DBusGProxy *proxy, DBusGProxyCall *call,
                                 gpointer user_data)
{
  CmDevice *device = user_data;
  CmDevicePrivate *priv = device->priv;
  GError *error = NULL;

  if (priv->join_network_proxy_call != call)
    g_print ("%s call mismatch!\n", __FUNCTION__);

  priv->join_network_proxy_call = NULL;

  if (!dbus_g_proxy_end_call (proxy, call, &error, G_TYPE_INVALID))
  {
    g_print ("Error calling dbus_g_proxy_end_call in %s: %s\n",
             __FUNCTION__, error->message);
    g_clear_error (&error);
  }

  ASYNC_DEBUG ("Device::JoinNetwork invocation complete.\n");
}

static void
_free_g_value (GValue *value)
{
  g_value_unset (value);
}

gboolean
cm_device_join_network (CmDevice *device, const gchar *ssid,
                        const gchar *security, const gchar *passphrase)
{
  CmDevicePrivate *priv = device->priv;
  GHashTable *net_props;
  GValue *mode_val, *network_val, *security_val, *secret_val;
  gchar *proxy;

  if (priv->join_network_proxy_call)
  {
    g_print ("Not joining network on %s - pending call\n",
             cm_device_get_name (device));
    return FALSE;
  }

  /* Populate a hashtable/dictionary of network property key-value pairs */
  net_props = g_hash_table_new_full (g_str_hash,
                                     g_str_equal,
                                     g_free,
                                     (GDestroyNotify) _free_g_value);

  mode_val = g_new0 (GValue, 1);
  g_value_init (mode_val, G_TYPE_STRING);
  g_value_set_string (mode_val, g_strdup ("managed"));
  g_hash_table_insert (net_props, g_strdup ("WiFi.Mode"), mode_val);

  network_val = g_new0 (GValue, 1);
  g_value_init (network_val, G_TYPE_STRING);
  g_value_set_string (network_val, ssid);
  g_hash_table_insert (net_props, g_strdup ("WiFi.SSID"), network_val);

  /* Security and passphrase are optional */
  if (security)
  {
    security_val = g_new0 (GValue, 1);
    g_value_init (security_val, G_TYPE_STRING);
    g_value_set_string (security_val, security);
    g_hash_table_insert (net_props, g_strdup ("WiFi.Security"), security_val);
  }

  if (passphrase)
  {
    secret_val = g_new0 (GValue, 1);
    g_value_init (secret_val, G_TYPE_STRING);
    g_value_set_string (secret_val, passphrase);
    g_hash_table_insert (net_props, g_strdup ("WiFi.Passphrase"), secret_val);
  }

  ASYNC_DEBUG ("Device::JoinNetwork invocation starting.\n");

  priv->join_network_proxy_call = dbus_g_proxy_begin_call
    (priv->proxy, "JoinNetwork", device_join_network_call_notify, device, NULL,
     dbus_g_type_get_map ("GHashTable", G_TYPE_STRING, G_TYPE_VALUE),
     net_props, G_TYPE_INVALID);

  if (!priv->join_network_proxy_call)
  {
    g_print ("Joining network on %s failed.\n",
             cm_device_get_name (device));
    return FALSE;
  }

  return TRUE;
}

/*****************************************************************************
 *
 *
 * GObject class_init, init, and finalize methods
 *
 *
 *****************************************************************************/

static void
device_finalize (GObject *object)
{
  CmDevice *device = CM_DEVICE (object);
  CmDevicePrivate *priv = device->priv;

  dbus_g_proxy_disconnect_signal (
    priv->proxy, "PropertyChanged",
    G_CALLBACK (device_property_change_handler_proxy),
    device);

  device_proxy_call_destroy (device, &priv->get_properties_proxy_call);
  device_proxy_call_destroy (device, &priv->propose_scan_proxy_call);
  device_proxy_call_destroy (device, &priv->join_network_proxy_call);
  device_proxy_call_destroy (device, &priv->set_property_proxy_call);

  while (priv->networks)
  {
    g_object_unref (priv->networks->data);
    priv->networks = g_list_delete_link (priv->networks, priv->networks);
  }

  g_free (priv->path);
  g_free (priv->policy);
  g_free (priv->ipv4_method);
  g_free (priv->address);
  g_free (priv->iface);
  g_free (priv->name);

  if (priv->proxy)
    g_object_unref (priv->proxy);

  G_OBJECT_CLASS (device_parent_class)->finalize (object);
}

static void
device_init (CmDevice *self)
{
  self->priv = CM_DEVICE_GET_PRIVATE (self);
  self->priv->path = NULL;
  self->priv->policy = NULL;
  self->priv->ipv4_method = NULL;
  self->priv->address = NULL;
  self->priv->iface = NULL;
  self->priv->name = NULL;
  self->priv->scan_interval = 0;
}

static void
device_class_init (CmDeviceClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = device_finalize;

  device_signals[SIGNAL_UPDATE] = g_signal_new (
    "device-updated",
    G_TYPE_FROM_CLASS (gobject_class),
    G_SIGNAL_RUN_LAST,
    0,
    NULL, NULL,
    g_cclosure_marshal_VOID__VOID,
    G_TYPE_NONE, 0);

  g_type_class_add_private (gobject_class, sizeof (CmDevicePrivate));
}

const gchar *
cm_device_type_to_string (CmDeviceType type)
{
  switch (type)
  {
  case DEVICE_WIFI:
    return "Wireless";

  case DEVICE_WIMAX:
    return "WiMAX";

  case DEVICE_BLUETOOTH:
    return "Bluetooth";

  case DEVICE_CELLULAR:
    return "Cellular";

  case DEVICE_ETHERNET:
    return "Ethernet";

  case DEVICE_UNKNOWN:
  default:
    break;
  }

  return "Unknown";
}
