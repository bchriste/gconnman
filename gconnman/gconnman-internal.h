#ifndef __gconnman_internal_h__
#define __gconnman_internal_h__

#include <glib.h>
#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include <gconnman/gconnman.h>


/* object management */
CmNetwork *internal_network_new (DBusGProxy *proxy, CmDevice *device,
                                 const gchar *path, GError **error);
CmDevice *internal_device_new (DBusGProxy *proxy, const gchar *path,
                               GError **error);
CmService *internal_service_new (DBusGProxy *proxy, const gchar *path,
				 GError **error);
CmConnection *internal_connection_new (DBusGProxy *proxy, const gchar *path,
                                       GError **error);

#endif
