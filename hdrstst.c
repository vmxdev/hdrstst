#include <gtk/gtk.h>
#include <math.h>

/* frames per second */
#define FPS 25

/* number of last points */
#define TAIL_SIZE 1000

typedef struct dot_coord
{
	double x, y;
} dot_coord;

typedef struct app_data
{
	int running;                      /* drawing state */
	GtkWidget *drawing;               /* drawing widget */

	GtkWidget *r1_ctrl;
	GtkWidget *r2_ctrl;
	GtkWidget *r3_ctrl;
	GtkWidget *speed_ctrl;

	GtkWidget *start_stop_button;

	size_t idx;                       /* index in tail points */
	double angle;
	dot_coord side_circle;            /* side circle coords */
	dot_coord tail_points[TAIL_SIZE]; /* tail points */
} app_data;

/* calculate coordinates of side circle and point on r3 to display */
static void
calc_coords(double r1, double r2, double r3, double angle,
	dot_coord *circle, dot_coord *point)
{
	double side_angle;

	/* side circle */
	circle->x = (r1 + r2) * cos(angle);
	circle->y = (r1 + r2) * sin(angle);

	/* r3 */
	if (r2 < DBL_EPSILON) {
		side_angle = angle;
	} else {
		side_angle = angle + angle * r1 / r2;
	}
	point->x = circle->x  + r3 * cos(side_angle);
	point->y = circle->y  + r3 * sin(side_angle);
}

static gboolean
timer_callback(gpointer user_data)
{
	app_data *data = (app_data *)user_data;


	if (data->running) {
		data->angle +=
			gtk_spin_button_get_value (GTK_SPIN_BUTTON (data->speed_ctrl));
	}

	data->idx++;
	if (data->idx >= TAIL_SIZE) {
		data->idx = 0;
	}

	calc_coords(
		gtk_spin_button_get_value (GTK_SPIN_BUTTON (data->r1_ctrl)),
		gtk_spin_button_get_value (GTK_SPIN_BUTTON (data->r2_ctrl)),
		gtk_spin_button_get_value (GTK_SPIN_BUTTON (data->r3_ctrl)),
		data->angle,
		&data->side_circle, &data->tail_points[data->idx]);

	/* force redraw */
	gtk_widget_queue_draw_area (data->drawing, 0, 0,
		gtk_widget_get_allocated_width (data->drawing),
		gtk_widget_get_allocated_height (data->drawing));

	return TRUE;
}

static void
start_stop_callback (GtkWidget *widget, gpointer user_data)
{
	app_data *data = (app_data *)user_data;
	if (data->running) {
		gtk_button_set_label (GTK_BUTTON (data->start_stop_button),
			"Start");
		data->running = 0;
	} else {
		gtk_button_set_label (GTK_BUTTON (data->start_stop_button),
			"Stop");
		data->running = 1;
	}
}

gboolean
draw_callback (GtkWidget *widget, cairo_t *cr, gpointer user_data)
{
	app_data *data = (app_data *)user_data;

	size_t i, dot_alpha;

	const double rgb_low = 0.7;
	const double rgb_high = 0.9;

	guint width, height, lesser_half;
	guint central_r, side_r;
	guint side_circle_x, side_circle_y;
	guint r3_x, r3_y;

	double r1, r2, r3, max23;

	width = gtk_widget_get_allocated_width (widget);
	height = gtk_widget_get_allocated_height (widget);

	lesser_half = MIN (width, height) / 2 - 10; /* leave small border */

	/* ?? */
	if (!data->r1_ctrl) {
		return FALSE;
	}

	r1 = gtk_spin_button_get_value (GTK_SPIN_BUTTON (data->r1_ctrl));
	r2 = gtk_spin_button_get_value (GTK_SPIN_BUTTON (data->r2_ctrl));
	r3 = gtk_spin_button_get_value (GTK_SPIN_BUTTON (data->r3_ctrl));

	max23 = r3 > r2 ? (r3 + r2) : (r2 * 2);

	if ((r1 + max23) < DBL_EPSILON) {
		return FALSE;
	}

	cairo_set_line_width (cr, 0.3);

	/* central circle */
	/* radius */
	central_r = lesser_half  - lesser_half * max23 / (r1 + max23);

	cairo_save (cr);
	cairo_arc (cr,
		width / 2.0, height / 2.0,
		central_r,
		0.0, 2 * G_PI);

	cairo_set_source_rgb (cr, rgb_low, rgb_high, rgb_low);
	cairo_fill_preserve (cr);
	cairo_restore (cr);
	cairo_stroke (cr);

	/* side circle */
	side_circle_x = width / 2.0   + lesser_half * data->side_circle.x / (r1 + max23);
	side_circle_y = height / 2.0  + lesser_half * data->side_circle.y / (r1 + max23);
	side_r = lesser_half * r2 / (r1 + max23);

	cairo_save (cr);
	cairo_arc (cr,
		side_circle_x, side_circle_y,
		side_r,
		0.0, 2 * G_PI);

	cairo_set_source_rgb (cr, rgb_low, rgb_low, rgb_high);
	cairo_fill_preserve (cr);
	cairo_restore (cr);
	cairo_stroke (cr);

	/* r3 line */
	r3_x = width / 2.0
		+ lesser_half * data->tail_points[data->idx].x / (r1 + max23);
	r3_y = height / 2.0
		+ lesser_half * data->tail_points[data->idx].y / (r1 + max23);

	cairo_set_line_width (cr, lesser_half / 150.0);
	cairo_set_source_rgb (cr, rgb_high, rgb_high, rgb_low);
	cairo_save (cr);
	cairo_move_to (cr, side_circle_x, side_circle_y);
	cairo_line_to (cr, r3_x, r3_y);
	cairo_restore (cr);
	cairo_stroke (cr);

	/* tail dots */
	cairo_save (cr);
	for (i=data->idx, dot_alpha=0; dot_alpha<TAIL_SIZE; dot_alpha++) {
		guint x, y;

		i++;
		if (i == TAIL_SIZE) {
			i = 0;
		}

		if (isnan(data->tail_points[i].x)
			&& isnan(data->tail_points[i].y)) {
			continue;
		}

		x = width / 2.0
			+ lesser_half * data->tail_points[i].x / (r1 + max23);
		y = height / 2.0
			+ lesser_half * data->tail_points[i].y / (r1 + max23);
		cairo_set_source_rgba (cr, rgb_high, rgb_low / 2, rgb_low / 2,
			(double)dot_alpha / TAIL_SIZE);
		cairo_arc (cr,
			x,
			y,
			lesser_half / 150.0,
			0.0, 2 * G_PI);
		cairo_fill_preserve (cr);
		cairo_stroke (cr);
	}
	cairo_restore (cr);

	return FALSE;
}

/* add label and spin button to control pane */
static GtkWidget *
add_labeled_spin_btn (GtkWidget *controls_grid, const gchar *label_text,
	GtkAdjustment *adj, guint row)
{
	GtkWidget *label;
	GtkWidget *spin_btn;
	GValue align_start = G_VALUE_INIT;

	g_value_init (&align_start, G_TYPE_INT);
	g_value_set_int (&align_start, GTK_ALIGN_START);

	label = gtk_label_new (label_text);
	g_object_set_property (G_OBJECT (label), "halign", &align_start);
	gtk_grid_attach (GTK_GRID (controls_grid),
		label, 0, row, 1, 1);

	spin_btn = gtk_spin_button_new (adj, 0.001, 3);
	gtk_grid_attach (GTK_GRID (controls_grid),
		spin_btn,
		1, row, 1, 1);

	return spin_btn;
}

static void
activate (GtkApplication *app, gpointer user_data)
{
	GtkWidget *window;
	GtkWidget *paned;
	GtkWidget *drawing;

	GtkWidget *controls_frame;
	GtkWidget *controls_grid;

	GtkAdjustment *r1_adj, *r2_adj, *r3_adj;
	GtkAdjustment *speed_adj;
	guint row = 0;

	app_data *data = (app_data *)user_data;

	window = gtk_application_window_new (app);
	gtk_window_set_title (GTK_WINDOW (window), "HS Test");
	gtk_window_set_default_size (GTK_WINDOW (window), 500, 500);

	r1_adj = gtk_adjustment_new (5.500, 0.0, 100.0, 0.001, 0.1, 0.0);
	r2_adj = gtk_adjustment_new (2.500, 0.0, 100.0, 0.001, 0.1, 0.0);
	r3_adj = gtk_adjustment_new (3.500, 0.0, 100.0, 0.001, 0.1, 0.0);
	speed_adj = gtk_adjustment_new (0.050, 0.0, 5.0, 0.001, 0.1, 0.0);

	/* drawing area */
	drawing = gtk_drawing_area_new ();
	gtk_widget_set_size_request (drawing, 100, 100);
	g_signal_connect (G_OBJECT (drawing), "draw", G_CALLBACK (draw_callback), data);
	data->drawing = drawing;
	paned = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);

	/* controls */
	controls_frame = gtk_frame_new ("Controls");

	controls_grid  = gtk_grid_new ();

	data->r1_ctrl = add_labeled_spin_btn (controls_grid, "R1:", r1_adj, row++);
	data->r2_ctrl = add_labeled_spin_btn (controls_grid, "R2:", r2_adj, row++);
	data->r3_ctrl = add_labeled_spin_btn (controls_grid, "R3:", r3_adj, row++);
	data->speed_ctrl = add_labeled_spin_btn (controls_grid,
		"Rotation speed:", speed_adj, row++);

	/* start-stop button */
	data->start_stop_button = gtk_button_new_with_label ("Start");
	g_signal_connect (data->start_stop_button,
		"clicked", G_CALLBACK (start_stop_callback), data);

	gtk_grid_attach (GTK_GRID (controls_grid),
		data->start_stop_button,
		0, row, 2, 1);

	gtk_container_add (GTK_CONTAINER (controls_frame), controls_grid);

	/* setup paned */
	gtk_paned_pack1 (GTK_PANED (paned), drawing, TRUE, FALSE);
	gtk_paned_pack2 (GTK_PANED (paned), controls_frame, FALSE, FALSE);
	gtk_container_add (GTK_CONTAINER (window), paned);

	/* timer_callback will be called FPS times per second */
	gdk_threads_add_timeout (1000 / FPS, &timer_callback, data);

	gtk_widget_show_all (window);
}

int
main (int argc, char *argv[])
{
	GtkApplication *app;
	int status;
	app_data *data;
	size_t i;

	data = (app_data *) g_new (app_data, 1);
	data->angle = 0.0f;
	data->idx = 0;
	data->running = 0;
	for (i=0; i<TAIL_SIZE; i++) {
		data->tail_points[i].x = data->tail_points[i].y = NAN;
	}

	app = gtk_application_new ("org.gtk.example", G_APPLICATION_FLAGS_NONE);
	g_signal_connect (app, "activate", G_CALLBACK (activate), data);
	status = g_application_run (G_APPLICATION (app), argc, argv);
	g_object_unref (app);

	return status;
}

