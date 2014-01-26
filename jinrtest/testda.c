#include <stdio.h>
#include <gtk/gtk.h>

static gboolean
checkerboard_draw (GtkWidget *da,
                   cairo_t   *cr,
                   gpointer   data)
{
   guint width, height;
  GdkRGBA color;

  width = gtk_widget_get_allocated_width (da);
  height = gtk_widget_get_allocated_height (da);
  cairo_arc (cr,
             width / 2.0, height / 2.0,
             MIN (width, height) / 2.0,
             0, 2 * G_PI);

  gtk_style_context_get_color (gtk_widget_get_style_context (da),
                               0,
                               &color);
  gdk_cairo_set_source_rgba (cr, &color);

  cairo_fill (cr);

 return FALSE;
}

int main(int argc, char **argv)
{
	GtkWidget *window, *vbox, *da, *frame;
	
	gtk_init (&argc, &argv);		

    /* Create a new window */
    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

    gtk_window_set_title (GTK_WINDOW (window), "TDPAC BYK JINR");
    gtk_widget_set_size_request (window, 1300, 500);

    /* It's a good idea to do this for all windows. */
    g_signal_connect (G_OBJECT (window), "destroy",
                        G_CALLBACK (gtk_main_quit), NULL);
                        
	gtk_container_set_border_width (GTK_CONTAINER (window), 8);

	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 8);
	gtk_container_add (GTK_CONTAINER (window), vbox);
	
	 frame = gtk_frame_new (NULL);
      gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
      gtk_box_pack_start (GTK_BOX (vbox), frame, TRUE, TRUE, 0);

      da = gtk_drawing_area_new ();
      /* set a minimum size */
      gtk_widget_set_size_request (da, 100, 100);
      
            g_signal_connect (da, "draw",
                        G_CALLBACK (checkerboard_draw), NULL);

      gtk_container_add (GTK_CONTAINER (frame), da);
      
         gtk_widget_show_all (window);
    gtk_main ();
	
	return 0;
}

