#include <pebble.h>
#include <string.h>
#include "problems.h"
#include "settings.h"
#include "message_keys.auto.h"

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
#define CLR_DONE     GColorFromRGB(0x22, 0xc0, 0x3c) // puzzle-complete green

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
static int s_temperature = 0;      // outside temp (C), 0 = not fetched
static bool s_has_weather = false;
static Settings s_settings;

static AppTimer *s_reset_timer = NULL;       // reset board to setup
static AppTimer *s_new_problem_timer = NULL; // load a fresh problem

static int ix_x(int i) { return s_board_x0 + i * SPACING; }
static int ix_y(int i) { return s_board_y0 + i * SPACING; }

// fwd decls (defined later in file)
static void reset_board(void);
static void reset_timeout(void *data);
static void set_random_problem(void);
static void new_problem_timeout(void *data);
static void advance(void);
static void cancel_timers(void);
static void request_weather(void);

// ---- layout ----
static void layout_board(GRect b) {
  int grid_px = (BOARD_PX - 1) * SPACING;
  s_board_x0 = (b.size.w - grid_px) / 2;
  s_board_y0 = (b.size.h - grid_px) / 2 - 10;
  if (s_board_y0 < 2) s_board_y0 = 2;
}

// ---- timers ----
static void cancel_timers(void) {
  if (s_reset_timer) { app_timer_cancel(s_reset_timer); s_reset_timer = NULL; }
  if (s_new_problem_timer) { app_timer_cancel(s_new_problem_timer); s_new_problem_timer = NULL; }
}

// Arm both inactivity timers (called on each tap/advance).
static void arm_timers(void) {
  cancel_timers();
  if (s_settings.reset_ms > 0)
    s_reset_timer = app_timer_register((uint32_t)s_settings.reset_ms, reset_timeout, NULL);
  if (s_settings.new_problem_ms > 0)
    s_new_problem_timer = app_timer_register((uint32_t)s_settings.new_problem_ms, new_problem_timeout, NULL);
}

// ---- board state ----
static void reset_board(void) {
  cancel_timers();
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

  arm_timers();
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

static void draw_turn_indicator(GContext *ctx, int turn_color, bool done) {
  GRect bounds = layer_get_bounds(s_canvas);
  int tx = bounds.size.w - 18;
  int ty = bounds.size.h - 16;
  if (done) {
    // puzzle complete: show a solid green circle in place of color-to-play
    fill_circle(ctx, CLR_DONE, tx, ty, TURN_R);
    stroke_circle(ctx, CLR_GRAY, tx, ty, TURN_RING, 1);
    return;
  }
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

  // clock, bottom-left (12-hour AM/PM by default; 24-hour if configured)
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  static char time_str[16];
  if (s_settings.clock_24h) {
    snprintf(time_str, sizeof(time_str), "%02d:%02d", t->tm_hour, t->tm_min);
  } else {
    int h = t->tm_hour % 12; if (h == 0) h = 12;
    const char *ampm = t->tm_hour < 12 ? "AM" : "PM";
    snprintf(time_str, sizeof(time_str), "%d:%02d %s", h, t->tm_min, ampm);
  }

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
  int bx = 8 + t_w + 6;
  GRect bbox = GRect(bx, b.size.h - 22, 60, 16);
  graphics_context_set_text_color(ctx, CLR_GRAY);
  graphics_draw_text(ctx, batt_str, small_font, bbox,
      GTextOverflowModeFill, GTextAlignmentLeft, NULL);

  // outside temperature, just right of the battery (same small font)
  if (s_has_weather) {
    int b_w = measure_text(batt_str, small_font).w;
    static char temp_str[16];
    snprintf(temp_str, sizeof(temp_str), "%d°", s_temperature);
    GRect temp_box = GRect(bx + b_w + 6, b.size.h - 22, 60, 16);
    graphics_context_set_text_color(ctx, CLR_GRAY);
    graphics_draw_text(ctx, temp_str, small_font, temp_box,
        GTextOverflowModeFill, GTextAlignmentLeft, NULL);
  }

  // turn indicator stone, bottom-right; green when the puzzle is solved
  int turn_color = (s_step + 1) % 2;
  bool done = (s_step + 1) >= (int)s_problem.line_len;
  draw_turn_indicator(ctx, turn_color, done);
}

// ---- reset timer (returns board to setup after idle) ----
static void reset_timeout(void *data) {
  s_reset_timer = NULL;
  reset_board();
}

// ---- new-problem timer (loads a fresh problem after longer idle) ----
static void new_problem_timeout(void *data) {
  s_new_problem_timer = NULL;
  set_random_problem();
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
  s_new_problem_timer = NULL;   // fired; don't re-arm here
  // choose difficulty per settings (0 = random across all sets)
  DiffId diff;
  if (s_settings.problem_set == 0) {
    diff = (DiffId)(rand() % NUM_DIFF);
  } else {
    diff = (DiffId)(s_settings.problem_set - 1);
    if (diff > NUM_DIFF - 1) diff = (DiffId)(rand() % NUM_DIFF);
  }
  uint16_t count = problems_load(diff);
  if (count == 0) return;
  uint16_t idx = (uint16_t)(rand() % count);
  if (problems_get(idx, &s_problem) != 0) return;
  // random mirror/rotation (0..7 = all 8 board symmetries) for replayability;
  // the 9x9 grid maps onto itself under every one, so the puzzle stays legal.
  problems_apply_transform(&s_problem, (uint8_t)(rand() % 8));
  reset_board();
  APP_LOG(APP_LOG_LEVEL_INFO, "goface-c loaded: %u set=%d", (unsigned)count, (int)s_settings.problem_set);
}

// ---- battery ----
static void battery_handler(BatteryChargeState state) {
  s_battery_pct = state.charge_percent;
  if (s_canvas) layer_mark_dirty(s_canvas);
}

// ---- time tick (minute) ----
static void tick_handler(struct tm *tick, TimeUnits units_changed) {
  if (s_canvas) layer_mark_dirty(s_canvas);

  // refresh weather every 30 minutes
  if (tick->tm_min % 30 == 0) {
    request_weather();
  }
}

// ---- weather request (ask the phone to fetch Open-Meteo temp + send back) ----
static void request_weather(void) {
  DictionaryIterator *iter;
  if (app_message_outbox_begin(&iter) == APP_MSG_OK) {
    dict_write_uint8(iter, MESSAGE_KEY_REQUEST_WEATHER, 1);
    if (app_message_outbox_send() != APP_MSG_OK) {
      APP_LOG(APP_LOG_LEVEL_WARNING, "outbox send failed");
    }
  }
}

// ---- phone config (Clay -> AppMessage) ----
static int num_tuple(DictionaryIterator *iter, uint32_t key, int fallback) {
  Tuple *t = dict_find(iter, key);
  if (!t) return fallback;
  if (t->type == TUPLE_CSTRING) return atoi(t->value->cstring);
  return (int)t->value->int32;
}

static void inbox_received_handler(DictionaryIterator *iter, void *context) {
  bool changed = false;

  // outside temperature from the phone (Open-Meteo via pkjs); does NOT count
  // as a settings change (we don't reload a fresh problem for it).
  Tuple *temp_t = dict_find(iter, MESSAGE_KEY_TEMPERATURE);
  if (temp_t) {
    s_temperature = (int)temp_t->value->int32;
    s_has_weather = true;
    if (s_canvas) layer_mark_dirty(s_canvas);
    APP_LOG(APP_LOG_LEVEL_INFO, "temp %d", s_temperature);
  }

  // Units came from the config page: just re-request weather so the displayed
  // temp reflects the new unit. Not a settings change (no problem reload).
  if (dict_find(iter, MESSAGE_KEY_Units)) {
    request_weather();
  }

  Tuple *ps = dict_find(iter, MESSAGE_KEY_ProblemSet);
  if (ps) {
    int v = (ps->type == TUPLE_CSTRING) ? atoi(ps->value->cstring) : (int)ps->value->int32;
    if (v >= 0 && v <= 3) { s_settings.problem_set = (int8_t)v; changed = true; }
  }
  int reset_s = num_tuple(iter, MESSAGE_KEY_ResetSeconds, -1);
  if (reset_s >= 0) { s_settings.reset_ms = reset_s * 1000; changed = true; }
  int new_s = num_tuple(iter, MESSAGE_KEY_NewProblemSeconds, -1);
  if (new_s >= 0) { s_settings.new_problem_ms = new_s * 1000; changed = true; }
  Tuple *cf = dict_find(iter, MESSAGE_KEY_ClockFormat);
  if (cf) {
    const char *s = (cf->type == TUPLE_CSTRING) ? cf->value->cstring : NULL;
    bool c24 = s ? (strcmp(s, "24") == 0) : (cf->value->int32 == 24);
    s_settings.clock_24h = c24; changed = true;
  }

  if (changed) {
    settings_save(&s_settings);
    cancel_timers();
    set_random_problem();   // re-pick a problem respecting the new difficulty
    if (s_canvas) layer_mark_dirty(s_canvas);
    APP_LOG(APP_LOG_LEVEL_INFO, "goface-c settings updated");
  }
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
  settings_load(&s_settings);

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

  // phone config (Clay) -> AppMessage
  app_message_register_inbox_received(inbox_received_handler);
  app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());

  // ask the phone for outside temperature (Open-Meteo via pkjs)
  request_weather();

  set_random_problem();
  APP_LOG(APP_LOG_LEVEL_INFO, "goface-c init done");
}

static void deinit(void) {
  cancel_timers();
  app_message_deregister_callbacks();
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