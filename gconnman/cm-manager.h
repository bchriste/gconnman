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

#ifndef __cm_manager_h__
#define __cm_manager_h__

#define CONNMAN_SERVICE			"org.moblin.connman"

typedef struct _CmManager CmManager;
typedef struct _CmManagerClass CmManagerClass;
typedef struct _CmManagerPrivate CmManagerPrivate;

#include <gconnman/gconnman.h>
#include <gconnman/cm-service.h>
#include <gconnman/cm-connection.h>

G_BEGIN_DECLS

#define CM_TYPE_MANAGER            (manager_get_type ())
#define CM_MANAGER(obj)            (G_TYPE_CHECK_INSTANCE_CAST (           \
                                    (obj), CM_TYPE_MANAGER, CmManager))
#define CM_MANAGER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (              \
                                    (klass), CM_TYPE_MANAGER, CmManagerClass))
#define CM_IS_MANAGER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (   \
                                    (obj), CM_TYPE_MANAGER))
#define CM_IS_MANAGER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE (      \
                                    (klass), CM_TYPE_MANAGER))
#define CM_MANAGER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS (            \
                                    (obj), CM_TYPE_MANAGER, CmManagerClass))

GType manager_get_type (void) G_GNUC_CONST;

struct _CmManager
{
  /*< private >*/
  GObject parent_instance;
  CmManagerPrivate *priv;
};

struct _CmManagerClass
{
  GObjectClass parent_class;
};

typedef enum
{
  MANAGER_ERROR_NO_CONNMAN, /* DBus failed to connect to Connman service */
  MANAGER_ERROR_CONNMAN_GET_PROPERTIES, /* GetProperties failed on Manager */
} CmManagerError;

#define CONNMAN_MANAGER_INTERFACE	CONNMAN_SERVICE ".Manager"
#define CONNMAN_MANAGER_PATH		"/"

CmManager *cm_manager_new (GError **error);
/* getters */
const GList *cm_manager_get_devices (CmManager *manager);
const GList *cm_manager_get_connections (CmManager *manager);
const GList *cm_manager_get_services (CmManager *manager);
gboolean cm_manager_get_offline_mode (CmManager *manager);
gboolean cm_manager_set_offline_mode (CmManager *manager, gboolean offline);
const gchar *cm_manager_get_state (CmManager *manager);
CmService *cm_manager_get_active_service (CmManager *manager);
CmConnection *cm_manager_get_active_connection (CmManager *manager);
const gchar *cm_manager_get_policy (CmManager *manager);
gboolean cm_manager_set_policy (CmManager *manager, gchar *policy);

gboolean cm_manager_refresh (CmManager *manager);

gboolean cm_manager_request_scan (CmManager *manager);
gboolean cm_manager_request_scan_devices (CmManager *manager, CmDeviceType type);

CmDevice *cm_manager_find_device (CmManager *manager, const gchar *opath);
CmService *cm_manager_find_service (CmManager *manager, const gchar *opath);
CmConnection *cm_manager_find_connection (CmManager *manager,
                                          const gchar *opath);

G_END_DECLS

#endif /* __cm_manager_h__ */
