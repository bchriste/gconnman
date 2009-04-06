#ifndef __manager_h__
#define __manager_h__

#define CONNMAN_SERVICE			"org.moblin.connman"

typedef struct _Manager Manager;
typedef struct _ManagerClass ManagerClass;
typedef struct _ManagerPrivate ManagerPrivate;

#include <gconnman/gconnman.h>

G_BEGIN_DECLS

#define TYPE_MANAGER            (manager_get_type ())
#define MANAGER(obj)            (G_TYPE_CHECK_INSTANCE_CAST (           \
                                   (obj), TYPE_MANAGER, Manager))
#define MANAGER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (              \
                                   (klass), TYPE_MANAGER, ManagerClass))
#define IS_MANAGER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (   \
                                   (obj), TYPE_MANAGER))
#define IS_MANAGER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE (      \
                                   (klass), TYPE_MANAGER))
#define MANAGER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS (            \
                                   (obj), TYPE_MANAGER, ManagerClass))

GType manager_get_type (void) G_GNUC_CONST;

struct _Manager
{
  /*< private >*/
  GObject parent_instance;
  ManagerPrivate *priv;
};

struct _ManagerClass
{
  GObjectClass parent_class;
};

typedef enum 
{
  MANAGER_ERROR_NO_CONNMAN, /* DBus failed to connect to Connman service */
} ManagerError;

#define CONNMAN_MANAGER_INTERFACE	CONNMAN_SERVICE ".Manager"
#define CONNMAN_MANAGER_PATH		"/"

Manager *cm_manager_new (GError **error);
GList *cm_manager_get_devices (Manager *manager);
gboolean cm_manager_refresh (Manager *manager);

G_END_DECLS

#endif /* __manager_h__ */
