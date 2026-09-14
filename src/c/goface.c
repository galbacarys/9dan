#include <pebble.h>
#include "problems.h"

// ---- geometry ----
#define BOARD_PX 9
#define SPACING  19
#define STONE_R  7
#define MARK_R   (STONE_R + 2)  // last-move marker ring radius
#define TURN_R   8
#define TURN_RING (TURN_R + 3)

// ---- colors (Pebble GColor8 palette approximations) ----
#define CLR_BG       GColorBlack
#define CLR_BOARD    GColorFromRGB(0xd8, 0xb4, 0x7c) // light wood
#define CLR_GRID     GColorFromRGB(0x8a, 0x6a, 0x3c) // darker grid line
#define CLR_BLACK_ST GColorFromRGB(0x1a, 0x1a, 0x1a)
#define CLR_WHITE_ST GColorFromRGB(0xf5, 0xf5, 0xf5)
#define CLR_W_ST_OUT GColorFromRGB(0x30, 0x30, 0x30) // white stone outline
#define CLR_WHITE    GColorWhite
#define CLR_GRAY     GColorFromRGB(0x80, 0x80, 0x80)
#define CLR_MARKER   GColorFromRGB(0xff, 0x22, 0x22) // last-move ring

#define RESET_MS 10000  // ms of inactivity before the board resets to setup

// ---- state ----
static Window *s_window;
static Layer  *s_canvas;
static int s_board_x0, s_board_y0;

static uint8_t s_board[BOARD][BOARD];   // 0 empty, 1 black, 2 white
static Problem s_problem;
static int s_step = -1;
static int s_mark_x, s_mark_y;           // last played stone for red ring
static bool s_has_mark;
static int s_battery_pct = 100;
static AppTimer *s_reset_timer = NULL;

static int ix_x(int i) { return s_board_x0 + i * SPACING; }
static int ix_y(int i) { return s_board_y0 + i * SPACING; }

// fwd decls (defined later in file)
static void reset_board(void);
static void reset_timeout(void *data);
static void set_random_problem(void);
static void advance(void);

// ---- layout ----
static void layout_board(GRect b) {
  int grid_px = (BOARD_PX - 1) * SPACING;
  s_board_x0 = (b.size.w - grid_px) / 2;
  s_board_y0 = (b.size.h - grid_px) / 2 - 10;
  if (s_board_y0 < 2) s_board_y0 = 2;
}

// ---- board state ----
static void reset_board(void) {
  memset(s_board, 0, sizeof(s_board));
  for (uint16_t i = 0; i < s_problem.setup_len; i++) {
    Move *m = &s_problem.setup[i];
    s_board[m->y][m->x] = m->color + 1;
  }
  s_step = -1;
  s_has_mark = false;
  layer_mark_dirty(s_canvas);
}

static void advance(void) {
  if (s_step + 1 >= (int)s_problem.line_len) return;
  s_step++;
  Move *m = &s_problem.line[s_step];
  s_board[m->y][m->x] = m->color + 1;
  remove_adjacent_captures(s_board, m->y, m->x, m->color + 1);
  s_mark_x = m->x; s_mark_y = m->y;
  s_has_mark = true;
  light_enable_interaction();

  // arm the inactivity reset
  if (s_reset_timer) app_timer_cancel(s_reset_timer);
  s_reset_timer = app_timer_register(RESET_MS, reset_timeout, NULL);

  layer_mark_dirty(s_canvas);
}

// ---- drawing ----
static void fill_circle(GContext *ctx, GColor c, int cx, int cy, int r) {
  graphics_context_set_fill_color(ctx, c);
  graphics_fill_circle(ctx, GPoint(cx, cy), (uint16_t)r);
}

// Thin ring / stroke circle with a settable width: draws the OUTLINE only,
// leaving the interior untouched so an existing stone fill shows through.
static void stroke_circle(GContext *ctx, GColor c, int cx, int cy, int outer, int thick) {
  graphics_context_set_stroke_color(ctx, c);
  graphics_context_set_stroke_width(ctx, (uint16_t)thick);
  graphics_draw_circle(ctx, GPoint(cx, cy), (uint16_t)outer);
}

static void draw_stone(GContext *ctx, uint8_t color, int x, int y, bool mark) {
  int cx = ix_x(x), cy = ix_y(y);
  if (color == 0) {
    fill_circle(ctx, CLR_BLACK_ST, cx, cy, STONE_R);
    stroke_circle(ctx, CLR_W_ST_OUT, cx, cy, STONE_R, 1);
  } else {
    fill_circle(ctx, CLR_WHITE_ST, cx, cy, STONE_R);
    stroke_circle(ctx, CLR_W_ST_OUT, cx, cy, STONE_R, 1);
  }
  if (mark) stroke_circle(ctx, CLR_MARKER, cx, cy, MARK_R, 2);
}

static void draw_board(GContext *ctx) {
  int grid_px = (BOARD_PX - 1) * SPACING;
  // board background (wood)
  graphics_context_set_fill_color(ctx, CLR_BOARD);
  graphics_fill_rect(ctx,
      GRect(s_board_x0 - SPACING * 6 / 10, s_board_y0 - SPACING * 6 / 10,
            grid_px + SPACING * 12 / 10, grid_px + SPACING * 12 / 10),
      0, GCornerNone);
  // grid lines
  graphics_context_set_stroke_color(ctx, CLR_GRID);
  graphics_context_set_stroke_width(ctx, 1);
  for (int i = 0; i < BOARD_PX; i++) {
    int p = ix_x(i);
    graphics_draw_line(ctx, GPoint(p, ix_y(0)), GPoint(p, ix_y(BOARD_PX - 1)));
    int q = ix_y(i);
    graphics_draw_line(ctx, GPoint(ix_x(0), q), GPoint(ix_x(BOARD_PX - 1), q));
  }
  // star points (hoshi)
  const int stars[][2] = {{2,2},{6,2},{2,6},{6,6},{4,4}};
  for (int i = 0; i < 5; i++) {
    fill_circle(ctx, CLR_GRID, ix_x(stars[i][0]), ix_y(stars[i][1]), 2);
  }
}

static void draw_turn_indicator(GContext *ctx, int turn_color) {
  GRect bounds = layer_get_bounds(s_canvas);
  int tx = bounds.size.w - 18;
  int ty = bounds.size.h - 16;
  if (turn_color == 0) {
    fill_circle(ctx, CLR_BLACK_ST, tx, ty, TURN_R);
    stroke_circle(ctx, CLR_W_ST_OUT, tx, ty, TURN_R, 1);
  } else {
    fill_circle(ctx, CLR_WHITE_ST, tx, ty, TURN_R);
    stroke_circle(ctx, CLR_W_ST_OUT, tx, ty, TURN_R, 1);
  }
  stroke_circle(ctx, CLR_GRAY, tx, ty, TURN_RING, 1);
}

static GSize measure_text(const char *text, GFont font) {
  GRect box = GRect(0, 0, 300, 100);
  return graphics_text_layout_get_content_size(text, font, box,
      GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft);
}

static void canvas_update_proc(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);

  // clear background
  graphics_context_set_fill_color(ctx, CLR_BG);
  graphics_fill_rect(ctx, b, 0, GCornerNone);

  draw_board(ctx);

  // stones
  for (int y = 0; y < BOARD; y++) {
    for (int x = 0; x < BOARD; x++) {
      if (!s_board[y][x]) continue;
      bool mark = s_has_mark && s_mark_x == x && s_mark_y == y;
      draw_stone(ctx, s_board[y][x] - 1, x, y, mark);
    }
  }

  // clock, bottom-left (12-hour with AM/PM)
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  int h = t->tm_hour % 12; if (h == 0) h = 12;
  const char *ampm = t->tm_hour < 12 ? "AM" : "PM";
  static char time_str[16];
  snprintf(time_str, sizeof(time_str), "%d:%02d %s", h, t->tm_min, ampm);

  GFont time_font = fonts_get_system_font(FONT_KEY_ROBOTO_CONDENSED_21);
  GFont small_font = fonts_get_system_font(FONT_KEY_GOTHIC_14);
  GRect tbox = GRect(8, b.size.h - 27, b.size.w - 16, 23);
  graphics_context_set_text_color(ctx, CLR_WHITE);
  graphics_draw_text(ctx, time_str, time_font, tbox,
      GTextOverflowModeFill, GTextAlignmentLeft, NULL);

  // battery %, just right of the clock (smaller text)
  static char batt_str[16];
  snprintf(batt_str, sizeof(batt_str), "%d%%", s_battery_pct);
  int t_w = measure_text(time_str, time_font).w;
  GRect bbox = GRect(8 + t_w + 6, b.size.h - 22, 60, 16);
  graphics_context_set_text_color(ctx, CLR_GRAY);
  graphics_draw_text(ctx, batt_str, small_font, bbox,
      GTextOverflowModeFill, GTextAlignmentLeft, NULL);

  // turn indicator stone, bottom-right
  int turn_color = (s_step + 1) % 2;
  draw_turn_indicator(ctx, turn_color);
}

// ---- reset timer ----
static void reset_timeout(void *data) {
  s_reset_timer = NULL;
  reset_board();
}

// ---- input: accelerometer tap is the ONLY watchface-native input ----
static void accel_tap_handler(AccelAxisType axis, int32_t direction) {
  APP_LOG(APP_LOG_LEVEL_INFO, "tap axis=%d dir=%ld", (int)axis, (long)direction);
  advance();
}

// ---- app focus: new random problem each time the face is (re)shown ----
static void focus_handler(bool in_focus) {
  if (in_focus) set_random_problem();
}

static void set_random_problem(void) {
  // choose a random difficulty set, then a random problem within it
  DiffId diff = (DiffId)(rand() % NUM_DIFF);
  uint16_t count = problems_load(diff);
  if (count == 0) return;
  uint16_t idx = (uint16_t)(rand() % count);
  if (problems_get(idx, &s_problem) != 0) return;
  reset_board();
  APP_LOG(APP_LOG_LEVEL_INFO, "goface-c loaded: %u %s", (unsigned)count, "set");
}

// ---- battery ----
static void battery_handler(BatteryChargeState state) {
  s_battery_pct = state.charge_percent;
  if (s_canvas) layer_mark_dirty(s_canvas);
}

// ---- time tick (minute) ----
static void tick_handler(struct tm *tick, TimeUnits units_changed) {
  if (s_canvas) layer_mark_dirty(s_canvas);
}

static void main_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect b = layer_get_bounds(root);
  layout_board(b);
  s_canvas = layer_create(b);
  layer_set_update_proc(s_canvas, canvas_update_proc);
  layer_add_child(root, s_canvas);
}

static void main_window_unload(Window *window) {
  layer_destroy(s_canvas);
}

static void init(void) {
  srand(time(NULL));
  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);
  window_set_window_handlers(s_window, (WindowHandlers){
    .load = main_window_load,
    .unload = main_window_unload,
  });
  window_stack_push(s_window, true);

  // battery
  battery_state_service_subscribe(battery_handler);
  battery_handler(battery_state_service_peek());

  // time
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);

  // accelerometer tap = advance
  accel_tap_service_subscribe(accel_tap_handler);

  // new random problem on (re)focus
  app_focus_service_subscribe(focus_handler);

  set_random_problem();
  APP_LOG(APP_LOG_LEVEL_INFO, "goface-c init done");
}

static void deinit(void) {
  if (s_reset_timer) app_timer_cancel(s_reset_timer);
  accel_tap_service_unsubscribe();
  battery_state_service_unsubscribe();
  app_focus_service_unsubscribe();
  tick_timer_service_unsubscribe();
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}