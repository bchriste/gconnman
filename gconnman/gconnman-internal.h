#ifndef __gconnman_internal_h__
#define __gconnman_internal_h__

#include <glib.h>
#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include <gconnman/gconnman.h>


/* object management */
Network *internal_network_new (DBusGProxy *proxy, Device *device, 
                               const gchar *path, GError **error);
Device *internal_device_new (DBusGProxy *proxy, const gchar *path, 
                             GError **error);

#endif
