#ifndef __cm_connection_h__
#define __cm_connection_h__

typedef struct _CmConnection CmConnection;
typedef struct _CmConnectionClass CmConnectionClass;
typedef struct _CmConnectionPrivate CmConnectionPrivate;

#include <gconnman/gconnman.h>

G_BEGIN_DECLS

#define CM_TYPE_CONNECTION            (connection_get_type ())
#define CM_CONNECTION(obj)            (G_TYPE_CHECK_INSTANCE_CAST (           \
                                       (obj), CM_TYPE_CONNECTION, CmConnection))
#define CM_CONNECTION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (              \
                                       (klass), CM_TYPE_CONNECTION, CmConnectionClass))
#define CM_IS_CONNECTION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (   \
                                       (obj), CM_TYPE_CONNECTION))
#define CM_IS_CONNECTION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE (      \
                                       (klass), CM_TYPE_CONNECTION))
#define CM_CONNECTION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS (            \
                                       (obj), CM_TYPE_CONNECTION, CmConnectionClass))

GType connection_get_type (void) G_GNUC_CONST;

struct _CmConnection
{
  /*< private >*/
  GObject parent_instance;
  CmConnectionPrivate *priv;
};

struct _CmConnectionClass
{
  GObjectClass parent_class;
};

typedef enum
{
  CONNECTION_ERROR_NO_MEMORY,
  CONNECTION_ERROR_CONNMAN_INTERFACE,      /* Connection interface does not exist */
  CONNECTION_ERROR_CONNMAN_GET_PROPERTIES, /* GetProperties failed on Connection */
} CmConnectionError;

#define CONNMAN_CONNECTION_INTERFACE	CONNMAN_SERVICE ".Connection"

typedef enum
{
  CONNECTION_UNKNOWN = 0,
  CONNECTION_WIFI = 1,
  CONNECTION_WIMAX,
  CONNECTION_BLUETOOTH,
  CONNECTION_CELLULAR,
  CONNECTION_ETHERNET,
} CmConnectionType;


CmConnectionType cm_connection_get_type (const CmConnection *connection);
const gchar *cm_connection_type_to_string (CmConnectionType type);
void cm_connection_free (CmConnection *connection);
gboolean cm_connection_is_same (const CmConnection *connection, const gchar *path);

const gchar *cm_connection_get_interface (CmConnection *connection);
const gchar *cm_connection_get_path (CmConnection *connection);
guint cm_connection_get_strength (CmConnection *connection);
gboolean cm_connection_get_default (CmConnection *connection);

G_END_DECLS

#endif /* __cm_connection_h__ */