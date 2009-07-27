#include <glib-object.h>
#include <glib.h>
#include <gconnman/gconnman.h>

void
_offline_mode_changed_cb (CmManager *manager,
                          gpointer   user_data)
{
  g_debug ("Offline mode changed, it's now: %i",
           cm_manager_get_offline_mode (manager));
}

void
_manager_updated_cb (CmManager *manager,
                     gpointer user_data)
{
  g_debug ("Manager updated\n");
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
                      "offline-mode-changed",
                      G_CALLBACK (_offline_mode_changed_cb),
                      NULL);
    cm_manager_refresh (manager);
  }

  loop = g_main_loop_new (NULL,
                          FALSE);
  g_main_loop_run (loop);

  return 0;
}
