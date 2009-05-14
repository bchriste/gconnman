#ifndef __cm_manager_h__
#define __cm_manager_h__

#define CONNMAN_SERVICE			"org.moblin.connman"

typedef struct _CmManager CmManager;
typedef struct _CmManagerClass CmManagerClass;
typedef struct _CmManagerPrivate CmManagerPrivate;

#include <gconnman/gconnman.h>
#include <gconnman/cm-service.h>

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
} CmManagerError;

#define CONNMAN_MANAGER_INTERFACE	CONNMAN_SERVICE ".Manager"
#define CONNMAN_MANAGER_PATH		"/"

CmManager *cm_manager_new (GError **error);
/* getters */
GList *cm_manager_get_devices (CmManager *manager);
GList *cm_manager_get_connections (CmManager *manager);
GList *cm_manager_get_services (CmManager *manager);
gboolean cm_manager_get_offline_mode (CmManager *manager);
const gchar *cm_manager_get_state (CmManager *manager);
CmService *cm_manager_get_active_service (CmManager *manager);
const gchar *cm_manager_get_active_service_state (CmManager *manager);
const gchar *cm_manager_get_active_service_type (CmManager *manager);
const gchar *cm_manager_get_active_service_name (CmManager *manager);

gboolean cm_manager_refresh (CmManager *manager);
void cm_manager_set_offline_mode (CmManager *manager, gboolean offline);

G_END_DECLS

#endif /* __cm_manager_h__ */
