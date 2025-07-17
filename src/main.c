#include <noct/noct.h>
#include <gtk/gtk.h>

static NoctEnv *global_rt;
static NoctValue global_dict;
static int global_argc;
static char **global_argv;
static GtkWidget *global_window;
static char *global_title;
static int global_width;
static int global_height;

/* Forward declaration. */
static bool load_file_content(const char *fname, char **data, size_t *size);
static bool register_ffi(NoctEnv *rt);
static void activate(GtkApplication *app, gpointer user_data);
static bool cfunc_UIRun(NoctEnv *rt);
static bool cfunc_VBox(NoctEnv *rt);
static bool cfunc_Label(NoctEnv *rt);
static bool cfunc_Button(NoctEnv *rt);
static bool cfunc_print(NoctEnv *rt);
static bool add_ui_element(NoctEnv *rt, NoctValue *dict, GtkWidget *parent);
static bool add_vbox_element(NoctEnv *rt, NoctValue *dict, GtkWidget *parent);
static bool add_label_element(NoctEnv *rt, NoctValue *dict, GtkWidget *parent);
static bool add_button_element(NoctEnv *rt, NoctValue *dict, GtkWidget *parent);
static void on_button_clicked(GtkButton *button, void *func);

/* FFI registration table. */
struct ffi_item {
	const char *name;
	int param_count;
	const char *param[NOCT_ARG_MAX];
	bool (*cfunc)(NoctEnv *rt);
} ffi_items[] = {
	{"UIRun", 4, {"title", "width", "height", "dict"}, cfunc_UIRun},
	{"VBox", 1, {"array"}, cfunc_VBox},
	{"Label", 1, {"dict"}, cfunc_Label},
	{"Button", 1, {"dict"}, cfunc_Button},
	{"print", 1, {"msg"}, cfunc_print},
};

/*
 * Main
 */
int main(int argc, char *argv[])
{
	char *file_data;
	size_t file_size;
	NoctEnv *rt;
	NoctValue ret;

	global_argc = argc;
	global_argv = argv;

	extern bool conf_use_jit;
	conf_use_jit = false;

	/* Check if a file is specified. */
	if (argc < 2) {
		printf("Specify a file.\n");
		return 1;
	}

	/* Load the specified file. */
	if (!load_file_content(argv[1], &file_data, &file_size))
		return 1;

	/* Create a runtime. */
	if (!noct_create(&rt))
		return 1;
	global_rt = rt;

	/* Register foreign functions. */
	if (!register_ffi(rt))
		return 1;

	/* Compile the source code. */
	if (!noct_register_source(rt, argv[1], file_data)) {
		printf("%s:%d: error: %s\n",
		       noct_get_error_file(rt),
		       noct_get_error_line(rt),
		       noct_get_error_message(rt));
		return 1;
	}

	/* Run the "main()" function. */
	if (!noct_call_with_name(rt, "main", NULL, 0, NULL, &ret)) {
		printf("%s:%d: error: %s\n",
		       noct_get_error_file(rt),
		       noct_get_error_line(rt),
		       noct_get_error_message(rt));
		return 1;
	}
	
	/* Destroy the runtime. */
	if (!noct_destroy(rt))
		return 1;

	return 0;
}

/* Load a file content. */
static bool load_file_content(const char *fname, char **data, size_t *size)
{
	FILE *fp;
	long pos;

	/* Open the file. */
	fp = fopen(fname, "rb");
	if (fp == NULL) {
		printf("Cannot open file %s.\n", fname);
		return false;
	}

	/* Get the file size. */
	fseek(fp, 0, SEEK_END);
	*size = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	/* Allocate a buffer. */
	*data = malloc(*size + 1);
	if (*data == NULL) {
		printf("Out of memory.\n");
		return false;
	}

	/* Read the data. */
	if (fread(*data, 1, *size, fp) != *size) {
		printf("Cannot read file %s.\n", fname);
		return false;
	}

	/* Terminate the string. */
	(*data)[*size] = '\0';

	fclose(fp);

	return true;
}

/* Register FFI functions. */
static bool register_ffi(NoctEnv *rt)
{
	int i;

	for (i = 0; i < (int)(sizeof(ffi_items) / sizeof(struct ffi_item)); i++) {
		if (!noct_register_cfunc(rt,
				       ffi_items[i].name,
				       ffi_items[i].param_count,
				       ffi_items[i].param,
				       ffi_items[i].cfunc,
				       NULL))
			return false;
	}

	return true;
}

/* Implementation of UIRun(). */
static bool
cfunc_UIRun(
	NoctEnv *rt)
{
	const char *title;
	int width;
	int height;
	NoctValue dict;
	GtkApplication *app;

	/* Get the "title" parameer. */
	if (!noct_get_arg_string(rt, 0, &title))
		return false;
	global_title = strdup(title);

	/* Get the "width" parameter. */
	if (!noct_get_arg_int(rt, 1, &width))
		return false;
	global_width = width;

	/* Get the "height" parameter. */
	if (!noct_get_arg_int(rt, 2, &height))
		return false;
	global_height = height;

	/* Get the "dict" parameter. */
	if (!noct_get_arg_dict(rt, 3, &dict))
		return false;
	global_dict = dict;

	/* Make an application. */
	app = gtk_application_new("io.noctvm.noctui", G_APPLICATION_DEFAULT_FLAGS);
	g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);

	/* Run the main loop. */
	g_application_run(G_APPLICATION(app), 0, NULL);
	g_object_unref(app);

	return true;
}

/* Implementation of VBox() */
static bool
cfunc_VBox(
	NoctEnv *rt)
{
	NoctValue array;
	NoctValue ret;

	/* Get the "array" argument. */
	if (!noct_get_arg_array(rt, 0, &array))
		return false;

	/* Return a dictionary. */
	if (!noct_make_empty_dict(rt, &ret))
		return false;
	if (!noct_set_dict_elem_string(rt, &ret, "type", "VBox"))
		return false;
	if (!noct_set_dict_elem(rt, &ret, "array", &array))
		return false;
	if (!noct_set_return(rt, &ret))
		return false;

	return true;
}

/* Implementation of Label() */
static bool
cfunc_Label(
	NoctEnv *rt)
{
	NoctValue dict;
	NoctValue ret;

	/* Get the dict argument. */
	if (!noct_get_arg_dict(rt, 0, &dict))
		return false;

	/* Return a dictionary. */
	if (!noct_make_dict_copy(rt, &dict, &ret))
		return false;
	if (!noct_set_dict_elem_string(rt, &ret, "type", "Label"))
		return false;
	if (!noct_set_return(rt, &ret))
		return false;

	return true;
}

/* Implementation of Button() */
static bool
cfunc_Button(
	NoctEnv *rt)
{
	NoctValue dict;
	NoctValue ret;

	/* Get the dict argument. */
	if (!noct_get_arg_dict(rt, 0, &dict))
		return false;

	/* Return a dictionary. */
	if (!noct_make_dict_copy(rt, &dict, &ret))
		return false;
	if (!noct_set_dict_elem_string(rt, &ret, "type", "Button"))
		return false;
	if (!noct_set_return(rt, &ret))
		return false;

	return true;
}

/* Implementation of print() */
static bool
cfunc_print(
	NoctEnv *rt)
{
	NoctValue msg;
	const char *s;
	float f;
	int i;
	int type;

	if (!noct_get_arg(rt, 0, &msg))
		return false;

	if (!noct_get_value_type(rt, &msg, &type))
		return false;

	switch (type) {
	case NOCT_VALUE_INT:
		if (!noct_get_int(rt, &msg, &i))
			return false;
		printf("%i\n", i);
		break;
	case NOCT_VALUE_FLOAT:
		if (!noct_get_float(rt, &msg, &f))
			return false;
		printf("%f\n", f);
		break;
	case NOCT_VALUE_STRING:
		if (!noct_get_string(rt, &msg, &s))
			return false;
		printf("%s\n", s);
		break;
	default:
		printf("[object]\n");
		break;
	}

	return true;
}

/* Signal "activate". */
static void activate(GtkApplication *app, gpointer user_data)
{
	NoctEnv *rt;
	NoctValue dict;

	rt = global_rt;
	dict = global_dict;

	GtkWidget *window = gtk_application_window_new(app);
	gtk_window_set_title(GTK_WINDOW(window), global_title);
	gtk_window_set_default_size(GTK_WINDOW(window), global_width, global_height);
	gtk_widget_set_visible(window, true);

	global_window = window;

	/* Add UI elements. */
	add_ui_element(rt, &dict, window);
}

/* Add a UI element. */
static bool add_ui_element(NoctEnv *rt, NoctValue *dict, GtkWidget *parent)
{
	const char *type;
	NoctValue prop_type;

	if (!noct_get_dict_elem_string(rt, dict, "type", &type))
		return false;
	if (strcmp(type, "VBox") == 0) {
		if (!add_vbox_element(rt, dict, parent))
			return false;
	} else if (strcmp(type, "Label") == 0) {
		if (!add_label_element(rt, dict, parent))
			return false;
	} else if (strcmp(type, "Button") == 0) {
		if (!add_button_element(rt, dict, parent))
			return false;
	} else {
		noct_error(rt, "Non supported item.");
		return false;
	}

	return true;
}

/* Add a VBox element. */
static bool add_vbox_element(NoctEnv *rt, NoctValue *dict, GtkWidget *parent)
{
	NoctValue array;
	int size, i;

	GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
	if (parent == global_window)
		gtk_window_set_child(GTK_WINDOW(parent), vbox);
	else
		gtk_box_append(GTK_BOX(parent), vbox);

	if (!noct_get_dict_elem_array(rt, dict, "array", &array))
		return false;
	if (!noct_get_array_size(rt, &array, &size))
		return false;
	for (i = 0; i < size; i++) {
		NoctValue dict;

		if (!noct_get_array_elem_dict(rt, &array, i, &dict))
			return false;
		if (!add_ui_element(rt, &dict, vbox))
			return false;
	}

	return true;
}

/* Add a Label element. */
static bool add_label_element(NoctEnv *rt, NoctValue *dict, GtkWidget *parent)
{
	const char *title;

	if (!noct_get_dict_elem_string(rt, dict, "title", &title))
		return false;

	GtkWidget *label = gtk_label_new(title);
	if (parent == global_window)
		gtk_window_set_child(GTK_WINDOW(parent), label);
	else
		gtk_box_append(GTK_BOX(parent), label);

	return true;
}

/* Add a Button element. */
static bool add_button_element(NoctEnv *rt, NoctValue *dict, GtkWidget *parent)
{
	const char *title;
	NoctFunc *func;

	if (!noct_get_dict_elem_string(rt, dict, "title", &title))
		return false;
	if (!noct_get_dict_elem_func(rt, dict, "onClick", &func))
		return false;

	GtkWidget *button = gtk_button_new_with_label(title);
	if (parent == global_window)
		gtk_window_set_child(GTK_WINDOW(parent), button);
	else
		gtk_box_append(GTK_BOX(parent), button);

	g_signal_connect(button, "clicked", G_CALLBACK(on_button_clicked), func);

	return true;
}

/* Signal: "clicked" */
static void on_button_clicked(GtkButton *button, void *func)
{
	NoctFunc *f;
	NoctValue ret;

	f = (NoctFunc *)func;

	/* Call a function. */
	noct_call(global_rt, f, NULL, 0, NULL, &ret);
}
