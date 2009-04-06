#include <glib-object.h>
#include <glib.h>
#include <gconnman/gconnman.h>
#include <gtk/gtk.h>

typedef struct _Sample Sample;

struct _Sample
{
  GtkWidget *window;
  GtkWidget *notebook;
  Manager *manager;

  gint general_index;
  GtkWidget *general_label;
  GtkWidget *general_view;

  gint ethernet_index;
  GtkWidget *ethernet_label;
  GtkWidget *ethernet_view;

  gint wifi_index;
  GtkWidget *wifi_label;
  GtkWidget *wifi_view;
};

static gboolean window_delete_event (GtkWidget *widget, GdkEvent *event, 
                                     Sample *sample)
{
  return FALSE;
}

static void window_destroy(GtkWidget *widget, Sample *sample)
{
  gtk_main_quit();
}

/* sample_device_update
 *
 * A device may be updated at any time after the system is initialized.
 *
 * As such, sample_device_update is used to populate the set of devices
 * shown in the tab control.
 *
 */
static void
sample_device_update (Device *device, Sample *sample)
{
  DeviceType type = cm_device_get_type (device);

  switch (type)
  {
  case DEVICE_ETHERNET:
    if (sample->ethernet_index)
      break;
    sample->ethernet_label = gtk_button_new_with_label ("Ethernet");
    sample->ethernet_view = gtk_text_view_new ();
    gtk_text_view_set_editable (GTK_TEXT_VIEW (sample->ethernet_view), FALSE);
    gtk_widget_show (sample->ethernet_view);
    gtk_widget_show (sample->ethernet_label);
    sample->ethernet_index = gtk_notebook_append_page (
      GTK_NOTEBOOK (sample->notebook), 
      sample->ethernet_view, sample->ethernet_label);
    break;

  case DEVICE_WIFI:
    if (sample->wifi_index)
      break;
    sample->wifi_label = gtk_button_new_with_label ("Wifi");
    sample->wifi_view = gtk_text_view_new ();
    gtk_text_view_set_editable (GTK_TEXT_VIEW (sample->wifi_view), FALSE);
    gtk_widget_show (sample->wifi_view);
    gtk_widget_show (sample->wifi_label);
    sample->wifi_index = gtk_notebook_append_page (
      GTK_NOTEBOOK (sample->notebook), 
      sample->wifi_view, sample->wifi_label);
    break;

  default:
    break;
  }
}

static void
sample_manager_update (Manager *manager, Sample *sample)
{
  GList *devices;

  devices = cm_manager_get_devices (sample->manager);
  while (devices)
  {
    Device *device = devices->data;
    g_signal_connect (G_OBJECT (devices->data), "device-updated",
                      G_CALLBACK (sample_device_update), sample);
    sample_device_update (device, sample);
    devices = devices->next;
  }
}

static
void sample_manager_create (Sample *sample)
{
  if (sample->manager)
    return;

  sample->manager = cm_manager_new (NULL);
  if (!sample->manager)
    return;

  g_signal_connect (G_OBJECT (sample->manager), "manager-updated",
                    G_CALLBACK (sample_manager_update), sample);

  sample_manager_update (sample->manager, sample);
  cm_manager_refresh (sample->manager);
}


int main (int argc, char *argv[])
{
  Sample *sample;
  GError *error = NULL;

  g_thread_init (NULL); /* Needed by gconnman */
  gtk_init (&argc, &argv);

  sample = g_new0 (Sample, 1);

  sample->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (sample->window), "gconnman sample");
  gtk_widget_set_size_request (sample->window, 800, 480);

  g_signal_connect (G_OBJECT (sample->window), "delete_event", 
                    G_CALLBACK (window_delete_event), NULL);
  g_signal_connect (G_OBJECT (sample->window), "destroy", 
                    G_CALLBACK (window_destroy), NULL);

  sample->notebook = gtk_notebook_new ();
    
  sample->general_label = gtk_button_new_with_label ("General");
  sample->general_view = gtk_text_view_new ();
  gtk_text_view_set_editable (GTK_TEXT_VIEW (sample->general_view), FALSE);
  sample->general_index = gtk_notebook_append_page (
    GTK_NOTEBOOK (sample->notebook), 
    sample->general_view, sample->general_label);
  gtk_widget_show (sample->general_view);
  gtk_widget_show (sample->general_label);

  /* Creating the manager can result in multiple pages being added
   * to the notebook */
  sample_manager_create (sample);

  gtk_notebook_set_current_page (GTK_NOTEBOOK (sample->notebook),
                                 sample->general_index);

  gtk_container_add (GTK_CONTAINER (sample->window), sample->notebook);
  
  gtk_widget_show (sample->notebook);

  sample->manager = cm_manager_new (&error);

  gtk_widget_show (sample->window);

  gtk_main (); 

  g_object_unref (sample->manager);
  g_free (sample);

  return 0;
}
