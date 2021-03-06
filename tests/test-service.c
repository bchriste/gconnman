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
_strength_changed_cb (CmService *service,
                      gpointer   user_data)
{
  g_debug ("Service strength changed on %s, it's now %i",
           cm_service_get_name (service),
           cm_service_get_strength (service));
}

void
_security_changed_cb (CmService *service,
                      gpointer   user_data)
{
  g_debug ("Service security changed on %s, now using \"%s\" security",
           cm_service_get_name (service),
           cm_service_get_security (service));
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
  const GList *services;
  const GList *it;

  g_debug ("Services changed on manager\n");
  services = cm_manager_get_services (manager);
  if (!services)
  {
    g_debug ("Service list empty... :-(\n");
  }
  else
  {
    for (it = services; it != NULL; it = it->next)
    {
      CmService *service = it->data;

      g_signal_connect (G_OBJECT (service),
                        "service-updated",
                        G_CALLBACK (_service_updated_cb),
                        NULL);
      g_signal_connect (G_OBJECT (service),
                        "strength-changed",
                        G_CALLBACK (_strength_changed_cb),
                        NULL);
      g_signal_connect (G_OBJECT (service),
                        "security-changed",
                        G_CALLBACK (_security_changed_cb),
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

  manager = cm_manager_new (&error, FALSE);
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
