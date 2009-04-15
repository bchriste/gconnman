#ifndef __cm_network_h__
#define __cm_network_h__

typedef struct _CmNetwork CmNetwork;
typedef struct _CmNetworkClass CmNetworkClass;
typedef struct _CmNetworkPrivate CmNetworkPrivate;

#include <gconnman/gconnman.h>

G_BEGIN_DECLS

#define CM_TYPE_NETWORK            (network_get_type ())
#define CM_NETWORK(obj)            (G_TYPE_CHECK_INSTANCE_CAST (           \
                                    (obj), CM_TYPE_NETWORK, CmNetwork))
#define CM_NETWORK_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (              \
                                    (klass), CM_TYPE_NETWORK, CmNetworkClass))
#define CM_IS_NETWORK(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (   \
                                    (obj), CM_TYPE_NETWORK))
#define CM_IS_NETWORK_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE (      \
                                    (klass), CM_TYPE_NETWORK))
#define CM_NETWORK_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS (            \
                                    (obj), CM_TYPE_NETWORK, CmNetworkClass))

GType network_get_type (void) G_GNUC_CONST;

struct _CmNetwork
{
  /*< private >*/
  GObject parent_instance;
  CmNetworkPrivate *priv;
};

struct _CmNetworkClass
{
  GObjectClass parent_class;
};

#define NETWORK_ERROR network_error_quark ()

typedef enum
{
  NETWORK_ERROR_NO_MEMORY,
  NETWORK_ERROR_CONNMAN_INTERFACE,      /* Network interface does not exist */
  NETWORK_ERROR_CONNMAN_GET_PROPERTIES, /* GetProperties failed on Network */
} CmNetworkError;

#define CONNMAN_NETWORK_INTERFACE	CONNMAN_SERVICE ".Network"

typedef enum
{
  NETWORK_INFO_SSID       = 1 << 0,
  NETWORK_INFO_STRENGTH   = 1 << 1,
  NETWORK_INFO_PRIORITY   = 1 << 2,
  NETWORK_INFO_REMEMBER   = 1 << 3,
  NETWORK_INFO_AVAILABLE  = 1 << 4,
  NETWORK_INFO_CONNECTED  = 1 << 5,
  NETWORK_INFO_NAME       = 1 << 6,
  NETWORK_INFO_SECURITY   = 1 << 7,
  NETWORK_INFO_PASSPHRASE = 1 << 8,
  NETWORK_INFO_MODE       = 1 << 9,
} CmNetworkInfoMask;

/* debug */
void cm_network_print (const CmNetwork *network);

/* methods */
gboolean cm_network_connect (CmNetwork *network);
gboolean cm_network_disconnect (CmNetwork *network);

/* const getters */
gboolean cm_network_is_same (const CmNetwork *network, const gchar *path);
const gchar *cm_network_get_name (const CmNetwork *network);
gboolean cm_network_is_available (const CmNetwork *network);
gboolean cm_network_is_connected (const CmNetwork *network);
gboolean cm_network_is_secure (const CmNetwork *network);
gulong cm_network_get_timestamp (const CmNetwork *network);
guchar cm_network_get_strength (const CmNetwork *network);
guchar cm_network_get_priority (const CmNetwork *network);
gboolean cm_network_has_passphrase (const CmNetwork *network);
gint cm_network_get_passphrase_length (const CmNetwork *network);
CmDevice *cm_network_get_device (CmNetwork *network);
gboolean cm_network_set_passphrase (CmNetwork *network, const gchar *passphrase);

#endif
