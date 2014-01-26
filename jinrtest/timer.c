#include <cairo.h>
#include <gtk/gtk.h>
#include <time.h>

static char buffer[256];

static gboolean
time_handler(GtkWidget *widget)
{
  time_t curtime;
  struct tm *loctime;

  curtime = time(NULL);
  loctime = localtime(&curtime);
  strftime(buffer, 256, "%T", loctime);

  printf("buff = %s\n", buffer);
  
  return TRUE;
}

int
main (int argc, char *argv[])
{

  GtkWidget *window;
  GtkWidget *darea;

  gtk_init(&argc, &argv);

  window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

  darea = gtk_drawing_area_new();
  gtk_container_add(GTK_CONTAINER (window), darea);

  g_signal_connect(window, "destroy",
      G_CALLBACK(gtk_main_quit), NULL);

  gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
  gtk_window_set_default_size(GTK_WINDOW(window), 170, 100);

  gtk_window_set_title(GTK_WINDOW(window), "timer");
  g_timeout_add(1000, (GSourceFunc) time_handler, (gpointer) window);
  gtk_widget_show_all(window);
  time_handler(window);

  gtk_main();

  return 0;
}

in online cb
g_timeout_add(5000, (GSourceFunc) get_online_data_cycle, (gpointer) data);

static gboolean
time_handler(GtkWidget *widget)
{
	get data code

	return TRUE;
}
