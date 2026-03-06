#include <gtk/gtk.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>

pid_t ffmpeg_pid = 0;

GtkWidget *file_entry; 
GtkWidget *res_combo;
GtkWidget *fps_combo;
GtkWidget *encoder_combo;
GtkWidget *quality_spin;
GtkWidget *audio_bitrate_combo;
GtkWidget *start_btn;
GtkWidget *stop_btn;

// Timelapse widgets
GtkWidget *timelapse_check;
GtkWidget *timelapse_speed_spin;
GtkWidget *timelapse_box;

void on_browse_clicked(GtkWidget *widget, gpointer window) {
    GtkWidget *dialog = gtk_file_chooser_dialog_new("Save Output File",
                                  GTK_WINDOW(window),
                                  GTK_FILE_CHOOSER_ACTION_SAVE,
                                  "_Cancel", GTK_RESPONSE_CANCEL,
                                  "_Save", GTK_RESPONSE_ACCEPT,
                                  NULL);
    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog), TRUE);
    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), "output.mp4");
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        gtk_entry_set_text(GTK_ENTRY(file_entry), filename);
        g_free(filename);
    }
    gtk_widget_destroy(dialog);
}

void on_timelapse_toggled(GtkToggleButton *toggle, gpointer data) {
    gboolean active = gtk_toggle_button_get_active(toggle);
    gtk_widget_set_sensitive(timelapse_box, active);
}

void start_recording(GtkWidget *widget, gpointer data) {
    if (ffmpeg_pid == 0) {
        const gchar *filename_const = gtk_entry_get_text(GTK_ENTRY(file_entry));
        if (!filename_const || strlen(filename_const) == 0) {
            g_print("Error: Please select or type an output file location first.\n");
            return;
        }

        gchar *filename       = g_strdup(filename_const);
        gchar *resolution     = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(res_combo));
        gchar *fps            = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(fps_combo));
        gchar *encoder_choice = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(encoder_combo));
        gchar *audio_bitrate  = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(audio_bitrate_combo));

        int quality_val = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(quality_spin));
        char quality_str[16];
        snprintf(quality_str, sizeof(quality_str), "%d", quality_val);

        // --- Timelapse logic ---
        // Capture 1 frame every N seconds using fractional framerate on the INPUT.
        // Output is forced to 30fps. No audio. Nearly zero CPU.
        gboolean is_timelapse = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(timelapse_check));
        int timelapse_speed   = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(timelapse_speed_spin));

        char capture_fps_str[16];
        if (is_timelapse) {
            snprintf(capture_fps_str, sizeof(capture_fps_str), "1/%d", timelapse_speed);
            g_print("Timelapse: capturing at %s fps → output 30 fps (%dx speed-up)\n",
                    capture_fps_str, timelapse_speed * 30);
        }

        char *video_codec = "libx264";
        if      (g_strcmp0(encoder_choice, "NVIDIA (h264_nvenc)") == 0)   video_codec = "h264_nvenc";
        else if (g_strcmp0(encoder_choice, "Intel (h264_qsv)") == 0)      video_codec = "h264_qsv";
        else if (g_strcmp0(encoder_choice, "AMD/Open (h264_vaapi)") == 0) video_codec = "h264_vaapi";

        ffmpeg_pid = fork();

        if (ffmpeg_pid == 0) {
            const char *args[50];
            int i = 0;

            args[i++] = "ffmpeg";
            args[i++] = "-y";

            if (strcmp(video_codec, "h264_vaapi") == 0) {
                args[i++] = "-vaapi_device";
                args[i++] = "/dev/dri/renderD128";
            }

            // VIDEO INPUT
            // In timelapse mode: x11grab captures at 1/N fps — barely any CPU work
            args[i++] = "-thread_queue_size"; args[i++] = "512";
            args[i++] = "-video_size";        args[i++] = resolution;
            args[i++] = "-framerate";         args[i++] = is_timelapse ? capture_fps_str : fps;
            args[i++] = "-f";                 args[i++] = "x11grab";
            args[i++] = "-i";                 args[i++] = ":0.0";

            // AUDIO INPUT — skip entirely for timelapse
            if (!is_timelapse) {
                args[i++] = "-thread_queue_size"; args[i++] = "512";
                args[i++] = "-f";                 args[i++] = "pulse";
                args[i++] = "-i";                 args[i++] = "default";
            }

            if (strcmp(video_codec, "h264_vaapi") == 0) {
                args[i++] = "-vf";
                args[i++] = "format=nv12,hwupload";
            }

            // Force output framerate to 30fps in timelapse so frames pack tightly
            if (is_timelapse) {
                args[i++] = "-r"; args[i++] = "30";
            }

            args[i++] = "-c:v"; args[i++] = video_codec;

            if (strcmp(video_codec, "libx264") == 0) {
                args[i++] = "-preset"; args[i++] = "ultrafast";
                args[i++] = "-crf";    args[i++] = quality_str;
            } else if (strcmp(video_codec, "h264_nvenc") == 0) {
                args[i++] = "-preset"; args[i++] = "p4";
                args[i++] = "-cq";     args[i++] = quality_str;
            } else {
                args[i++] = "-global_quality"; args[i++] = quality_str;
            }

            // AUDIO OUTPUT — disabled for timelapse
            if (!is_timelapse) {
                args[i++] = "-c:a"; args[i++] = "aac";
                args[i++] = "-b:a"; args[i++] = audio_bitrate;
            } else {
                args[i++] = "-an";
            }

            args[i++] = "-movflags"; args[i++] = "+faststart";
            args[i++] = filename;
            args[i++] = NULL;

            execvp("ffmpeg", (char *const *)args);
            perror("execvp failed");
            exit(1);
        }

        gtk_widget_set_sensitive(start_btn, FALSE);
        gtk_widget_set_sensitive(stop_btn, TRUE);

        g_free(filename);
        g_free(resolution);
        g_free(fps);
        g_free(encoder_choice);
        g_free(audio_bitrate);
    }
}

void stop_recording(GtkWidget *widget, gpointer data) {
    if (ffmpeg_pid != 0) {
        kill(ffmpeg_pid, SIGTERM);
        ffmpeg_pid = 0;
        gtk_widget_set_sensitive(start_btn, TRUE);
        gtk_widget_set_sensitive(stop_btn, FALSE);
        g_print("Recording stopped.\n");
    }
}

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "ProCapture - FFmpeg Recorder");
    gtk_window_set_default_size(GTK_WINDOW(window), 450, 500);
    gtk_container_set_border_width(GTK_CONTAINER(window), 15);

    GtkWidget *main_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 15);
    gtk_container_add(GTK_CONTAINER(window), main_vbox);

    // FRAME 1: OUTPUT DESTINATION
    GtkWidget *frame_output = gtk_frame_new("Output Destination");
    gtk_box_pack_start(GTK_BOX(main_vbox), frame_output, FALSE, FALSE, 0);
    GtkWidget *grid_output = gtk_grid_new();
    gtk_container_set_border_width(GTK_CONTAINER(grid_output), 10);
    gtk_grid_set_column_spacing(GTK_GRID(grid_output), 10);
    gtk_container_add(GTK_CONTAINER(frame_output), grid_output);

    file_entry = gtk_entry_new();
    gtk_widget_set_hexpand(file_entry, TRUE);
    GtkWidget *browse_btn = gtk_button_new_with_label("Browse...");
    g_signal_connect(browse_btn, "clicked", G_CALLBACK(on_browse_clicked), window);
    gtk_grid_attach(GTK_GRID(grid_output), gtk_label_new("Path:"), 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid_output), file_entry,             1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid_output), browse_btn,             2, 0, 1, 1);

    // FRAME 2: VIDEO SETTINGS
    GtkWidget *frame_video = gtk_frame_new("Video Settings");
    gtk_box_pack_start(GTK_BOX(main_vbox), frame_video, FALSE, FALSE, 0);
    GtkWidget *grid_video = gtk_grid_new();
    gtk_container_set_border_width(GTK_CONTAINER(grid_video), 10);
    gtk_grid_set_row_spacing(GTK_GRID(grid_video), 10);
    gtk_grid_set_column_spacing(GTK_GRID(grid_video), 10);
    gtk_container_add(GTK_CONTAINER(frame_video), grid_video);

    res_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(res_combo), "1920x1080");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(res_combo), "2560x1440");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(res_combo), "1280x720");
    gtk_combo_box_set_active(GTK_COMBO_BOX(res_combo), 0);

    fps_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(fps_combo), "30");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(fps_combo), "60");
    gtk_combo_box_set_active(GTK_COMBO_BOX(fps_combo), 1);

    encoder_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(encoder_combo), "Software (CPU - libx264)");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(encoder_combo), "NVIDIA (h264_nvenc)");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(encoder_combo), "Intel (h264_qsv)");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(encoder_combo), "AMD/Open (h264_vaapi)");
    gtk_combo_box_set_active(GTK_COMBO_BOX(encoder_combo), 0);

    quality_spin = gtk_spin_button_new_with_range(0.0, 51.0, 1.0);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(quality_spin), 23.0);
    GtkWidget *quality_hint = gtk_label_new("(Lower = Better Quality. Default: 23)");
    gtk_widget_set_halign(quality_hint, GTK_ALIGN_START);
    gtk_style_context_add_class(gtk_widget_get_style_context(quality_hint), "dim-label");

    gtk_grid_attach(GTK_GRID(grid_video), gtk_label_new("Resolution:"),    0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid_video), res_combo,                        1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid_video), gtk_label_new("Framerate:"),     0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid_video), fps_combo,                        1, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid_video), gtk_label_new("Encoder:"),       0, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid_video), encoder_combo,                    1, 2, 2, 1);
    gtk_grid_attach(GTK_GRID(grid_video), gtk_label_new("Quality (CRF):"), 0, 3, 1, 1);
    gtk_grid_attach(GTK_GRID(grid_video), quality_spin,                     1, 3, 1, 1);
    gtk_grid_attach(GTK_GRID(grid_video), quality_hint,                     2, 3, 1, 1);

    // FRAME 3: AUDIO SETTINGS
    GtkWidget *frame_audio = gtk_frame_new("Audio Settings");
    gtk_box_pack_start(GTK_BOX(main_vbox), frame_audio, FALSE, FALSE, 0);
    GtkWidget *grid_audio = gtk_grid_new();
    gtk_container_set_border_width(GTK_CONTAINER(grid_audio), 10);
    gtk_grid_set_column_spacing(GTK_GRID(grid_audio), 10);
    gtk_container_add(GTK_CONTAINER(frame_audio), grid_audio);

    audio_bitrate_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(audio_bitrate_combo), "128k");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(audio_bitrate_combo), "192k");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(audio_bitrate_combo), "256k");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(audio_bitrate_combo), "320k");
    gtk_combo_box_set_active(GTK_COMBO_BOX(audio_bitrate_combo), 1);
    gtk_grid_attach(GTK_GRID(grid_audio), gtk_label_new("Bitrate:"), 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid_audio), audio_bitrate_combo,        1, 0, 1, 1);

    // FRAME 4: TIMELAPSE
    GtkWidget *frame_timelapse = gtk_frame_new("Timelapse");
    gtk_box_pack_start(GTK_BOX(main_vbox), frame_timelapse, FALSE, FALSE, 0);

    GtkWidget *tl_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(tl_vbox), 10);
    gtk_container_add(GTK_CONTAINER(frame_timelapse), tl_vbox);

    timelapse_check = gtk_check_button_new_with_label("Enable Timelapse Mode");
    gtk_box_pack_start(GTK_BOX(tl_vbox), timelapse_check, FALSE, FALSE, 0);

    // Speed row — greyed out until checkbox is ticked
    timelapse_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_sensitive(timelapse_box, FALSE);
    gtk_box_pack_start(GTK_BOX(tl_vbox), timelapse_box, FALSE, FALSE, 0);

    timelapse_speed_spin = gtk_spin_button_new_with_range(1.0, 60.0, 1.0);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(timelapse_speed_spin), 5.0);

    GtkWidget *suffix = gtk_label_new("second(s) between frames  →  30 fps output, no audio");
    gtk_style_context_add_class(gtk_widget_get_style_context(suffix), "dim-label");

    gtk_box_pack_start(GTK_BOX(timelapse_box), gtk_label_new("1 frame every"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(timelapse_box), timelapse_speed_spin,            FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(timelapse_box), suffix,                          FALSE, FALSE, 0);

    g_signal_connect(timelapse_check, "toggled", G_CALLBACK(on_timelapse_toggled), NULL);

    // ACTION BUTTONS
    start_btn = gtk_button_new_with_label("▶ Start Recording");
    stop_btn  = gtk_button_new_with_label("⏹ Stop Recording");
    gtk_widget_set_sensitive(stop_btn, FALSE);
    gtk_widget_set_size_request(start_btn, -1, 40);
    gtk_widget_set_size_request(stop_btn,  -1, 40);

    g_signal_connect(start_btn, "clicked", G_CALLBACK(start_recording), NULL);
    g_signal_connect(stop_btn,  "clicked", G_CALLBACK(stop_recording),  NULL);
    g_signal_connect(window,    "destroy", G_CALLBACK(gtk_main_quit),   NULL);

    GtkWidget *btn_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 15);
    gtk_box_set_homogeneous(GTK_BOX(btn_box), TRUE);
    gtk_box_pack_start(GTK_BOX(btn_box), start_btn, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(btn_box), stop_btn,  TRUE, TRUE, 0);
    gtk_box_pack_end(GTK_BOX(main_vbox), btn_box, FALSE, FALSE, 0);

    gtk_widget_show_all(window);
    gtk_main();

    return 0;
}