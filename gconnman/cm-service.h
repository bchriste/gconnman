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

#ifndef __cm_service_h__
#define __cm_service_h__

typedef struct _CmService CmService;
typedef struct _CmServiceClass CmServiceClass;
typedef struct _CmServicePrivate CmServicePrivate;

#include <gconnman/gconnman.h>

G_BEGIN_DECLS

#define CM_TYPE_SERVICE            (service_get_type ())
#define CM_SERVICE(obj)            (G_TYPE_CHECK_INSTANCE_CAST (           \
                                    (obj), CM_TYPE_SERVICE, CmService))
#define CM_SERVICE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (              \
                                    (klass), CM_TYPE_SERVICE, CmServiceClass))
#define CM_IS_SERVICE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (   \
                                    (obj), CM_TYPE_SERVICE))
#define CM_IS_SERVICE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE (      \
                                    (klass), CM_TYPE_SERVICE))
#define CM_SERVICE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS (            \
                                    (obj), CM_TYPE_SERVICE, CmServiceClass))

GType service_get_type (void) G_GNUC_CONST;

struct _CmService
{
  /*< private >*/
  GObject parent_instance;
  CmServicePrivate *priv;
};

struct _CmServiceClass
{
  GObjectClass parent_class;
};

#define SERVICE_ERROR service_error_quark ()

typedef enum
{
  SERVICE_ERROR_NO_MEMORY,
  SERVICE_ERROR_CONNMAN_INTERFACE,      /* Service interface does not exist */
  SERVICE_ERROR_CONNMAN_GET_PROPERTIES, /* GetProperties failed on Service */
} CmServiceError;

#define CONNMAN_SERVICE_INTERFACE	CONNMAN_SERVICE ".Service"

typedef enum
{
  SERVICE_INFO_STATE	  = 1 << 0,
  SERVICE_INFO_NAME 	  = 1 << 1,
  SERVICE_INFO_TYPE	  = 1 << 2,
  SERVICE_INFO_MODE       = 1 << 3,
  SERVICE_INFO_SECURITY   = 1 << 4,
  SERVICE_INFO_PASSPHRASE = 1 << 5,
  SERVICE_INFO_STRENGTH   = 1 << 6,
  SERVICE_INFO_FAVORITE   = 1 << 7,
} CmServiceInfoMask;

/* debug */
void cm_service_print (const CmService *service);

/* methods */
gboolean cm_service_connect (CmService *service);
gboolean cm_service_disconnect (CmService *service);
gboolean cm_service_move_before (CmService *service, CmService *before);
gboolean cm_service_move_after (CmService *service, CmService *after);

/* const getters */
const gchar *cm_service_get_path (CmService *service);
const gchar *cm_service_get_state (CmService *service);
const gchar *cm_service_get_name (const CmService *service);
const gchar *cm_service_get_type (CmService *service);
const gchar *cm_service_get_mode (CmService *service);
const gchar *cm_service_get_security (CmService *service);
const gchar *cm_service_get_passphrase (CmService *service);
guint cm_service_get_strength (CmService *service);
gboolean cm_service_get_favorite (CmService *service);
gboolean cm_service_get_connected (CmService *service);

gboolean cm_service_set_passphrase (CmService *service, const char* passphrase);

#endif
