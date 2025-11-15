#include <gio/gio.h>
#include <gtk/gtk.h>
#include <glib.h>
#include <stdlib.h>
#include <string.h>

#define G_APPLICATION_DEFAULT_FLAGS 0

const char* CWD = ".";

enum {
	FILE_NAME,
	NUM_COLS
};

int exit_status = EXIT_SUCCESS;

void add_row_to_tree(GtkTreeStore* tree_store, GtkTreeIter* iter, GtkTreeIter* parent, char* file_name) {
	gtk_tree_store_append(tree_store, iter, parent);
	gtk_tree_store_set(tree_store, iter, FILE_NAME, file_name, -1);
}

void list_recursively(GFile *dir, GtkTreeStore* tree_store, GtkTreeIter* iter, GtkTreeIter* parent, GError **error) {
	GFileEnumerator *enumerator;
	GFileInfo *file_info;

	enumerator = g_file_enumerate_children(dir, "standard", G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL, error);

	if (enumerator == NULL) {
		char* path = g_file_get_path(dir);
		g_warning("Could not enumerate children of %s: %s", path, (*error)->message);
		g_free(path);
		return;
	}

	while ((file_info = g_file_enumerator_next_file(enumerator, NULL, error)) != NULL) {
		GFileType file_type = g_file_info_get_file_type(file_info);
		const char* file_name = g_file_info_get_name(file_info);
		GFile *child_file = g_file_get_child(dir, file_name);

		char* path = g_file_get_path(child_file);
		char* enhanced_path;
		switch (file_type) {
			case G_FILE_TYPE_REGULAR:
				enhanced_path = (char*)malloc((strlen(path)+4) * sizeof(char));
				if (enhanced_path == NULL) {
					g_warning("Could not allocate memory for the path");
					break;
				}
				sprintf(enhanced_path, "└%s", path);
				add_row_to_tree(tree_store, iter, parent, enhanced_path);
				free(enhanced_path);
				break;
			case G_FILE_TYPE_SYMBOLIC_LINK: {
				const char* target_path = g_file_info_get_symlink_target(file_info);
				enhanced_path = (char*)malloc(((strlen(path)+8)+strlen(target_path)) * sizeof(char));
				if (enhanced_path == NULL) {
					g_warning("Could not allocate memory for the path");
					break;
				}
				sprintf(enhanced_path, "└%s -> %s", path, target_path);
				add_row_to_tree(tree_store, iter, parent, enhanced_path);
				free(enhanced_path);
				break;
			}
			case G_FILE_TYPE_DIRECTORY:
				if (g_strcmp0(file_name, ".") != 0 && g_strcmp0(file_name, "..") != 0) {
					enhanced_path = (char*)malloc((strlen(path)+4) * sizeof(char));
					if (enhanced_path == NULL) {
						g_warning("Could not allocate memory for the path");
						break;
					}
					sprintf(enhanced_path, "└%s", path);
					add_row_to_tree(tree_store, iter, parent, enhanced_path);
					free(enhanced_path);
					GtkTreeIter new_iter;
					list_recursively(child_file, tree_store, &new_iter, iter, error);
				}
				break;
			default:
				break;
		}

		g_free(path);

		g_object_unref(file_info);
		g_object_unref(child_file);
	}

	g_object_unref(enumerator);
}

int build_tree(GtkTreeStore* tree_store, GtkTreeIter* iter, GtkTreeIter* parent) {
	g_autoptr(GFile) root_dir = NULL;
	g_autoptr(GError) error = NULL;

	list_recursively(g_file_new_for_path(CWD), tree_store, iter, parent, &error);

	if (error != NULL) {
		g_printerr("Error: %s\n", error->message);
		return 1;
	}

	return 0;
}

void app_activate_cb(GApplication* app) {
	GtkWidget* window;
	GtkWidget* tree_view;
	GtkTreeStore* tree_store;
	GtkTreeIter iter;
	GtkTreeIter* root = NULL;
	GtkCellRenderer* renderer;
	GtkTreeViewColumn *column;

	window = gtk_application_window_new(GTK_APPLICATION(app));
	gtk_window_set_title(GTK_WINDOW(window), "directory tree");
	gtk_window_set_default_size(GTK_WINDOW(window), 600, 600);

	tree_store = gtk_tree_store_new(NUM_COLS, G_TYPE_STRING);

	add_row_to_tree(tree_store, &iter, root, ".");

	if (build_tree(tree_store, &iter, root) > 0) {
		exit_status = EXIT_FAILURE;
		g_application_quit(app);
	}

	tree_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(tree_store));
	g_object_unref(tree_store);

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes("Name", renderer, "text", FILE_NAME, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);
	gtk_tree_view_expand_all(GTK_TREE_VIEW(tree_view));

	GtkWidget *scrolled_window = gtk_scrolled_window_new();
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled_window), tree_view);
	gtk_window_set_child(GTK_WINDOW(window), scrolled_window);

	gtk_widget_show(window);
}

int main(int argc, char** argv) {
	GtkApplication *app;
	int status;

	app = gtk_application_new("directory.tree.app", G_APPLICATION_DEFAULT_FLAGS);
	g_signal_connect(app, "activate", G_CALLBACK(app_activate_cb), NULL);
	status = g_application_run(G_APPLICATION(app), argc, argv);
	g_object_unref(app);

	return status;
}
