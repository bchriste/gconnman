#ifndef __device_h__
#define __device_h__

typedef struct _Device Device;
typedef struct _DeviceClass DeviceClass;
typedef struct _DevicePrivate DevicePrivate;

#include <gconnman/gconnman.h>

G_BEGIN_DECLS

#define TYPE_DEVICE            (device_get_type ())
#define DEVICE(obj)            (G_TYPE_CHECK_INSTANCE_CAST (           \
                                   (obj), TYPE_DEVICE, Device))
#define DEVICE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (              \
                                   (klass), TYPE_DEVICE, DeviceClass))
#define IS_DEVICE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (   \
                                   (obj), TYPE_DEVICE))
#define IS_DEVICE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE (      \
                                   (klass), TYPE_DEVICE))
#define DEVICE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS (            \
                                   (obj), TYPE_DEVICE, DeviceClass))

GType device_get_type (void) G_GNUC_CONST;

struct _Device
{
  /*< private >*/
  GObject parent_instance;
  DevicePrivate *priv;
};

struct _DeviceClass
{
  GObjectClass parent_class;
};

typedef enum 
{
  DEVICE_ERROR_NO_MEMORY,
  DEVICE_ERROR_CONNMAN_INTERFACE,      /* Device interface does not exist */
  DEVICE_ERROR_CONNMAN_GET_PROPERTIES, /* GetProperties failed on Device */
} DeviceError;

#define CONNMAN_DEVICE_INTERFACE	CONNMAN_SERVICE ".Device"

typedef enum 
{
  DEVICE_UNKNOWN = 0,
  DEVICE_WIFI = 1,
  DEVICE_WIMAX,
  DEVICE_BLUETOOTH,
  DEVICE_CELLULAR,
  DEVICE_ETHERNET,
} DeviceType;


DeviceType cm_device_get_type (const Device *device);
const gchar *cm_device_type_to_string (DeviceType type);
GList *cm_device_get_networks (Device *device);
void cm_device_free (Device *device);
const gchar *cm_device_get_path (Device *device);
gboolean cm_device_scan (Device *device);
gboolean cm_device_is_same (const Device *device, const gchar *path);
void cm_device_print (const Device *device);
const gchar *cm_device_get_name (const Device *device);
gboolean cm_device_is_scanning (const Device *device);

G_END_DECLS

#endif /* __manager_h__ */
