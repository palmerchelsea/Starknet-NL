#include <gtk/gtk.h>
#include <pthread.h>

static GdkPixmap *pixmap = NULL;
static int currently_drawing = 0;

// Forward declarations
gboolean on_window_configure_event(GtkWidget *da, GdkEventConfigure *event, gpointer user_data);
gboolean on_window_expose_event(GtkWidget *da, GdkEventExpose *event, gpointer user_data);
void *do_draw(void *ptr);
gboolean timer_exe(GtkWidget *window);

int main(int argc, char *argv[]) {
    // Initialize GTK and ensure thread awareness
    if (!g_thread_supported()) {
        g_thread_init(NULL);
    }
    gdk_threads_init();
    gdk_threads_enter();

    gtk_init(&argc, &argv);

    // Create GTK window
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    g_signal_connect(G_OBJECT(window), "destroy", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(G_OBJECT(window), "expose_event", G_CALLBACK(on_window_expose_event), NULL);
    g_signal_connect(G_OBJECT(window), "configure_event", G_CALLBACK(on_window_configure_event), NULL);

    // Show the window
    gtk_widget_show_all(window);

    // Set up the pixmap for drawing
    pixmap = gdk_pixmap_new(window->window, 500, 500, -1);
    gtk_widget_set_app_paintable(window, TRUE);
    gtk_widget_set_double_buffered(window, FALSE);

    // Set up a timer for periodic execution
    (void)g_timeout_add(33, (GSourceFunc)timer_exe, window);

    // Start GTK main loop
    gtk_main();
    gdk_threads_leave();

    return 0;
}

// Handle window configure events
gboolean on_window_configure_event(GtkWidget *da, GdkEventConfigure *event, gpointer user_data) {
    static int oldw = 0;
    static int oldh = 0;

    // Resize the pixmap if the window size has changed
    if (oldw != event->width || oldh != event->height) {
        GdkPixmap *tmppixmap = gdk_pixmap_new(da->window, event->width, event->height, -1);
        int minw = oldw, minh = oldh;
        if (event->width < minw) {
            minw = event->width;
        }
        if (event->height < minh) {
            minh = event->height;
        }
        gdk_draw_drawable(tmppixmap, da->style->fg_gc[GTK_WIDGET_STATE(da)], pixmap, 0, 0, 0, 0, minw, minh);
        g_object_unref(pixmap);
        pixmap = tmppixmap;
    }

    oldw = event->width;
    oldh = event->height;
    return TRUE;
}

// Handle window expose events
gboolean on_window_expose_event(GtkWidget *da, GdkEventExpose *event, gpointer user_data) {
    gdk_draw_drawable(da->window,
                      da->style->fg_gc[GTK_WIDGET_STATE(da)], pixmap,
                      event->area.x, event->area.y,
                      event->area.x, event->area.y,
                      event->area.width, event->area.height);
    return TRUE;
}

// Thread function to perform drawing
void *do_draw(void *ptr) {
    currently_drawing = 1;

    int width, height;
    gdk_threads_enter();
    gdk_drawable_get_size(pixmap, &width, &height);
    gdk_threads_leave();

    cairo_surface_t *cst = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    cairo_t *cr = cairo_create(cst);

    // Time-consuming drawing
    static int i = 0;
    ++i;
    i = i % 300; // Give a little movement to our animation
    cairo_set_source_rgb(cr, 0.9, 0.9, 0.9);
    cairo_paint(cr);

    int j, k;
    for (k = 0; k < 100; ++k) {
        for (j = 0; j < 1000; ++j) {
            cairo_set_source_rgb(cr, (double)j / 1000.0, (double)j / 1000.0, 1.0 - (double)j / 1000.0);
            cairo_move_to(cr, i, j / 2);
            cairo_line_to(cr, i + 100, j / 2);
            cairo_stroke(cr);
        }
    }

    cairo_destroy(cr);

    gdk_threads_enter();
    cairo_t *cr_pixmap = gdk_cairo_create(pixmap);
    cairo_set_source_surface(cr_pixmap, cst, 0, 0);
    cairo_paint(cr_pixmap);
    cairo_destroy(cr_pixmap);
    gdk_threads_leave();

    cairo_surface_destroy(cst);

    currently_drawing = 0;

    return NULL;
}

// Timer function for periodic execution
gboolean timer_exe(GtkWidget *window) {
    static gboolean first_execution = TRUE;

    int drawing_status = g_atomic_int_get(&currently_drawing);

    if (drawing_status == 0) {
        static pthread_t thread_info;
        int iret;
        if (first_execution != TRUE) {
            pthread_join(thread_info, NULL);
        }
        iret = pthread_create(&thread_info, NULL, do_draw, NULL);
    }

    int width, height;
    gdk_drawable_get_size(pixmap, &width, &height);
    gtk_widget_queue_draw_area(window, 0, 0, width, height);

    first_execution = FALSE;

    return TRUE;
}
