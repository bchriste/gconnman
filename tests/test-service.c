#include <glib-object.h>
#include <glib.h>
#include <gconnman/gconnman.h>

void
_pretty_print_service (CmService *service)
{
  g_debug ("Service %s found of type %s and state %s\nPath is %s\n",
           cm_service_get_name (service),
           cm_service_get_type (service),
           cm_service_get_state (service),
           cm_service_get_path (service));
}

void
_manager_updated_cb (CmManager *manager,
                     gpointer user_data)
{
  g_debug ("Manager updated\n");
}

void
_service_state_changed_cb (CmService *service,
                           gpointer   user_data)
{
  g_debug ("Service state changed\nState now %s",
           cm_service_get_state (service));
}

void
_service_updated_cb (CmService *service,
                     gpointer user_data)
{
  g_debug ("Service updated\n");
  _pretty_print_service (service);
  g_signal_handlers_disconnect_by_func (G_OBJECT (service),
                                        G_CALLBACK (_service_updated_cb),
                                        user_data);
  g_signal_connect (G_OBJECT (service),
                    "state-changed",
                    G_CALLBACK (_service_state_changed_cb),
                    NULL);
}

void
_services_changed_cb (CmManager *manager,
                      gpointer user_data)
{
  GList *services;
  int i;

  g_debug ("Services changed on manager\n");
  services = cm_manager_get_services (manager);
  if (!services)
  {
    g_debug ("Service list empty... :-(\n");
  }
  else
  {
    for (i = 0; i < g_list_length (services); i++)
    {
      CmService *service = CM_SERVICE (g_list_nth (services,
                                                   i)->data);
      g_signal_connect (G_OBJECT (service),
                        "service-updated",
                        G_CALLBACK (_service_updated_cb),
                        NULL);
    }
  }
}

int
main (int    argc,
      char **argv)
{
  CmManager *manager;
  GList *services;
  GError *error = NULL;
  GMainLoop *loop;

  g_type_init ();

  manager = cm_manager_new (&error);
  if (error)
  {
    g_debug ("Error initialising manager: %s\n",
             error->message);
    g_clear_error (&error);

    return -1;
  }

  if (manager)
  {
    g_debug ("Got Manager interface\n");

    g_signal_connect (G_OBJECT (manager),
                      "manager-updated",
                      G_CALLBACK (_manager_updated_cb),
                      NULL);
    g_signal_connect (G_OBJECT (manager),
                      "services-changed",
                      G_CALLBACK (_services_changed_cb),
                      NULL);
    cm_manager_refresh (manager);
  }

  loop = g_main_loop_new (NULL,
                          FALSE);
  g_main_loop_run (loop);

  return 0;
}
