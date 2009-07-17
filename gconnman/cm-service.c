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

#include <string.h>
#include <stdlib.h>
#include <glib.h>

#include "gconnman-internal.h"

#define SERVICE_ERROR service_error_quark ()

G_DEFINE_TYPE (CmService, service, G_TYPE_OBJECT);

#define CM_SERVICE_GET_PRIVATE(obj)                                        \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj), CM_TYPE_SERVICE, CmServicePrivate))

struct _CmServicePrivate
{
  CmManager *manager;

  DBusGProxy *proxy;
  gchar *path;

  gchar *state;
  gchar *name;
  gchar *type;
  gchar *mode;
  gchar *security;
  gchar *passphrase;
  guint strength;
  gint order;
  gboolean favorite;
  gchar *error;
  gchar *method;

  gboolean connected;
  CmServiceInfoMask flags;

  gulong last_update;
};

enum
{
  SIGNAL_UPDATE,
  SIGNAL_STATE_CHANGED,
  SIGNAL_NAME_CHANGED,
  SIGNAL_TYPE_CHANGED,
  SIGNAL_MODE_CHANGED,
  SIGNAL_SECURITY_CHANGED,
  SIGNAL_PASSPHRASE_CHANGED,
  SIGNAL_STRENGTH_CHANGED,
  SIGNAL_FAVORITE_CHANGED,
  SIGNAL_ERROR_CHANGED,
  SIGNAL_METHOD_CHANGED,
  SIGNAL_LAST
};

static gint service_signals[SIGNAL_LAST];

static GQuark
service_error_quark (void)
{
  return g_quark_from_static_string ("service-error-quark");
}

static void
service_emit_updated (CmService *service)
{
  g_signal_emit (service, service_signals[SIGNAL_UPDATE], 0 /* detail */);
}

static void service_property_change_handler_proxy (DBusGProxy *, const gchar *,
						   GValue *, gpointer);

static void
service_update_property (const gchar *key, GValue *value, CmService *service)
{
  CmServicePrivate *priv = service->priv;
  gchar *tmp;

  if (!strcmp ("State", key))
  {
    g_free (priv->state);
    priv->state = g_value_dup_string (value);
    priv->flags |= SERVICE_INFO_STATE;
    if (!strcmp ("ready", priv->state))
    {
      priv->connected = TRUE;
    }
    else
    {
      priv->connected = FALSE;
    }
    g_signal_emit (service, service_signals[SIGNAL_STATE_CHANGED], 0,
                   priv->state);
  }
  else if (!strcmp ("Name", key))
  {
    g_free (priv->name);
    priv->name = g_value_dup_string (value);
    priv->flags |= SERVICE_INFO_NAME;
    g_signal_emit (service, service_signals[SIGNAL_NAME_CHANGED], 0,
                   priv->name);
  }
  else if (!strcmp ("Type", key))
  {
    g_free (priv->type);
    priv->type = g_value_dup_string (value);
    priv->flags |= SERVICE_INFO_TYPE;
    g_signal_emit (service, service_signals[SIGNAL_TYPE_CHANGED], 0,
                   priv->type);
  }
  else if (!strcmp ("Mode", key))
  {
    g_free (priv->mode);
    priv->mode = g_value_dup_string (value);
    priv->flags |= SERVICE_INFO_MODE;
    g_signal_emit (service, service_signals[SIGNAL_MODE_CHANGED], 0,
                   priv->mode);
  }
  else if (!strcmp ("Security", key))
  {
    g_free (priv->security);
    priv->security = g_value_dup_string (value);
    priv->flags |= SERVICE_INFO_SECURITY;
    g_signal_emit (service, service_signals[SIGNAL_SECURITY_CHANGED], 0,
                   priv->security);
  }
  else if (!strcmp ("Passphrase", key))
  {
    g_free (priv->passphrase);
    priv->passphrase = g_value_dup_string (value);
    priv->flags |= SERVICE_INFO_PASSPHRASE;
    g_signal_emit (service, service_signals[SIGNAL_PASSPHRASE_CHANGED], 0,
                   priv->passphrase);
  }
  else if (!strcmp ("Strength", key))
  {
    priv->strength = g_value_get_uchar (value);
    priv->flags |= SERVICE_INFO_STRENGTH;
    g_signal_emit (service, service_signals[SIGNAL_STRENGTH_CHANGED], 0,
                   priv->strength);
  }
  else if (!strcmp ("Favorite", key))
  {
    priv->favorite = g_value_get_boolean (value);
    priv->flags |= SERVICE_INFO_FAVORITE;
    g_signal_emit (service, service_signals[SIGNAL_FAVORITE_CHANGED], 0,
                   priv->favorite);
  }
  else if (!strcmp ("Error", key))
  {
    g_free (priv->error);
    priv->error = g_value_dup_string (value);
    priv->flags |= SERVICE_INFO_ERROR;
    g_signal_emit (service, service_signals[SIGNAL_ERROR_CHANGED], 0,
                   priv->error);
  }
  else if (!strcmp ("IPv4.Method", key))
  {
    g_free (priv->method);
    priv->method = g_value_dup_string (value);
    priv->flags |= SERVICE_INFO_METHOD;
    g_signal_emit (service, service_signals[SIGNAL_METHOD_CHANGED], 0,
                   priv->method);
  }
  else
  {
    tmp = g_strdup_value_contents (value);
    g_debug ("Unhandled Service property on %s: %s = %s\n",
             cm_service_get_name (service), key, tmp);
    g_free (tmp);
  }
}

static void
service_property_change_handler_proxy (DBusGProxy *proxy,
				       const gchar *key,
				       GValue *value,
				       gpointer data)
{
  CmService *service = data;

  service_update_property (key, value, service);

  service_emit_updated (service);
}

static void
service_get_properties_call_notify (DBusGProxy *proxy,
				   DBusGProxyCall *call,
				   gpointer data)
{
  CmService *service = data;
  GError *error = NULL;
  GHashTable *properties = NULL;
  gint count;

  if (!dbus_g_proxy_end_call (
	proxy, call, &error,
	/* OUT values */
	dbus_g_type_get_map ("GHashTable", G_TYPE_STRING, G_TYPE_VALUE),
	&properties, G_TYPE_INVALID))
  {
    g_debug ("Error calling dbus_g_proxy_end_call in %s on %s: %s\n",
             __FUNCTION__, cm_service_get_name (service), error->message);
    g_error_free (error);
    return;
  }

  count = g_hash_table_size (properties);

  g_hash_table_foreach (properties, (GHFunc)service_update_property, service);
  g_hash_table_unref (properties);
  service_emit_updated (service);
}

CmService *
internal_service_new (DBusGProxy *proxy, const gchar *path, int order,
                      CmManager *manager, GError **error)
{
  CmService *service;
  CmServicePrivate *priv;
  DBusGProxyCall *call;

  service = g_object_new (CM_TYPE_SERVICE, NULL);
  if (!service)
  {
    g_set_error (error, SERVICE_ERROR, SERVICE_ERROR_NO_MEMORY,
                 "Unable to allocate CmService.");
    return NULL;
  }

  priv = service->priv;
  priv->manager = manager;

  priv->path = g_strdup (path);
  if (!priv->path)
  {
    g_set_error (error, SERVICE_ERROR, SERVICE_ERROR_NO_MEMORY,
                 "Unable to allocate service path.");
    g_object_unref (service);
    return NULL;
  }

  priv->proxy = dbus_g_proxy_new_from_proxy (
    proxy, CONNMAN_SERVICE_INTERFACE, path);
  if (!priv->proxy)
  {
    g_set_error (error, SERVICE_ERROR, SERVICE_ERROR_CONNMAN_INTERFACE,
                 "No interface for %s/%s from Connman.",
                 CONNMAN_SERVICE_INTERFACE, path);
    g_object_unref (service);
    return NULL;
  }

  dbus_g_proxy_add_signal (
    priv->proxy, "PropertyChanged",
    G_TYPE_STRING, G_TYPE_VALUE, G_TYPE_INVALID);

  dbus_g_proxy_connect_signal (
    priv->proxy, "PropertyChanged",
    G_CALLBACK (service_property_change_handler_proxy),
    service, NULL);

  call = dbus_g_proxy_begin_call (priv->proxy, "GetProperties",
                                  service_get_properties_call_notify, service,
                                  NULL, G_TYPE_INVALID);
  if (!call)
  {
    g_set_error (error, SERVICE_ERROR, SERVICE_ERROR_CONNMAN_GET_PROPERTIES,
                 "Invocation of GetProperties failed.");
    g_object_unref (service);
    return NULL;
  }

  priv->order = order;

  return service;
}

static void
service_disconnect_call_notify (DBusGProxy *proxy,
                                DBusGProxyCall *call,
                                gpointer data)
{
  CmService *service = data;
  GError *error = NULL;

  if (!dbus_g_proxy_end_call (proxy, call, &error, G_TYPE_INVALID))
  {
    g_debug ("Error calling dbus_g_proxy_end_call in %s on %s: %s\n",
             __FUNCTION__, cm_service_get_name (service), error->message);
    g_error_free (error);
  }
}

gboolean
cm_service_disconnect (CmService *service)
{
  CmServicePrivate *priv = service->priv;
  GError *error = NULL;
  DBusGProxyCall *call;

  if (!priv->connected)
    return TRUE;

  call = dbus_g_proxy_begin_call (priv->proxy, "Disconnect",
                                  service_disconnect_call_notify, service,
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
service_connect_call_notify (DBusGProxy *proxy,
                             DBusGProxyCall *call,
                             gpointer data)
{
  CmService *service = data;
  GError *error = NULL;

  if (!dbus_g_proxy_end_call (proxy, call, &error, G_TYPE_INVALID))
  {
    g_debug ("Error calling dbus_g_proxy_end_call in %s on %s: %s\n",
             __FUNCTION__, cm_service_get_name (service), error->message);
    g_error_free (error);
  }
}

gboolean
cm_service_connect (CmService *service)
{
  CmServicePrivate *priv = service->priv;
  GError *error = NULL;
  DBusGProxyCall *call;

  if (priv->connected)
    return TRUE;

  /* 
   * We use an unusually long timeout since this dbus method
   * will not return until there is an error or till connman
   * has an IP address for the connection.
   */
  call = dbus_g_proxy_begin_call_with_timeout (priv->proxy, 
					       "Connect",
					       service_connect_call_notify, 
					       service, 
					       NULL,
					       120000,
					       G_TYPE_INVALID);
  if (!call)
  {
    g_debug ("Connect failed: %s\n", error ? error->message : "Unknown");
    g_error_free (error);
    return FALSE;
  }

  return TRUE;
}

static void
service_remove_call_notify (DBusGProxy *proxy,
                            DBusGProxyCall *call,
                            gpointer data)
{
  CmService *service = data;
  CmServicePrivate *priv = service->priv;
  GError *error = NULL;

  /* Clear the local passphrase, should possibly wait until daemon signals? */
  g_free (priv->passphrase);
  priv->passphrase = NULL;

  if (!dbus_g_proxy_end_call (proxy, call, &error, G_TYPE_INVALID))
  {
    g_debug ("Error calling dbus_g_proxy_end_call in %s on %s: %s\n",
             __FUNCTION__, cm_service_get_name (service), error->message);
    g_error_free (error);
  }
}

gboolean
cm_service_remove (CmService *service)
{
  CmServicePrivate *priv = service->priv;
  GError *error = NULL;
  DBusGProxyCall *call;

  call = dbus_g_proxy_begin_call (priv->proxy, "Remove",
                                  service_remove_call_notify, service, NULL,
                                  G_TYPE_INVALID);
  if (!call)
  {
    g_debug ("Remove failed: %s\n", error ? error->message : "Unknown");
    g_error_free (error);
    return FALSE;
  }

  return TRUE;
}

static void
service_set_property_call_notify (DBusGProxy *proxy,
				  DBusGProxyCall *call,
				  gpointer data)
{
  CmService *service = data;
  GError *error = NULL;

  if (!dbus_g_proxy_end_call (proxy, call, &error, G_TYPE_INVALID))
  {
    g_debug ("Error calling dbus_g_proxy_end_call in %s on %s: %s\n",
             __FUNCTION__, cm_service_get_name (service), error->message);
    g_error_free (error);
  }
}

gboolean
cm_service_set_property (CmService *service, const gchar *property,
                         GValue *value)
{
  CmServicePrivate *priv = service->priv;
  GError *error = NULL;
  DBusGProxyCall *call;

  call = dbus_g_proxy_begin_call (priv->proxy, "SetProperty",
                                  service_set_property_call_notify, service,
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

static void
service_move_before_call_notify (DBusGProxy *proxy, DBusGProxyCall *call,
                                 gpointer data)
{
  CmService *service = data;
  GError *error = NULL;

  if (!dbus_g_proxy_end_call (proxy, call, &error, G_TYPE_INVALID))
  {
    g_debug ("Error calling dbus_g_proxy_end_call in %s on %s: %s\n",
             __FUNCTION__, cm_service_get_name (service), error->message);
    g_error_free (error);
  }
}

gboolean
cm_service_move_before (CmService *service, CmService *before)
{
  CmServicePrivate *priv = service->priv;
  GError *error = NULL;
  const gchar *path = cm_service_get_path (before);
  DBusGProxyCall *call;

  call = dbus_g_proxy_begin_call (priv->proxy, "MoveBefore",
                                  service_move_before_call_notify,
                                  service, NULL,
                                  DBUS_TYPE_G_OBJECT_PATH, path,
                                  G_TYPE_INVALID);

  if (!call)
  {
    g_debug ("MoveBefore failed: %s\n", error ? error->message : "Unknown");
    g_error_free (error);
    return FALSE;
  }

  return TRUE;
}

static void
service_move_after_call_notify (DBusGProxy *proxy, DBusGProxyCall *call,
                                   gpointer data)
{
  CmService *service = data;
  GError *error = NULL;

  if (!dbus_g_proxy_end_call (proxy, call, &error, G_TYPE_INVALID))
  {
    g_debug ("Error calling dbus_g_proxy_end_call in %s on %s: %s\n",
             __FUNCTION__, cm_service_get_name (service), error->message);
    g_error_free (error);
  }
}

gboolean
cm_service_move_after (CmService *service, CmService *after)
{
  CmServicePrivate *priv = service->priv;
  GError *error = NULL;
  const gchar *path = cm_service_get_path (after);
  DBusGProxyCall *call;

  call = dbus_g_proxy_begin_call (priv->proxy, "MoveAfter",
                                  service_move_after_call_notify,
                                  service, NULL,
                                  DBUS_TYPE_G_OBJECT_PATH, path,
                                  G_TYPE_INVALID);

  if (!call)
  {
    g_debug ("MoveAfter failed: %s\n", error ? error->message : "Unknown");
    g_error_free (error);
    return FALSE;
  }

  return TRUE;
}

gboolean
cm_service_is_same (const CmService *first, const CmService *second)
{
  CmServicePrivate *first_priv = first->priv;
  CmServicePrivate *second_priv = second->priv;
  gboolean ret = FALSE;

  if (g_strcmp0 (first_priv->path, second_priv->path) == 0)
    ret = TRUE;

  return ret;
}

gint
cm_service_compare (CmService *first, CmService *second)
{
  CmServicePrivate *fpriv = first->priv;
  CmServicePrivate *spriv = second->priv;

  if (fpriv->order < spriv->order)
    return -1;
  else if (fpriv->order == spriv->order)
    return 0;
  else
    return 1;
}

/* Property getters/setters */
const gchar *
cm_service_get_state (CmService *service)
{
  CmServicePrivate *priv = service->priv;
  return priv->state;
}

/* Ethernet services may not have a name set, in which case return the type */
const gchar *
cm_service_get_name (const CmService *service)
{
  CmServicePrivate *priv = service->priv;
  if (priv->name == NULL && g_strcmp0 ("ethernet", priv->type) == 0)
    return priv->type;
  else
    return priv->name;
}

const gchar *
cm_service_get_mode (CmService *service)
{
  CmServicePrivate *priv = service->priv;
  return priv->mode;
}

const gchar *
cm_service_get_security (CmService *service)
{
  CmServicePrivate *priv = service->priv;
  return priv->security;
}

const gchar *
cm_service_get_passphrase (CmService *service)
{
  CmServicePrivate *priv = service->priv;
  return priv->passphrase;
}

gboolean
cm_service_set_passphrase (CmService *service, const gchar *passphrase)
{
  GValue value = { 0 };
  gboolean ret;

  g_value_init (&value, G_TYPE_STRING);
  g_value_set_static_string (&value, passphrase);
  ret = cm_service_set_property (service, "Passphrase", &value);
  g_value_unset (&value);

  return ret;
}

const char *
cm_service_get_type (CmService *service)
{
  CmServicePrivate *priv = service->priv;
  return priv->type;
}

guint
cm_service_get_strength (CmService *service)
{
  CmServicePrivate *priv = service->priv;
  return priv->strength;
}

gint
cm_service_get_order (CmService *service)
{
  CmServicePrivate *priv = service->priv;
  return priv->order;
}

void
cm_service_set_order (CmService *service, gint order)
{
  CmServicePrivate *priv = service->priv;
  priv->order = order;
}

gboolean
cm_service_get_favorite (CmService *service)
{
  CmServicePrivate *priv = service->priv;
  return priv->favorite;
}

gboolean
cm_service_get_connected (CmService *service)
{
  CmServicePrivate *priv = service->priv;
  return priv->connected;
}

const gchar *
cm_service_get_path (CmService *service)
{
  CmServicePrivate *priv = service->priv;

  return priv->path;
}

const gchar *
cm_service_get_method (CmService *service)
{
  CmServicePrivate *priv = service->priv;

  return priv->method;
}

gboolean
cm_service_make_default (CmService *service)
{
  CmServicePrivate *priv = service->priv;
  gboolean ret = FALSE;
  const GList *services = cm_manager_get_services (priv->manager);
  CmService *first = services->data;

  if (!priv->connected)
    ret = cm_service_connect (service);

  if (ret)
    ret = cm_service_move_before (service, first);

  return ret;
}

const gchar *
cm_service_get_error (CmService *service)
{
  CmServicePrivate *priv = service->priv;
  return priv->error;
}

/*****************************************************************************
 *
 *
 * GObject class_init, init, and finalize methods
 *
 *
 *****************************************************************************/

static void
service_dispose (GObject *object)
{
  CmService *service = CM_SERVICE (object);
  CmServicePrivate *priv = service->priv;

  if (priv->proxy)
  {
    dbus_g_proxy_disconnect_signal (
      priv->proxy, "PropertyChanged",
      G_CALLBACK (service_property_change_handler_proxy),
      service);

    g_object_unref (priv->proxy);
    priv->proxy = NULL;
  }

  priv->manager = NULL;

  G_OBJECT_CLASS (service_parent_class)->dispose (object);
}

static void
service_finalize (GObject *object)
{
  CmService *service = CM_SERVICE (object);
  CmServicePrivate *priv = service->priv;

  g_free (priv->state);
  g_free (priv->name);
  g_free (priv->type);
  g_free (priv->mode);
  g_free (priv->security);
  g_free (priv->path);
  g_free (priv->passphrase);
  g_free (priv->error);
  g_free (priv->method);

  G_OBJECT_CLASS (service_parent_class)->finalize (object);
}

static void
service_init (CmService *self)
{
  self->priv = CM_SERVICE_GET_PRIVATE (self);
  self->priv->manager = NULL;
  self->priv->path = NULL;
  self->priv->state = NULL;
  self->priv->name = NULL;
  self->priv->type = NULL;
  self->priv->mode = NULL;
  self->priv->security = NULL;
  self->priv->passphrase = NULL;
  self->priv->favorite = FALSE;
  self->priv->connected = FALSE;
  self->priv->error = NULL;
  self->priv->method = NULL;
}

static void
service_class_init (CmServiceClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = service_dispose;
  gobject_class->finalize = service_finalize;

  service_signals[SIGNAL_UPDATE] = g_signal_new (
    "service-updated",
    G_TYPE_FROM_CLASS (gobject_class),
    G_SIGNAL_RUN_LAST,
    0,
    NULL, NULL,
    g_cclosure_marshal_VOID__VOID,
    G_TYPE_NONE, 0);
  service_signals[SIGNAL_STATE_CHANGED] = g_signal_new (
    "state-changed",
    G_TYPE_FROM_CLASS (gobject_class),
    G_SIGNAL_RUN_LAST,
    0,
    NULL, NULL,
    g_cclosure_marshal_VOID__STRING,
    G_TYPE_NONE,
    1,
    G_TYPE_STRING);
  service_signals[SIGNAL_NAME_CHANGED] = g_signal_new (
    "name-changed",
    G_TYPE_FROM_CLASS (gobject_class),
    G_SIGNAL_RUN_LAST,
    0,
    NULL, NULL,
    g_cclosure_marshal_VOID__STRING,
    G_TYPE_NONE,
    1,
    G_TYPE_STRING);
  service_signals[SIGNAL_TYPE_CHANGED] = g_signal_new (
    "type-changed",
    G_TYPE_FROM_CLASS (gobject_class),
    G_SIGNAL_RUN_LAST,
    0,
    NULL, NULL,
    g_cclosure_marshal_VOID__STRING,
    G_TYPE_NONE,
    1,
    G_TYPE_STRING);
  service_signals[SIGNAL_MODE_CHANGED] = g_signal_new (
    "mode-changed",
    G_TYPE_FROM_CLASS (gobject_class),
    G_SIGNAL_RUN_LAST,
    0,
    NULL, NULL,
    g_cclosure_marshal_VOID__STRING,
    G_TYPE_NONE,
    1,
    G_TYPE_STRING);
  service_signals[SIGNAL_SECURITY_CHANGED] = g_signal_new (
    "security-changed",
    G_TYPE_FROM_CLASS (gobject_class),
    G_SIGNAL_RUN_LAST,
    0,
    NULL, NULL,
    g_cclosure_marshal_VOID__STRING,
    G_TYPE_NONE,
    1,
    G_TYPE_STRING);
  service_signals[SIGNAL_PASSPHRASE_CHANGED] = g_signal_new (
    "passphrase-changed",
    G_TYPE_FROM_CLASS (gobject_class),
    G_SIGNAL_RUN_LAST,
    0,
    NULL, NULL,
    g_cclosure_marshal_VOID__STRING,
    G_TYPE_NONE,
    1,
    G_TYPE_STRING);
  service_signals[SIGNAL_STRENGTH_CHANGED] = g_signal_new (
    "strength-changed",
    G_TYPE_FROM_CLASS (gobject_class),
    G_SIGNAL_RUN_LAST,
    0,
    NULL, NULL,
    g_cclosure_marshal_VOID__UINT,
    G_TYPE_NONE,
    1,
    G_TYPE_UINT);
  service_signals[SIGNAL_FAVORITE_CHANGED] = g_signal_new (
    "favorite-changed",
    G_TYPE_FROM_CLASS (gobject_class),
    G_SIGNAL_RUN_LAST,
    0,
    NULL, NULL,
    g_cclosure_marshal_VOID__BOOLEAN,
    G_TYPE_NONE,
    1,
    G_TYPE_BOOLEAN);
  service_signals[SIGNAL_ERROR_CHANGED] = g_signal_new (
    "error-changed",
    G_TYPE_FROM_CLASS (gobject_class),
    G_SIGNAL_RUN_LAST,
    0,
    NULL, NULL,
    g_cclosure_marshal_VOID__STRING,
    G_TYPE_NONE,
    1,
    G_TYPE_STRING);
  service_signals[SIGNAL_METHOD_CHANGED] = g_signal_new (
    "method-changed",
    G_TYPE_FROM_CLASS (gobject_class),
    G_SIGNAL_RUN_LAST,
    0,
    NULL, NULL,
    g_cclosure_marshal_VOID__STRING,
    G_TYPE_NONE,
    1,
    G_TYPE_STRING);

  g_type_class_add_private (gobject_class, sizeof (CmServicePrivate));
}

