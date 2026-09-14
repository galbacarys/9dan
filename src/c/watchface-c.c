#include <pebble.h>

#define BOARD 9
#define SPACING 20
#define STONE_R 8
#define MARK_R 10

// --- cropped 9x9 problem data (setup + solution line), [color,x,y], Black=0 White=1 ---
static const int8_t s_setup[][3] = {
  {0,4,5},{0,5,5},{0,6,5},{0,7,5},{0,8,5},
  {0,2,6},{0,4,6},{0,3,7},{0,6,8},
  {1,5,6},{1,6,6},{1,8,6},{1,4,7},{1,6,7},{1,8,7},{1,4,8}
};
static const int s_num_setup = 16;

static const int8_t s_line[][3] = { {0,7,8},{1,7,7},{0,3,8} };
static const int s_num_line = 3;

static Window *s_window;
static Layer  *s_canvas;

// bit board: 0 empty, 1 black, 2 white
static uint8_t s_board[BOARD][BOARD];
static int s_step = -1;          // -1 = not started
static bool s_marked[BOARD][BOARD];

// last stone played position (for the red mark)
static int s_mark_x, s_mark_y;
static bool s_has_mark;

static int s_board_x0, s_board_y0;
static TextLayer *s_time_layer;
static TextLayer *s_hint_layer;

static int ix_x(int i) { return s_board_x0 + i * SPACING; }
static int ix_y(int i) { return s_board_y0 + i * SPACING; }

static void fill_initial(void) {
  memset(s_board, 0, sizeof(s_board));
  memset(s_marked, 0, sizeof(s_marked));
  for (int i = 0; i < s_num_setup; i++) {
    int color = s_setup[i][0], x = s_setup[i][1], y = s_setup[i][2];
    s_board[y][x] = (uint8_t)(color == 0 ? 1 : 2);
  }
  s_step = -1;
  s_has_mark = false;
}

static void canvas_update_proc(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);

  // clear background
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, b, 0, GCornerNone);

  // board background (wood tone) + grid
  int grid_px = (BOARD - 1) * SPACING;
  graphics_context_set_fill_color(ctx, GColorFromRGB(0xd8, 0xb4, 0x7c));
  graphics_fill_rect(ctx, GRect(s_board_x0 - SPACING / 2, s_board_y0 - SPACING / 2,
                                grid_px + SPACING, grid_px + SPACING), 0, GCornerNone);

  graphics_context_set_stroke_color(ctx, GColorDarkGray);
  graphics_context_set_stroke_width(ctx, 1);
  for (int i = 0; i < BOARD; i++) {
    graphics_draw_line(ctx, GPoint(ix_x(i), ix_y(0)), GPoint(ix_x(i), ix_y(BOARD - 1)));
    graphics_draw_line(ctx, GPoint(ix_x(0), ix_y(i)), GPoint(ix_x(BOARD - 1), ix_y(i)));
  }

  // stones
  for (int y = 0; y < BOARD; y++) {
    for (int x = 0; x < BOARD; x++) {
      if (!s_board[y][x]) continue;
      GPoint c = GPoint(ix_x(x), ix_y(y));
      if (s_board[y][x] == 1) {
        graphics_context_set_fill_color(ctx, GColorBlack);
        graphics_fill_circle(ctx, c, STONE_R);
        graphics_context_set_stroke_color(ctx, GColorDarkGray);
        graphics_context_set_stroke_width(ctx, 1);
        graphics_draw_circle(ctx, c, STONE_R);
      } else {
        graphics_context_set_fill_color(ctx, GColorWhite);
        graphics_fill_circle(ctx, c, STONE_R);
        graphics_context_set_stroke_color(ctx, GColorDarkGray);
        graphics_context_set_stroke_width(ctx, 1);
        graphics_draw_circle(ctx, c, STONE_R);
      }
    }
  }

  // red last-move mark
  if (s_has_mark) {
    GPoint c = GPoint(ix_x(s_mark_x), ix_y(s_mark_y));
    graphics_context_set_stroke_color(ctx, GColorRed);
    graphics_context_set_stroke_width(ctx, 2);
    graphics_draw_circle(ctx, c, MARK_R);
  }
}

static void draw_thing(void) {
  layer_mark_dirty(s_canvas);
}

static void advance(void) {
  if (s_step + 1 >= s_num_line) return;
  s_step++;
  const int8_t *m = s_line[s_step];
  int color = m[0], x = m[1], y = m[2];
  s_board[y][x] = (uint8_t)(color == 0 ? 1 : 2);
  // only mark black's moves (the solver) in this spike
  s_mark_x = x; s_mark_y = y; s_has_mark = true;
  light_enable_interaction();
  draw_thing();
  APP_LOG(APP_LOG_LEVEL_INFO, "advance step=%d move=(%d,%d)", s_step, x, y);
}

/* ---- click handlers: prove buttons fire in a watchface ---- */
static void prv_select_click(ClickRecognizerRef rec, void *ctx) {
  APP_LOG(APP_LOG_LEVEL_INFO, "SELECT clicked");
  advance();
}
static void prv_up_click(ClickRecognizerRef rec, void *ctx) {
  APP_LOG(APP_LOG_LEVEL_INFO, "UP clicked");
  advance();
}
static void prv_down_click(ClickRecognizerRef rec, void *ctx) {
  APP_LOG(APP_LOG_LEVEL_INFO, "DOWN clicked");
  advance();
}
static void prv_long_select(ClickRecognizerRef rec, void *ctx) {
  APP_LOG(APP_LOG_LEVEL_INFO, "LONG SELECT: reset");
  light_enable_interaction();
  fill_initial();
  draw_thing();
}
static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select_click);
  window_single_click_subscribe(BUTTON_ID_UP, prv_up_click);
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_down_click);
  window_long_click_subscribe(BUTTON_ID_SELECT, 500, prv_long_select, NULL);
}

/* ---- time ---- */
static void update_time(void) {
  time_t t = time(NULL);
  struct tm *tick = localtime(&t);
  static char buf[8];
  strftime(buf, sizeof(buf), "%H:%M", tick);
  text_layer_set_text(s_time_layer, buf);
}
static void tick_handler(struct tm *tick, TimeUnits units_changed) {
  update_time();
}
static void hint_refresh(void) {
  static char h[24];
  snprintf(h, sizeof(h), "easy-01  %d/%d", s_step + 1, s_num_line);
  text_layer_set_text(s_hint_layer, h);
}

/* ---- window ---- */
static void main_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect b = layer_get_bounds(root);

  // center 9x9 grid, shifted up to leave room for clock at bottom
  int grid_px = (BOARD - 1) * SPACING;
  s_board_x0 = (b.size.w - grid_px) / 2;
  s_board_y0 = (b.size.h - grid_px) / 2 - 12;
  if (s_board_y0 < 2) s_board_y0 = 2;

  s_canvas = layer_create(b);
  layer_set_update_proc(s_canvas, canvas_update_proc);
  layer_add_child(root, s_canvas);

  s_time_layer = text_layer_create(GRect(0, b.size.h - 40, b.size.w, 28));
  text_layer_set_text_color(s_time_layer, GColorWhite);
  text_layer_set_background_color(s_time_layer, GColorClear);
  text_layer_set_font(s_time_layer, fonts_get_system_font(FONT_KEY_ROBOTO_CONDENSED_21));
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);
  layer_add_child(root, text_layer_get_layer(s_time_layer));

  s_hint_layer = text_layer_create(GRect(0, b.size.h - 62, b.size.w, 20));
  text_layer_set_text_color(s_hint_layer, GColorLightGray);
  text_layer_set_background_color(s_hint_layer, GColorClear);
  text_layer_set_font(s_hint_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(s_hint_layer, GTextAlignmentCenter);
  layer_add_child(root, text_layer_get_layer(s_hint_layer));

  hint_refresh();
}

static void main_window_unload(Window *window) {
  layer_destroy(s_canvas);
  text_layer_destroy(s_time_layer);
  text_layer_destroy(s_hint_layer);
}

static void init(void) {
  fill_initial();
  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);
  window_set_window_handlers(s_window, (WindowHandlers){
    .load = main_window_load,
    .unload = main_window_unload,
  });
  window_set_click_config_provider(s_window, click_config_provider);
  window_stack_push(s_window, true);

  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  update_time();
  APP_LOG(APP_LOG_LEVEL_INFO, "goface-c init done");
}

static void deinit(void) {
  tick_timer_service_unsubscribe();
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}