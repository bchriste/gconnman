
#include <glib.h>
#include <string.h> /* strcmp */
#include <stdlib.h> /* system */

#include "debug.h"
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
  gchar *state;
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
  SIGNAL_LAST
};

static gint manager_signals[SIGNAL_LAST];

static void
manager_emit_updated (CmManager *manager)
{
  g_signal_emit (manager, manager_signals[SIGNAL_UPDATE], 0 /* detail */);
}

static CmDevice *
manager_find_device (CmManager *manager, const gchar *path)
{
  CmManagerPrivate *priv = manager->priv;
  GList *tmp = priv->devices;
  while (tmp)
  {
    CmDevice *device = tmp->data;
    if (cm_device_is_same (device, path))
      return device;
    tmp = tmp->next;
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
    const gchar *path;

    for (i = 0; i < devices->len; i++)
    {
      path = g_ptr_array_index (devices, i);
      CmDevice *device = manager_find_device (manager, path);
      if (!device)
      {
        GError *error = NULL;
	g_print ("New device found: %s\n", path);
	device = internal_device_new (priv->proxy, path, &error);
	if (!device)
	{
	  g_print ("device_new failed in %s: %s\n", __FUNCTION__,
            error->message);
          g_clear_error (&error);
	  continue;
	}
	priv->devices = g_list_append (priv->devices, device);
      }
    }
    g_signal_emit (manager, manager_signals[SIGNAL_DEVICES_CHANGED], 0);
    return;
  }

  if (!strcmp ("Services", key))
  {
    GPtrArray *services = g_value_get_boxed (value);
    gint i;
    const gchar *path;
    GError *error = NULL;

    /* We are receiving a list which is potentially entirely different
     * from what we have. Throw away the current list and create a new
     * one from scratch.
     */
    while (priv->services)
    {
      g_object_unref (priv->services->data);
      priv->services = g_list_delete_link (priv->services, priv->services);
    }

    for (i = 0; i < services->len; i++)
    {
      path = g_ptr_array_index (services, i);
      CmService *service = internal_service_new (priv->proxy, path, &error);
      if (!service)
      {
        g_print ("service_new failed in %s: %s\n", __FUNCTION__,
			error->message);
	g_clear_error (&error);
	continue;
      }
      priv->services = g_list_append (priv->services, service);
    }
    g_signal_emit (manager, manager_signals[SIGNAL_SERVICES_CHANGED], 0);
    return;
  }

  if (!strcmp ("OfflineMode", key))
  {
    priv->offline_mode = g_value_get_boolean (value);
    g_signal_emit (manager, manager_signals[SIGNAL_OFFLINE_MODE_CHANGED], 0);
    return;
  }

  if (!strcmp ("State", key))
  {
    g_free (priv->state);
    priv->state = g_strdup (g_value_get_string (value));
    g_signal_emit (manager, manager_signals[SIGNAL_STATE_CHANGED], 0);
    return;
  }

  tmp = g_strdup_value_contents (value);
  g_print ("Unhandled property on Manager: %s = %s\n",
           key, tmp);
  g_free (tmp);
}

gboolean
cm_manager_refresh (CmManager *manager)
{
  CmManagerPrivate *priv = manager->priv;
  GError *error = NULL;
  GHashTable *properties = NULL;

  /* Remove all the prior devices */
  while (priv->devices)
  {
    g_object_unref (priv->devices->data);
    priv->devices = g_list_delete_link (priv->devices, priv->devices);
  }

  dbus_g_proxy_add_signal (
    priv->proxy, "PropertyChanged",
    G_TYPE_STRING, G_TYPE_VALUE, G_TYPE_INVALID);

  dbus_g_proxy_connect_signal (
    priv->proxy, "PropertyChanged",
    G_CALLBACK (manager_property_change_handler_proxy),
    manager, NULL);

  /* Synchronous call for now... */
  if (!dbus_g_proxy_call (priv->proxy, "GetProperties", &error,
			  /* IN values */
			  G_TYPE_INVALID,
			  /* OUT values */
			  dbus_g_type_get_map ("GHashTable", G_TYPE_STRING,
					       G_TYPE_VALUE),
			  &properties, G_TYPE_INVALID))
    goto manager_refresh_error;

  g_hash_table_foreach (properties, (GHFunc)manager_update_property, manager);
  g_hash_table_unref (properties);

  manager_emit_updated (manager);

  return TRUE;

manager_refresh_error:
  if (error)
  {
    g_warning ("Error in %s:\n%s", __FUNCTION__, error->message);
    g_clear_error (&error);
  }
  if (properties)
    g_hash_table_unref (properties);

  return FALSE;
}


static void
manager_property_change_handler_proxy (DBusGProxy *proxy,
				      const gchar *key,
				      GValue *value,
				      gpointer data)
{
  CmManager *manager = data;
  gchar *tmp = g_strdup_value_contents (value);
  g_print ("PropertyChange on Manager: %s = %s\n", key, tmp);
  g_free (tmp);

  manager_update_property (key, value, manager);
}

static GQuark
manager_error_quark (void)
{
  return g_quark_from_static_string ("manager-error-quark");
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

GList *
cm_manager_get_devices (CmManager *manager)
{
  CmManagerPrivate *priv = manager->priv;
  return g_list_copy (priv->devices);
}

GList *
cm_manager_get_services (CmManager *manager)
{
  CmManagerPrivate *priv = manager->priv;
  return g_list_copy (priv->services);
}

gboolean
cm_manager_get_offline_mode (CmManager *manager)
{
  CmManagerPrivate *priv = manager->priv;
  return priv->offline_mode;
}

void
cm_manager_set_offline_mode (CmManager *manager, gboolean offline)
{
  CmManagerPrivate *priv = manager->priv;
  priv->offline_mode = offline;
}

/*
 * The list of services is sorted by connman so the active service
 * should always be the first item in our list
 */
const gchar *
cm_manager_get_active_service_state (CmManager *manager)
{
  CmManagerPrivate *priv = manager->priv;
  CmService *active = (CmService *)g_list_first (priv->services)->data;

  return cm_service_get_state (active);
}

const gchar *
cm_manager_get_active_service_name (CmManager *manager)
{
  CmManagerPrivate *priv = manager->priv;
  CmService *active = (CmService *)g_list_first (priv->services)->data;
  const gchar *name = cm_service_get_name (active);
  if (!name)
  {
    name = cm_service_get_type (active);
  }

  return name;
}

const gchar *
cm_manager_get_active_service_type (CmManager *manager)
{
  CmManagerPrivate *priv = manager->priv;
  CmService *active = (CmService *)g_list_first (priv->services)->data;

  return cm_service_get_type (active);
}

const gchar *
cm_manager_get_state (CmManager *manager)
{
  CmManagerPrivate *priv = manager->priv;
  return priv->state;
}

/*****************************************************************************
 *
 *
 * GObject class_init, init, and finalize methods
 *
 *
 *****************************************************************************/

static void
manager_finalize (GObject *object)
{
  CmManager *manager = CM_MANAGER (object);
  CmManagerPrivate *priv = manager->priv;

  dbus_g_proxy_disconnect_signal (
    priv->proxy, "PropertyChanged",
    G_CALLBACK (manager_property_change_handler_proxy),
    manager);

  while (priv->devices)
  {
    g_object_unref (priv->devices->data);
    priv->devices = g_list_delete_link (priv->devices, priv->devices);
  }

  if (priv->proxy)
    g_object_unref (priv->proxy);

  if (priv->connection)
    dbus_g_connection_unref (priv->connection);

  G_OBJECT_CLASS (manager_parent_class)->finalize (object);
}

static void
manager_init (CmManager *self)
{
  self->priv = CM_MANAGER_GET_PRIVATE (self);
}

static void
manager_class_init (CmManagerClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = manager_finalize;

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
    "offline-mode-changed",
    G_TYPE_FROM_CLASS (gobject_class),
    G_SIGNAL_RUN_LAST,
    0,
    NULL, NULL,
    g_cclosure_marshal_VOID__VOID,
    G_TYPE_NONE, 0);
  manager_signals[SIGNAL_SERVICES_CHANGED] = g_signal_new (
    "offline-mode-changed",
    G_TYPE_FROM_CLASS (gobject_class),
    G_SIGNAL_RUN_LAST,
    0,
    NULL, NULL,
    g_cclosure_marshal_VOID__VOID,
    G_TYPE_NONE, 0);

  g_type_class_add_private (gobject_class, sizeof (CmManagerPrivate));
}

