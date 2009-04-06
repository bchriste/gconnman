#ifndef __network_h__
#define __network_h__

typedef struct _Network Network;
typedef struct _NetworkClass NetworkClass;
typedef struct _NetworkPrivate NetworkPrivate;

#include <gconnman/gconnman.h>

G_BEGIN_DECLS

#define TYPE_NETWORK            (network_get_type ())
#define NETWORK(obj)            (G_TYPE_CHECK_INSTANCE_CAST (           \
                                   (obj), TYPE_NETWORK, Network))
#define NETWORK_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (              \
                                   (klass), TYPE_NETWORK, NetworkClass))
#define IS_NETWORK(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (   \
                                   (obj), TYPE_NETWORK))
#define IS_NETWORK_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE (      \
                                   (klass), TYPE_NETWORK))
#define NETWORK_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS (            \
                                   (obj), TYPE_NETWORK, NetworkClass))

GType network_get_type (void) G_GNUC_CONST;

struct _Network
{
  /*< private >*/
  GObject parent_instance;
  NetworkPrivate *priv;
};

struct _NetworkClass
{
  GObjectClass parent_class;
};

#define NETWORK_ERROR network_error_quark ()

typedef enum 
{
  NETWORK_ERROR_NO_MEMORY,
  NETWORK_ERROR_CONNMAN_INTERFACE,      /* Network interface does not exist */
  NETWORK_ERROR_CONNMAN_GET_PROPERTIES, /* GetProperties failed on Network */
} NetworkError;

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
} NetworkInfoMask;

/* debug */
void cm_network_print (const Network *network);

/* methods */
gboolean cm_network_connect (Network *network);
gboolean cm_network_disconnect (Network *network);

/* const getters */
gboolean cm_network_is_same (const Network *network, const gchar *path);
const gchar *cm_network_get_name (const Network *network);
gboolean cm_network_is_available (const Network *network);
gboolean cm_network_is_connected (const Network *network);
gboolean cm_network_is_secure (const Network *network);
gulong cm_network_get_timestamp (const Network *network);
guchar cm_network_get_strength (const Network *network);
guchar cm_network_get_priority (const Network *network);
gboolean cm_network_has_passphrase (const Network *network);
gint cm_network_get_passphrase_length (const Network *network);
Device *cm_network_get_device (Network *network);
gboolean cm_network_set_passphrase (Network *network, const gchar *passphrase);

#endif
