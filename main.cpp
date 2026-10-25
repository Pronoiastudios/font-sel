#include <gtk/gtk.h>

#include <string>

#define WINDOW_WIDTH 560
#define WINDOW_HEIGHT 700

/* Reverse-DNS id with no domain claim; good enough for a personal app. */
#define APP_ID "local.fontsel"

/* Used only when there is no saved font yet, or the saved one is unreadable. */
#define DEFAULT_FONT "Sans 14"

/* The sample ladder: multiples of the size picked in the font dialog.
 * Rungs above 120pt are skipped, so a 48pt choice doesn't turn into a 144pt
 * wall of text. */
static const double LADDER[] = {1.0, 1.5, 2.0, 3.0};
#define MAX_LADDER_SIZE (120 * PANGO_SCALE)

static const char* SAMPLES[] = {
    /* replaced by the custom text when the entry has something in it */
    "The quick brown fox jumps over the lazy dog\n"
    "THE QUICK BROWN FOX JUMPS OVER THE LAZY DOG\n"
    "0123456789 !@#$%^&*()-=_+/\\,.;:'\"[]{}()\n"
    "Il1 O0 rn m ... — “quotes” £¥€\n"
    "😀 ❤ → ★",

    "The quick brown fox jumps over the lazy dog",
    "Handgloves",
    "Aa",
};

/* Each rung needs its own sample text; keep the two tables the same length. */
static_assert(G_N_ELEMENTS(LADDER) == G_N_ELEMENTS(SAMPLES),
              "LADDER and SAMPLES must have one entry each per rung");

struct App {
    GtkWidget* window = nullptr;
    GtkWidget* font_button = nullptr;
    GtkWidget* entry = nullptr;
    GtkWidget* samples = nullptr;
    PangoFontDescription* font_desc = nullptr;
    std::string custom_text;
};

/* Family, style and size all go through Pango attributes rather than CSS.
 * Attributes beat any stylesheet, so the font picked in the dialog is the font
 * shown, while CSS stays free to handle colour, padding and so on. */
static PangoAttrList* attrs_for(const PangoFontDescription* desc, int size) {
    PangoAttrList* list = pango_attr_list_new();

    const char* family = pango_font_description_get_family(desc);
    if (family != nullptr) {
        pango_attr_list_insert(list, pango_attr_family_new(family));
    }

    pango_attr_list_insert(list, pango_attr_size_new(size));

    PangoStyle style = pango_font_description_get_style(desc);
    if (style != PANGO_STYLE_NORMAL) {
        pango_attr_list_insert(list, pango_attr_style_new(style));
    }

    PangoWeight weight = pango_font_description_get_weight(desc);
    if (weight != PANGO_WEIGHT_NORMAL) {
        pango_attr_list_insert(list, pango_attr_weight_new(weight));
    }

    PangoStretch stretch = pango_font_description_get_stretch(desc);
    if (stretch != PANGO_STRETCH_NORMAL) {
        pango_attr_list_insert(list, pango_attr_stretch_new(stretch));
    }

    PangoVariant variant = pango_font_description_get_variant(desc);
    if (variant != PANGO_VARIANT_NORMAL) {
        pango_attr_list_insert(list, pango_attr_variant_new(variant));
    }

    return list;
}

static std::string config_path() {
    return std::string(g_get_user_config_dir()) + "/font-sel/config";
}

static void save_config(const App* app) {
    std::string path = config_path();

    gchar* dir = g_path_get_dirname(path.c_str());
    g_mkdir_with_parents(dir, 0755);
    g_free(dir);

    GKeyFile* kf = g_key_file_new();
    gchar* font = pango_font_description_to_string(app->font_desc);
    g_key_file_set_string(kf, "state", "font", font);
    g_key_file_set_string(kf, "state", "text", app->custom_text.c_str());
    g_free(font);

    gsize len = 0;
    gchar* data = g_key_file_to_data(kf, &len, nullptr);
    g_file_set_contents(path.c_str(), data, (gssize)len, nullptr);
    g_free(data);
    g_key_file_free(kf);
}

static void load_config(App* app) {
    GKeyFile* kf = g_key_file_new();

    if (g_key_file_load_from_file(kf, config_path().c_str(), G_KEY_FILE_NONE, nullptr)) {
        gchar* font = g_key_file_get_string(kf, "state", "font", nullptr);
        if (font != nullptr) {
            PangoFontDescription* saved = pango_font_description_from_string(font);
            /* An empty or garbled entry leaves no family behind; ignore it. */
            if (pango_font_description_get_family(saved) != nullptr) {
                pango_font_description_free(app->font_desc);
                app->font_desc = saved;
            } else {
                pango_font_description_free(saved);
            }
            g_free(font);
        }

        gchar* text = g_key_file_get_string(kf, "state", "text", nullptr);
        if (text != nullptr) {
            app->custom_text = text;
            g_free(text);
        }
    }

    g_key_file_free(kf);
}

/* Rebuild the whole ladder. Cheap enough to do on every change, and it keeps
 * the widget tree in step with the font description without any bookkeeping. */
static void rebuild_samples(App* app) {
    GtkWidget* child;
    while ((child = gtk_widget_get_first_child(app->samples)) != nullptr) {
        gtk_box_remove(GTK_BOX(app->samples), child);
    }

    int base = pango_font_description_get_size(app->font_desc);
    if (base <= 0) {
        base = 14 * PANGO_SCALE;
    }

    for (guint i = 0; i < G_N_ELEMENTS(LADDER); i++) {
        int size = (int)(base * LADDER[i]);
        /* Always keep the base sample, cap only the bigger rungs. */
        if (i > 0 && size > MAX_LADDER_SIZE) {
            continue;
        }

        const char* text = SAMPLES[i];
        if (i == 0 && !app->custom_text.empty()) {
            text = app->custom_text.c_str();
        }

        GtkWidget* label = gtk_label_new(text);
        gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
        gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
        gtk_label_set_wrap(GTK_LABEL(label), TRUE);
        gtk_label_set_wrap_mode(GTK_LABEL(label), PANGO_WRAP_WORD_CHAR);

        /* #font-label is the selector older gtk.css files used; the class is
         * the tidier one for styling all rungs at once. */
        gtk_widget_set_name(label, "font-label");
        gtk_widget_add_css_class(label, "font-sample");

        PangoAttrList* attrs = attrs_for(app->font_desc, size);
        gtk_label_set_attributes(GTK_LABEL(label), attrs);
        pango_attr_list_unref(attrs);

        gtk_box_append(GTK_BOX(app->samples), label);
    }
}

static void on_font_changed(GObject* button, GParamSpec*, gpointer data) {
    App* app = static_cast<App*>(data);

    PangoFontDescription* desc =
        gtk_font_dialog_button_get_font_desc(GTK_FONT_DIALOG_BUTTON(button));
    if (desc == nullptr) {
        return;
    }

    pango_font_description_free(app->font_desc);
    app->font_desc = pango_font_description_copy(desc);

    rebuild_samples(app);
    save_config(app);
}

static void on_text_changed(GtkEditable* editable, gpointer data) {
    App* app = static_cast<App*>(data);
    app->custom_text = gtk_editable_get_text(editable);
    rebuild_samples(app);
}

static gboolean on_close(GtkWindow*, gpointer data) {
    save_config(static_cast<App*>(data));
    return FALSE;
}

static void load_css(void) {
    /* Application priority, below the user's gtk.css, so personal styling wins. */
    GtkCssProvider* provider = gtk_css_provider_new();
    gtk_css_provider_load_from_string(provider,
        ".font-sample {"
        "  padding: 6px 10px;"
        "  border-bottom-width: 1px;"
        "  border-bottom-style: solid;"
        "  border-bottom-color: alpha(currentColor, 0.12);"
        "}");
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);
}

static void activate(GtkApplication* gtkapp, gpointer data) {
    App* app = static_cast<App*>(data);

    /* GtkApplication is single-instance per app id: a second launch calls
     * activate again. Raise the window that is already there instead of
     * building a second one that would share (and fight over) this state. */
    if (app->window != nullptr) {
        gtk_window_present(GTK_WINDOW(app->window));
        return;
    }

    load_css();

    app->window = gtk_application_window_new(gtkapp);
    gtk_window_set_title(GTK_WINDOW(app->window), "Font Sel");
    gtk_widget_set_name(app->window, "Font-Sel-Window");
    gtk_window_set_default_size(GTK_WINDOW(app->window), WINDOW_WIDTH, WINDOW_HEIGHT);

    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_top(vbox, 8);
    gtk_widget_set_margin_bottom(vbox, 8);
    gtk_widget_set_margin_start(vbox, 8);
    gtk_widget_set_margin_end(vbox, 8);
    gtk_window_set_child(GTK_WINDOW(app->window), vbox);

    app->font_button = gtk_font_dialog_button_new(gtk_font_dialog_new());
    gtk_font_dialog_button_set_level(GTK_FONT_DIALOG_BUTTON(app->font_button),
                                     GTK_FONT_LEVEL_FONT);
    gtk_font_dialog_button_set_use_font(GTK_FONT_DIALOG_BUTTON(app->font_button), TRUE);
    /* Size drives the samples, not the button's own label. */
    gtk_font_dialog_button_set_use_size(GTK_FONT_DIALOG_BUTTON(app->font_button), FALSE);
    gtk_font_dialog_button_set_font_desc(GTK_FONT_DIALOG_BUTTON(app->font_button),
                                         app->font_desc);
    gtk_box_append(GTK_BOX(vbox), app->font_button);

    app->entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(app->entry),
                                   "Custom sample text (empty = full character set)");
    if (!app->custom_text.empty()) {
        gtk_editable_set_text(GTK_EDITABLE(app->entry), app->custom_text.c_str());
    }
    gtk_box_append(GTK_BOX(vbox), app->entry);

    GtkWidget* scrolled = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scrolled, TRUE);
    gtk_widget_add_css_class(scrolled, "frame");

    app->samples = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_valign(app->samples, GTK_ALIGN_START);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), app->samples);
    gtk_box_append(GTK_BOX(vbox), scrolled);

    /* Connected after the initial font is set, so startup doesn't rebuild twice. */
    g_signal_connect(app->font_button, "notify::font-desc",
                     G_CALLBACK(on_font_changed), app);
    g_signal_connect(app->entry, "changed", G_CALLBACK(on_text_changed), app);
    g_signal_connect(app->window, "close-request", G_CALLBACK(on_close), app);

    rebuild_samples(app);
    gtk_window_present(GTK_WINDOW(app->window));
}

int main(int argc, char** argv) {
    App* app = new App();
    app->font_desc = pango_font_description_from_string(DEFAULT_FONT);
    load_config(app);

    GtkApplication* gtkapp = gtk_application_new(APP_ID, G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(gtkapp, "activate", G_CALLBACK(activate), app);

    int status = g_application_run(G_APPLICATION(gtkapp), argc, argv);

    g_object_unref(gtkapp);
    pango_font_description_free(app->font_desc);
    delete app;

    return status;
}
