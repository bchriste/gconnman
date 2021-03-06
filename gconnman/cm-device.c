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

#include "gconnman-internal.h"

G_DEFINE_TYPE (CmDevice, device, G_TYPE_OBJECT);
#define DEVICE_ERROR device_error_quark ()

#define CM_DEVICE_GET_PRIVATE(obj)                         \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj),                  \
                                CM_TYPE_DEVICE,            \
                                CmDevicePrivate))

struct _CmDevicePrivate
{
  CmManager *manager;
  gchar *path;
  CmDeviceType type;
  DBusGProxy *proxy;
  GList *networks;
  gboolean scanning;
  gboolean signals_added;
  gulong last_stamp;
  gchar *name;
  gchar *iface;

  gboolean powered;
  gchar *ipv4_method;
  gchar *address;
  guint scan_interval;
};

static void device_property_change_handler_proxy (DBusGProxy *, const gchar *,
						  GValue *, gpointer);
enum
{
  SIGNAL_UPDATE,
  SIGNAL_ADDRESS_CHANGED,
  SIGNAL_NAME_CHANGED,
  SIGNAL_TYPE_CHANGED,
  SIGNAL_INTERFACE_CHANGED,
  SIGNAL_POWERED_CHANGED,
  SIGNAL_SCAN_INTERVAL_CHANGED,
  SIGNAL_SCANNING_CHANGED,
  SIGNAL_NETWORKS_CHANGED,
  SIGNAL_METHOD_CHANGED,
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

CmNetwork *
cm_device_find_network (CmDevice *device, const gchar *opath)
{
  CmDevicePrivate *priv = device->priv;
  CmNetwork *network = NULL;
  GList *iter;
  const gchar *npath;

  for (iter = priv->networks; iter != NULL; iter = iter->next)
  {
    network = iter->data;
    npath = cm_network_get_path (network);

    if (g_strcmp0 (opath, npath) == 0)
      return network;
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
    const gchar *path = NULL;
    GList *iter;

    /* First remove stale networks */
    for (iter = priv->networks; iter != NULL; iter = iter->next)
    {
      CmNetwork *net = iter->data;
      gboolean found = FALSE;

      for (i = 0; i < networks->len && !found; i++)
      {
        path = g_ptr_array_index (networks, i);

        if (g_strcmp0 (path, cm_network_get_path (net)) == 0)
        {
          found = TRUE;
        }
      }

      /* If net not in networks (found = FALSE) delete form priv->networks */
      if (!found)
      {
        priv->networks = g_list_delete_link (priv->networks, iter);
      }
    }

    /* iterate networks and add new networks to priv->networks*/
    for (i = 0; i < networks->len; i++)
    {
      path = g_ptr_array_index (networks, i);
      CmNetwork *network;
      GError *error = NULL;

      network = cm_device_find_network (device, path);
      if (!network)
      {
        network = internal_network_new (priv->proxy, device, path,
                                        priv->manager, &error);
        if (!network)
        {
          g_debug ("network_new failed in %s: %s", __FUNCTION__,
                   error->message);
          g_error_free (error);
          continue;
        }
        else
        {
          priv->networks = g_list_append (priv->networks, network);
        }
      }
    }

    g_signal_emit (device, device_signals[SIGNAL_NETWORKS_CHANGED], 0);
  }
  else if (!strcmp ("Scanning", key))
  {
    priv->scanning = g_value_get_boolean (value);
    g_signal_emit (device, device_signals[SIGNAL_SCANNING_CHANGED], 0);
  }
  else if (!strcmp ("Name", key))
  {
    g_free (priv->name);
    priv->name = g_value_dup_string (value);
    g_signal_emit (device, device_signals[SIGNAL_NAME_CHANGED], 0);
  }
  else if (!strcmp ("Interface", key))
  {
    g_free (priv->iface);
    priv->iface = g_value_dup_string (value);
    g_signal_emit (device, device_signals[SIGNAL_INTERFACE_CHANGED], 0);
  }
  else if (!strcmp ("Type", key))
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
      g_debug ("Unknown device type on %s: %s\n",
               cm_device_get_name (device), type);
      priv->type = DEVICE_UNKNOWN;
    }
    g_signal_emit (device, device_signals[SIGNAL_TYPE_CHANGED], 0);
  }
  else if (!strcmp ("Powered", key))
  {
    priv->powered = g_value_get_boolean (value);
    g_signal_emit (device, device_signals[SIGNAL_POWERED_CHANGED], 0);
  }
  else if (!strcmp ("IPv4.Method", key))
  {
    g_free (priv->ipv4_method);
    priv->ipv4_method = g_value_dup_string (value);
    g_signal_emit (device, device_signals[SIGNAL_METHOD_CHANGED], 0);
  }
  else if (!strcmp ("ScanInterval", key))
  {
    priv->scan_interval = g_value_get_uint (value);
    g_signal_emit (device, device_signals[SIGNAL_SCAN_INTERVAL_CHANGED], 0);
  }
  else if (!strcmp ("Address", key))
  {
    g_free (priv->address);
    priv->address = g_value_dup_string (value);
    g_signal_emit (device, device_signals[SIGNAL_ADDRESS_CHANGED], 0);
  }
  else
  {
    tmp = g_strdup_value_contents (value);
    g_debug ("Unhandled Device property on %s: %s = %s\n",
             cm_device_get_name (device), key, tmp);
    g_free (tmp);
  }
}


static void
device_property_change_handler_proxy (DBusGProxy *proxy,
				      const gchar *key,
				      GValue *value,
				      gpointer data)
{
  CmDevice *device = data;

  device_update_property (key, value, device);

  device_emit_updated (device);
}

static void
device_get_properties_call_notify (DBusGProxy *proxy,
				   DBusGProxyCall *call,
				   gpointer data)
{
  CmDevice *device = data;
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
    g_clear_error (&error);
    return;
  }

  g_hash_table_foreach (properties, (GHFunc)device_update_property, device);
  g_hash_table_unref (properties);
  device_emit_updated (device);
}

CmDevice *
internal_device_new (DBusGProxy *proxy, const gchar *path, CmManager *manager,
                     GError **error)
{
  CmDevice *device;
  CmDevicePrivate *priv;
  DBusGProxyCall *call;

  device = g_object_new (CM_TYPE_DEVICE, NULL);
  if (!device)
  {
    g_set_error (error, DEVICE_ERROR, DEVICE_ERROR_NO_MEMORY,
                 "Unable to allocate CmDevice.");
    return NULL;
  }

  priv = device->priv;
  priv->manager = manager;
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

  call = dbus_g_proxy_begin_call (priv->proxy, "GetProperties",
                                  device_get_properties_call_notify, device,
                                  NULL, G_TYPE_INVALID);
  if (!call)
  {
    g_set_error (error, DEVICE_ERROR, DEVICE_ERROR_CONNMAN_GET_PROPERTIES,
                 "Invocation of GetProperties failed.");
    g_object_unref (device);
    return NULL;
  }

  return device;
}

const GList *
cm_device_get_networks (CmDevice *device)
{
  CmDevicePrivate *priv= device->priv;
  return priv->networks;
}

const gchar *
cm_device_get_path (CmDevice *device)
{
  CmDevicePrivate *priv= device->priv;
  return priv->path;
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
  GError *error = NULL;

  if (!dbus_g_proxy_end_call (proxy, call, &error, G_TYPE_INVALID))
  {
    g_debug ("Error calling dbus_g_proxy_end_call in %s: %s\n",
             __FUNCTION__, error->message);
    g_error_free (error);
  }
}

gboolean
cm_device_scan (CmDevice *device)
{
  CmDevicePrivate *priv= device->priv;
  DBusGProxyCall *call;

  switch (priv->type)
  {
  case DEVICE_WIFI:
  case DEVICE_WIMAX:
  case DEVICE_BLUETOOTH:
  case DEVICE_CELLULAR:
    break;

  case DEVICE_UNKNOWN:
  case DEVICE_ETHERNET:
    return FALSE;
  }

  call = dbus_g_proxy_begin_call (priv->proxy, "ProposeScan",
                                  device_propose_scan_call_notify, device,
                                  NULL, G_TYPE_INVALID);
  if (!call)
  {
    g_debug ("Net scanning on %s - ProposeScan failed.\n",
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
  GError *error = NULL;

  if (!dbus_g_proxy_end_call (proxy, call, &error, G_TYPE_INVALID))
  {
    g_debug ("Error calling dbus_g_proxy_end_call in %s on %s: %s\n",
             __FUNCTION__, cm_device_get_name (device), error->message);
    g_error_free (error);
  }
}

gboolean
device_set_property (CmDevice *device, const gchar *property, GValue *value)
{
  CmDevicePrivate *priv = device->priv;
  GError *error = NULL;
  DBusGProxyCall *call;

  call = dbus_g_proxy_begin_call (priv->proxy, "SetProperty",
                                  device_set_property_call_notify, device,
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

/*****************************************************************************
 *
 *
 * GObject class_init, init, and finalize methods
 *
 *
 *****************************************************************************/

static void
device_dispose (GObject *object)
{
  CmDevice *device = CM_DEVICE (object);
  CmDevicePrivate *priv = device->priv;

  while (priv->networks)
  {
    g_object_unref (priv->networks->data);
    priv->networks = g_list_delete_link (priv->networks, priv->networks);
  }

  if (priv->proxy)
  {
    dbus_g_proxy_disconnect_signal (
      priv->proxy, "PropertyChanged",
      G_CALLBACK (device_property_change_handler_proxy),
      device);

    g_object_unref (priv->proxy);
    priv->proxy = NULL;
  }

  priv->manager = NULL;

  G_OBJECT_CLASS (device_parent_class)->dispose (object);
}

static void
device_finalize (GObject *object)
{
  CmDevice *device = CM_DEVICE (object);
  CmDevicePrivate *priv = device->priv;

  g_free (priv->path);
  g_free (priv->ipv4_method);
  g_free (priv->address);
  g_free (priv->iface);
  g_free (priv->name);

  G_OBJECT_CLASS (device_parent_class)->finalize (object);
}

static void
device_init (CmDevice *self)
{
  self->priv = CM_DEVICE_GET_PRIVATE (self);
  self->priv->path = NULL;
  self->priv->ipv4_method = NULL;
  self->priv->address = NULL;
  self->priv->iface = NULL;
  self->priv->name = NULL;
  self->priv->scan_interval = 0;
  self->priv->type = DEVICE_UNKNOWN;
  self->priv->networks = NULL;
}

static void
device_class_init (CmDeviceClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = device_finalize;
  gobject_class->dispose = device_dispose;

  device_signals[SIGNAL_UPDATE] = g_signal_new (
    "device-updated",
    G_TYPE_FROM_CLASS (gobject_class),
    G_SIGNAL_RUN_LAST,
    0,
    NULL, NULL,
    g_cclosure_marshal_VOID__VOID,
    G_TYPE_NONE, 0);
  device_signals[SIGNAL_ADDRESS_CHANGED] = g_signal_new (
    "address-changed",
    G_TYPE_FROM_CLASS (gobject_class),
    G_SIGNAL_RUN_LAST,
    0,
    NULL, NULL,
    g_cclosure_marshal_VOID__VOID,
    G_TYPE_NONE, 0);
  device_signals[SIGNAL_NAME_CHANGED] = g_signal_new (
    "name-changed",
    G_TYPE_FROM_CLASS (gobject_class),
    G_SIGNAL_RUN_LAST,
    0,
    NULL, NULL,
    g_cclosure_marshal_VOID__VOID,
    G_TYPE_NONE, 0);
  device_signals[SIGNAL_TYPE_CHANGED] = g_signal_new (
    "type-changed",
    G_TYPE_FROM_CLASS (gobject_class),
    G_SIGNAL_RUN_LAST,
    0,
    NULL, NULL,
    g_cclosure_marshal_VOID__VOID,
    G_TYPE_NONE, 0);
  device_signals[SIGNAL_INTERFACE_CHANGED] = g_signal_new (
    "interface-changed",
    G_TYPE_FROM_CLASS (gobject_class),
    G_SIGNAL_RUN_LAST,
    0,
    NULL, NULL,
    g_cclosure_marshal_VOID__VOID,
    G_TYPE_NONE, 0);
  device_signals[SIGNAL_POWERED_CHANGED] = g_signal_new (
    "powered-changed",
    G_TYPE_FROM_CLASS (gobject_class),
    G_SIGNAL_RUN_LAST,
    0,
    NULL, NULL,
    g_cclosure_marshal_VOID__VOID,
    G_TYPE_NONE, 0);
  device_signals[SIGNAL_SCAN_INTERVAL_CHANGED] = g_signal_new (
    "scan-interval-changed",
    G_TYPE_FROM_CLASS (gobject_class),
    G_SIGNAL_RUN_LAST,
    0,
    NULL, NULL,
    g_cclosure_marshal_VOID__VOID,
    G_TYPE_NONE, 0);
  device_signals[SIGNAL_SCANNING_CHANGED] = g_signal_new (
    "scanning-changed",
    G_TYPE_FROM_CLASS (gobject_class),
    G_SIGNAL_RUN_LAST,
    0,
    NULL, NULL,
    g_cclosure_marshal_VOID__VOID,
    G_TYPE_NONE, 0);
  device_signals[SIGNAL_NETWORKS_CHANGED] = g_signal_new (
    "networks-changed",
    G_TYPE_FROM_CLASS (gobject_class),
    G_SIGNAL_RUN_LAST,
    0,
    NULL, NULL,
    g_cclosure_marshal_VOID__VOID,
    G_TYPE_NONE, 0);
  device_signals[SIGNAL_METHOD_CHANGED] = g_signal_new (
    "method-changed",
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
