#include <gtk/gtk.h>
#include <pthread.h>
#include <stdatomic.h>

// Global atomic variable to track drawing status
static atomic_int currently_drawing = ATOMIC_VAR_INIT(0);

// Function to perform drawing in a separate thread
void* do_draw(void* data) {
    // Your drawing logic goes here
    return NULL;
}

// Function to handle the timer execution
gboolean timer_exe(GtkWidget* window) {
    static gboolean first_execution = TRUE;

    // Use atomic operations to get the value of currently_drawing safely
    int drawing_status = atomic_load(&currently_drawing);

    // If not currently drawing, launch a thread to update the pixmap
    if (drawing_status == 0) {
        static pthread_t thread_info;
        if (!first_execution) {
            pthread_join(thread_info, NULL);
        }

        pthread_create(&thread_info, NULL, do_draw, NULL);
    }

    // Inform the window that it's time to draw the animation
    int width, height;
    gdk_drawable_get_size(pixmap, &width, &height);
    gtk_widget_queue_draw_area(window, 0, 0, width, height);

    first_execution = FALSE;

    return TRUE;
}
