// NOTE: https://docs.gtk.org/gtk3/index.html

#include <memory>
#include <algorithm>
#include <string>

#include <string.h>

#include <gtk/gtk.h>
#include <gst/gst.h>
#include <gst/video/videooverlay.h>

#include <gdk/gdk.h>
#if defined(GDK_WINDOWING_X11)
#include <gdk/gdkx.h>
#elif defined(GDK_WINDOWING_WIN32)
#include <gdk/gdkwin32.h>
#elif defined(GDK_WINDOWING_QUARTZ)
#include <gdk/gdkquartz.h>
#endif

struct Application {

  Application ()
  {
    playbin = gst_element_factory_make ("playbin", "playbin");
    g_assert_nonnull (playbin);
    g_object_set (playbin, "uri", "https://www.freedesktop.org/software/gstreamer-sdk/data/media/sintel_trailer-480p.webm", nullptr);

    auto const handle_tags_changed = +[] (GstElement* playbin, gint stream, Application* application) {
      g_assert (application && application->playbin == playbin);
      gst_element_post_message (playbin, gst_message_new_application (GST_OBJECT (playbin), gst_structure_new_empty ("tags-changed")));
    };
    g_signal_connect (G_OBJECT (playbin), "video-tags-changed", G_CALLBACK (handle_tags_changed), this);
    g_signal_connect (G_OBJECT (playbin), "audio-tags-changed", G_CALLBACK (handle_tags_changed), this);
    g_signal_connect (G_OBJECT (playbin), "text-tags-changed", G_CALLBACK (handle_tags_changed), this);
  }
  ~Application ()
  {
    if (playbin) {
      gst_element_set_state (playbin, GST_STATE_NULL);
      gst_object_unref (std::exchange (playbin, nullptr));
    }
  }

  void handle_video_realize (GtkWidget* widget)
  {
    GdkWindow* window = gtk_widget_get_window (widget);
    g_assert (gdk_window_ensure_native (window));

    guintptr window_handle;
#if defined(GDK_WINDOWING_WIN32)
    window_handle = (guintptr) GDK_WINDOW_HWND (window);
#elif defined(GDK_WINDOWING_QUARTZ)
    window_handle = gdk_quartz_window_get_nsview (window);
#elif defined(GDK_WINDOWING_X11)
    window_handle = GDK_WINDOW_XID (window);
#endif

    gst_video_overlay_set_window_handle (GST_VIDEO_OVERLAY (playbin), window_handle);
  }
  gboolean handle_video_draw (GtkWidget* widget, cairo_t* cr)
  {
    if (state < GST_STATE_PAUSED) {
      GtkAllocation allocation;
      gtk_widget_get_allocation (widget, &allocation);
      cairo_set_source_rgb (cr, 0, 0, 0);
      cairo_rectangle (cr, 0, 0, allocation.width, allocation.height);
      cairo_fill (cr);
    }
    return FALSE;
  }

  void create_ui ()
  {
    GtkWidget* main_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    g_signal_connect (G_OBJECT (main_window), "delete-event", G_CALLBACK (+[] (GtkWidget* widget, GdkEvent* event, Application* application) -> void {
      g_assert (application);
      g_assert (application->playbin);
      gst_element_set_state (application->playbin, GST_STATE_READY);
      gtk_main_quit ();
    }),
        this);

    GtkWidget* video_window = gtk_drawing_area_new ();
    // gtk_widget_set_double_buffered (video_window, FALSE); -- https://docs.gtk.org/gtk3/method.Widget.set_double_buffered.html
    g_signal_connect (video_window, "realize", G_CALLBACK (+[] (GtkWidget* widget, Application* application) -> void { application->handle_video_realize (widget); }), this);
    g_signal_connect (video_window, "draw", G_CALLBACK (+[] (GtkWidget* widget, cairo_t* cr, Application* application) -> gboolean { return application->handle_video_draw (widget, cr); }), this);

    GtkWidget* play_button = gtk_button_new_from_icon_name ("media-playback-start", GTK_ICON_SIZE_SMALL_TOOLBAR);
    g_signal_connect (G_OBJECT (play_button), "clicked", G_CALLBACK (+[] (GtkButton* button, Application* application) -> void {
      g_assert (application);
      g_assert (application->playbin);
      gst_element_set_state (application->playbin, GST_STATE_PLAYING);
    }),
        this);

    GtkWidget* pause_button = gtk_button_new_from_icon_name ("media-playback-pause", GTK_ICON_SIZE_SMALL_TOOLBAR);
    g_signal_connect (G_OBJECT (pause_button), "clicked", G_CALLBACK (+[] (GtkButton* button, Application* application) -> void {
      g_assert (application);
      g_assert (application->playbin);
      gst_element_set_state (application->playbin, GST_STATE_PAUSED);
    }),
        this);

    GtkWidget* stop_button = gtk_button_new_from_icon_name ("media-playback-stop", GTK_ICON_SIZE_SMALL_TOOLBAR);
    g_signal_connect (G_OBJECT (stop_button), "clicked", G_CALLBACK (+[] (GtkButton* button, Application* application) -> void {
      g_assert (application);
      g_assert (application->playbin);
      gst_element_set_state (application->playbin, GST_STATE_READY);
    }),
        this);

    slider = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 0, 100, 1);
    gtk_scale_set_draw_value (GTK_SCALE (slider), 0);
    slider_update_signal_id = g_signal_connect (G_OBJECT (slider), "value-changed", G_CALLBACK (+[] (GtkRange* range, Application* application) -> void {
      gdouble value = gtk_range_get_value (GTK_RANGE (application->slider));
      gst_element_seek_simple (application->playbin, GST_FORMAT_TIME, (GstSeekFlags) (GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT), static_cast<gint64> (value * GST_SECOND));
    }),
        this);

    GtkWidget* controls = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start (GTK_BOX (controls), play_button, FALSE, FALSE, 2);
    gtk_box_pack_start (GTK_BOX (controls), pause_button, FALSE, FALSE, 2);
    gtk_box_pack_start (GTK_BOX (controls), stop_button, FALSE, FALSE, 2);
    gtk_box_pack_start (GTK_BOX (controls), slider, TRUE, TRUE, 2);

    GtkWidget* main_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_pack_start (GTK_BOX (main_box), video_window, TRUE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (main_box), controls, FALSE, FALSE, 0);
    gtk_container_add (GTK_CONTAINER (main_window), main_box);
    gtk_window_set_default_size (GTK_WINDOW (main_window), 720, 480);
    gtk_widget_show_all (main_window);
  }
  gboolean refresh_ui ()
  {
    gint64 current = -1;
    if (state < GST_STATE_PAUSED)
      return TRUE;
    if (!GST_CLOCK_TIME_IS_VALID (duration)) {
      if (!gst_element_query_duration (playbin, GST_FORMAT_TIME, &duration)) {
        g_printerr ("Could not query current duration.\n");
      } else {
        gtk_range_set_range (GTK_RANGE (slider), 0, (gdouble) duration / GST_SECOND);
      }
    }
    if (gst_element_query_position (playbin, GST_FORMAT_TIME, &current)) {
      g_signal_handler_block (slider, slider_update_signal_id);
      gtk_range_set_value (GTK_RANGE (slider), (gdouble) current / GST_SECOND);
      g_signal_handler_unblock (slider, slider_update_signal_id);
    }
    return TRUE;
  }

  static std::string to_string (gchar*& input, char const* alternate_input = nullptr)
  {
    std::string output;
    if (input) {
      output = input;
      g_free (std::exchange (input, nullptr));
    } else
      output = alternate_input;
    return output;
  }

  void handle_bus_error_message (GstBus* bus, GstMessage* message)
  {
    g_assert_nonnull (bus);
    g_assert_nonnull (message);
    GError* err = nullptr;
    gchar* debug_info = nullptr;
    gst_message_parse_error (message, &err, &debug_info);
    g_printerr ("Error received from element %s: %s\n", GST_OBJECT_NAME (message->src), err->message);
    g_printerr ("Debugging information: %s\n", debug_info ? debug_info : "none");
    g_clear_error (&err);
    g_free (debug_info);
    gst_element_set_state (playbin, GST_STATE_READY);
  }
  void handle_bus_eos_message (GstBus* bus, GstMessage* message)
  {
    g_assert_nonnull (bus);
    g_assert_nonnull (message);
    g_print ("End-Of-Stream reached.\n");
    gst_element_set_state (playbin, GST_STATE_READY);
  }
  void handle_bus_state_changed_message (GstBus* bus, GstMessage* message)
  {
    g_assert_nonnull (bus);
    g_assert_nonnull (message);
    GstState old_state, new_state, pending_state;
    gst_message_parse_state_changed (message, &old_state, &new_state, &pending_state);
    if (GST_MESSAGE_SRC (message) == GST_OBJECT (playbin)) {
      state = new_state;
      g_print ("State set to %s\n", gst_element_state_get_name (new_state));
      if (old_state == GST_STATE_READY && new_state == GST_STATE_PAUSED)
        refresh_ui ();
    }
  }
  void handle_bus_application_message (GstBus* bus, GstMessage* message)
  {
    g_assert_nonnull (bus);
    g_assert_nonnull (message);
    if (g_strcmp0 (gst_structure_get_name (gst_message_get_structure (message)), "tags-changed") != 0)
      return;

    gint n_video, n_audio, n_text;
    g_object_get (playbin, "n-video", &n_video, "n-audio", &n_audio, "n-text", &n_text, NULL);

    for (gint index = 0; index < n_video; index++) {
      GstTagList* tags = nullptr;
      g_signal_emit_by_name (playbin, "get-video-tags", index, &tags);
      if (!tags)
        continue;
      gchar* codec;
      gst_tag_list_get_string (tags, GST_TAG_VIDEO_CODEC, &codec);
      g_print ("video %d: %s\n", index, to_string (codec, "unknown").c_str ());
      gst_tag_list_free (tags);
    }

    for (gint index = 0; index < n_audio; index++) {
      GstTagList* tags = nullptr;
      g_signal_emit_by_name (playbin, "get-audio-tags", index, &tags);
      if (!tags)
        continue;
      gchar* codec = nullptr;
      gst_tag_list_get_string (tags, GST_TAG_AUDIO_CODEC, &codec);
      gchar* language_code = nullptr;
      gst_tag_list_get_string (tags, GST_TAG_LANGUAGE_CODE, &language_code);
      guint rate = 0;
      gst_tag_list_get_uint (tags, GST_TAG_BITRATE, &rate);
      g_print ("audio %d: %s, language %s, rate %u\n", index, to_string (codec, "unknown").c_str (), to_string (language_code, "unknown").c_str (), rate);
      gst_tag_list_free (tags);
    }

    for (gint index = 0; index < n_text; index++) {
      GstTagList* tags = nullptr;
      g_signal_emit_by_name (playbin, "get-text-tags", index, &tags);
      if (!tags)
        continue;
      gchar* language_code = nullptr;
      gst_tag_list_get_string (tags, GST_TAG_LANGUAGE_CODE, &language_code);
      g_print ("text %d: language %s\n", index, to_string (language_code, "unknown").c_str ());
      gst_tag_list_free (tags);
    }
  }

  GstElement* playbin = nullptr;
  GtkWidget* slider = nullptr;
  gulong slider_update_signal_id;
  GstState state;
  gint64 duration = GST_CLOCK_TIME_NONE;
};

int main (int argc, char* argv[])
{
  gtk_init (&argc, &argv);
  gst_init (&argc, &argv);

  Application application {};
  application.create_ui ();
  {
    GstBus* bus = gst_element_get_bus (application.playbin);
    gst_bus_add_signal_watch (bus); // register a function that receives the message which we are interested in posted to the GStreamer bus.
    g_signal_connect (G_OBJECT (bus), "message::error", G_CALLBACK (+[] (GstBus* bus, GstMessage* message, Application* application) -> void { application->handle_bus_error_message (bus, message); }), &application);
    g_signal_connect (G_OBJECT (bus), "message::eos", G_CALLBACK (+[] (GstBus* bus, GstMessage* message, Application* application) -> void { application->handle_bus_eos_message (bus, message); }), &application);
    g_signal_connect (G_OBJECT (bus), "message::state-changed", G_CALLBACK (+[] (GstBus* bus, GstMessage* message, Application* application) -> void { application->handle_bus_state_changed_message (bus, message); }), &application);
    g_signal_connect (G_OBJECT (bus), "message::application", G_CALLBACK (+[] (GstBus* bus, GstMessage* message, Application* application) -> void { application->handle_bus_application_message (bus, message); }), &application);
    gst_object_unref (bus);
  }
  GstStateChangeReturn set_state_result = gst_element_set_state (application.playbin, GST_STATE_PLAYING);
  g_assert (set_state_result != GST_STATE_CHANGE_FAILURE);
  g_timeout_add_seconds (1, G_SOURCE_FUNC (+[] (Application* application) -> gboolean { return application->refresh_ui (); }), &application);

  gtk_main ();
  return 0;
}
