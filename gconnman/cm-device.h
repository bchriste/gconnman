#ifndef __cm_device_h__
#define __cm_device_h__

typedef struct _CmDevice CmDevice;
typedef struct _CmDeviceClass CmDeviceClass;
typedef struct _CmDevicePrivate CmDevicePrivate;

#include <gconnman/gconnman.h>

G_BEGIN_DECLS

#define CM_TYPE_DEVICE            (device_get_type ())
#define CM_DEVICE(obj)            (G_TYPE_CHECK_INSTANCE_CAST (           \
                                   (obj), CM_TYPE_DEVICE, CmDevice))
#define CM_DEVICE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (              \
                                   (klass), CM_TYPE_DEVICE, CmDeviceClass))
#define CM_IS_DEVICE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (   \
                                   (obj), CM_TYPE_DEVICE))
#define CM_IS_DEVICE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE (      \
                                   (klass), CM_TYPE_DEVICE))
#define CM_DEVICE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS (            \
                                   (obj), CM_TYPE_DEVICE, CmDeviceClass))

GType device_get_type (void) G_GNUC_CONST;

struct _CmDevice
{
  /*< private >*/
  GObject parent_instance;
  CmDevicePrivate *priv;
};

struct _CmDeviceClass
{
  GObjectClass parent_class;
};

typedef enum
{
  DEVICE_ERROR_NO_MEMORY,
  DEVICE_ERROR_CONNMAN_INTERFACE,      /* Device interface does not exist */
  DEVICE_ERROR_CONNMAN_GET_PROPERTIES, /* GetProperties failed on Device */
} CmDeviceError;

#define CONNMAN_DEVICE_INTERFACE	CONNMAN_SERVICE ".Device"

typedef enum
{
  DEVICE_UNKNOWN = 0,
  DEVICE_WIFI = 1,
  DEVICE_WIMAX,
  DEVICE_BLUETOOTH,
  DEVICE_CELLULAR,
  DEVICE_ETHERNET,
} CmDeviceType;


CmDeviceType cm_device_get_type (const CmDevice *device);
const gchar *cm_device_type_to_string (CmDeviceType type);
GList *cm_device_get_networks (CmDevice *device);
void cm_device_free (CmDevice *device);
const gchar *cm_device_get_path (CmDevice *device);
gboolean cm_device_scan (CmDevice *device);
gboolean cm_device_is_same (const CmDevice *device, const gchar *path);
void cm_device_print (const CmDevice *device);
const gchar *cm_device_get_name (const CmDevice *device);
gboolean cm_device_is_scanning (const CmDevice *device);

G_END_DECLS

#endif /* __cm_device_h__ */
