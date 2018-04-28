#include "pebble.h"

// Defining global variables

static Window *g_window; // Main window
GPoint g_clock_center; // Center of clock
struct tm* g_time; // Store global time structure
bool g_bluetooth_status; // Bluetooth status, connected or not

// Watchface
int g_watchface_width; 
int g_watchface_height;
GRect g_watchface_rect;

// Hour ticks
BitmapLayer* g_hour_ticks_layer;
GBitmap* g_hour_ticks_image;

// Minute ticks
BitmapLayer* g_minute_ticks_layer;
GBitmap* g_minute_ticks_image;

// Watchface numbers
BitmapLayer* g_watchface_numbers_layer;
GBitmap* g_watchface_numbers_image;

// Battery bar
Layer* g_battery_block; // Battery block pointer
int g_battery_meter_thickness; // Thickness of battery meter
int g_battery_level; // Battery percentage
GRect g_battery_block_rect; // Battery block bounding area

// Clock
Layer *g_clock_layer;
uint16_t g_clock_radius;

// Hands layer
static Layer *g_hands_layer;
int g_hour_hand_width;
int g_hour_hand_length;
int g_minute_hand_width;
int g_minute_hand_length;
uint16_t g_center_dot_radius;

// Date block
TextLayer* g_date_block;
GRect g_date_block_rect;

// Day block
TextLayer* g_day_block;
GRect g_day_block_rect;

// Temperature block
TextLayer* g_temp_block;
GRect g_temp_block_rect;

// Icon block
GBitmap* g_icon_image; // icon image pointer
BitmapLayer* g_icon_block; // icon block pointer
GRect g_icon_block_rect; // icon block bounding area

// Enum to select weather icon
enum ICON_NUMBER {
    ICON_DEFAULT,
    ICON_CLEAR_DAY,
    ICON_CLEAR_NIGHT,
    ICON_CLOUDY,
    ICON_FOG,
    ICON_PARTLYCLOUDY,
    ICON_PARTLYSUNNY,
    ICON_RAIN,
    ICON_SLEET,
    ICON_SNOW,
    ICON_TSTORMS,
    ICON_SUNRISE,
    ICON_SUNSET
};


// Define settings structure for persintent storage
typedef struct PersistSettings {
  GColor color_main_bg;
  GColor color_clock_bg;
  GColor color_battery_meter;
  GColor color_hour_hand;
  GColor color_minute_hand;
  GColor color_center_dot;
  GColor color_main_stroke;
  GColor color_clock_stroke;
  bool show_battery_meter;
  bool show_watchface_numbers;
  bool show_hour_ticks;
  bool show_minute_ticks;
  bool vibrate_disconnect;
  int update_interval;
  char temperature[8];
  char day[10];
  char date[10];
  char conditions[32];
  char date_format[10];
  int icon_number;
} PersistSettings;

// An instance of the struct
static PersistSettings persist_settings;

// Initialize all global variables
void init_globals(){

  // Set default persistent settings
  persist_settings.color_main_bg = GColorBlack;
  persist_settings.color_clock_bg = GColorWhite;
  persist_settings.color_battery_meter = GColorWhite;
  persist_settings.color_hour_hand = GColorBlack;
  persist_settings.color_minute_hand = GColorBlack;
  persist_settings.color_center_dot = GColorWhite;
  persist_settings.color_main_stroke = GColorWhite;
  persist_settings.color_clock_stroke = GColorBlack;
  persist_settings.show_battery_meter = true;
  persist_settings.show_watchface_numbers = true;
  persist_settings.show_hour_ticks = true;
  persist_settings.show_minute_ticks = true;
  persist_settings.vibrate_disconnect = false;
  persist_settings.update_interval = 20;
  snprintf(persist_settings.temperature, sizeof(persist_settings.temperature), " ");
  snprintf(persist_settings.day, sizeof(persist_settings.day), " ");
  snprintf(persist_settings.date, sizeof(persist_settings.date), " ");
  snprintf(persist_settings.conditions, sizeof(persist_settings.conditions), " ");
  snprintf(persist_settings.date_format, sizeof(persist_settings.date_format), "%%m/%%d");
  persist_settings.icon_number = ICON_DEFAULT;
  
  // Read settings from persistent storage to override defaults if they exist
  // Always check if data exists to prevent crash while reading
  if (persist_exists(MESSAGE_KEY_SETTINGS)) {
    // Read persisted value
    persist_read_data(MESSAGE_KEY_SETTINGS, &persist_settings, sizeof(persist_settings));
  }
  
  // Global time initialize
  time_t now = time(NULL);
  g_time = localtime(&now);
  
  // Set other global veriables
  g_clock_center = GPoint(74,84);
  
  // Watchface
  g_watchface_width = 129; // Must be odd
  g_watchface_height = 129; // Must be odd
  g_watchface_rect = GRect(g_clock_center.x-(g_watchface_width)/2,g_clock_center.y-(g_watchface_height)/2,g_watchface_width,g_watchface_height);

  // Battery meter
  g_battery_meter_thickness = 5;
  g_battery_block_rect = GRect(g_watchface_rect.origin.x+1,
                               g_watchface_rect.origin.y+1,
                               g_watchface_rect.size.w-2,
                               g_watchface_rect.size.h-2
                              );
  
  // Clock
  g_clock_radius = (g_watchface_width/2)-6;

  // Clock hands
  g_hour_hand_width = 4;
  g_hour_hand_length = 25;
  g_minute_hand_width = 4;
  g_minute_hand_length = 45;
  g_center_dot_radius = 1;
  
  // Date block
  g_date_block_rect = GRect(72,-1,68,30);
  
  // Day block
  g_day_block_rect = GRect(4,-1,72,30);
  
  // Temperature block
  g_temp_block_rect = GRect(74,133,66,34);
  
  // Icon block
  g_icon_block_rect = GRect(4,124,43,38); 

}

// Function to update date block
void update_date_block(){

  strftime(persist_settings.date, sizeof(persist_settings.date),persist_settings.date_format,g_time);
  text_layer_set_text(g_date_block,persist_settings.date);
}

// Function to update day block
void update_day_block(){

  if(g_bluetooth_status)
    strftime(persist_settings.day, sizeof(persist_settings.day),"%a",g_time);
  else
    strftime(persist_settings.day, sizeof(persist_settings.day),"%a !",g_time);
  text_layer_set_text(g_day_block,persist_settings.day);
}

// Callback function which gets called when bluetooth conneciton updates
static void bluetooth_callback(bool connected) {
  g_bluetooth_status = connected;
  if(persist_settings.vibrate_disconnect){
    if(!connected)
      vibes_double_pulse(); // Vibrate if disconnected
  }
  update_day_block(); // Day block shows bluetooth status
}

// Funciton called to update the battery block
static void battery_update_proc(Layer *layer, GContext *ctx){
  // Battery
  {
    graphics_context_set_stroke_color(ctx,persist_settings.color_battery_meter);
    graphics_context_set_stroke_width(ctx,g_battery_meter_thickness);
    graphics_draw_arc(ctx,g_battery_block_rect,GOvalScaleModeFitCircle,DEG_TO_TRIGANGLE(0),((float)g_battery_level/100.0F)*TRIG_MAX_ANGLE);
    //graphics_draw_arc(ctx,g_battery_block_rect,GOvalScaleModeFitCircle,DEG_TO_TRIGANGLE(0),TRIG_MAX_ANGLE);
  }
}

// Callback function which gets called when battery status changes
static void battery_callback(BatteryChargeState state) {
  g_battery_level = state.charge_percent; // Record the new battery level
  layer_mark_dirty(g_battery_block); // Set layer to be redrawn
}

// Function to update watchface
void update_watchface(){
  
  // Minute ticks
  {
    // Modify minute ticks image by determining whether color is supported
    GColor* palette_minute_ticks = gbitmap_get_palette(g_minute_ticks_image);
    
    #if defined(PBL_COLOR)
    palette_minute_ticks[0] = persist_settings.color_clock_stroke; // Clock stroke color
    palette_minute_ticks[1] = GColorClear; // Background color
    #elif defined(PBL_BW)
    palette_minute_ticks[0] = GColorBlack; // Foreground color
    palette_minute_ticks[1] = GColorWhite; // Background color
    #endif
  
    bitmap_layer_set_bitmap(g_minute_ticks_layer,g_minute_ticks_image); // Automatically marks dirty
  }
  
  // Hour ticks
  {
    // Modify hour ticks image by determining whether color is supported
    GColor* palette_hour_ticks = gbitmap_get_palette(g_hour_ticks_image);
    
    #if defined(PBL_COLOR)
    palette_hour_ticks[0] = persist_settings.color_clock_stroke; // Clock stroke color
    palette_hour_ticks[1] = GColorClear; // Background color
    #elif defined(PBL_BW)
    palette_hour_ticks[0] = GColorBlack; // Foreground color
    palette_hour_ticks[1] = GColorWhite; // Background color
    #endif
  
    bitmap_layer_set_bitmap(g_hour_ticks_layer,g_hour_ticks_image); // Automatically marks dirty
    
  }
  
  // Watchface numbers
  {
    // Modify watchface numbers image by determining whether color is supported
    GColor* palette_watchface_numbers = gbitmap_get_palette(g_watchface_numbers_image);
    
    #if defined(PBL_COLOR)
    palette_watchface_numbers[0] = persist_settings.color_clock_stroke; // Clock stroke color
    palette_watchface_numbers[1] = GColorClear; // Background color
    #elif defined(PBL_BW)
    palette_watchface_numbers[0] = GColorBlack; // Foreground color
    palette_watchface_numbers[1] = GColorWhite; // Background color
    #endif
  
    bitmap_layer_set_bitmap(g_watchface_numbers_layer,g_watchface_numbers_image); // Automatically marks dirty
  }
  
}

// Function to update the icon
void update_icon(int icon_number){
  
  // Debug
  // icon_number = ICON_SNOW;
  
  // Clear memory if used
  if(g_icon_image)
    gbitmap_destroy(g_icon_image);
  
  // Find the correct icon
  switch(icon_number){
    case ICON_CLEAR_DAY:
    g_icon_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_CLEAR_DAY);
    break;
    
    case ICON_CLEAR_NIGHT:
    g_icon_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_CLEAR_NIGHT);
    break;
    
    case ICON_CLOUDY:
    g_icon_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_CLOUDY);
    break;
    
    case ICON_FOG:
    g_icon_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_FOG);
    break;
    
    case ICON_PARTLYCLOUDY:
    g_icon_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_PARTLYCLOUDY);
    break;
    
    case ICON_PARTLYSUNNY:
    g_icon_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_PARTLYSUNNY);
    break;
    
    case ICON_RAIN:
    g_icon_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_RAIN);
    break;
    
    case ICON_SLEET:
    g_icon_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_SLEET);
    break;
    
    case ICON_SNOW:
    g_icon_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_SNOW);
    break;
    
    case ICON_TSTORMS:
    g_icon_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_TSTORMS);
    break;
    
    case ICON_SUNRISE:
    g_icon_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_SUNRISE);
    break;
    
    case ICON_SUNSET:
    g_icon_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_SUNSET);
    break;
    
    default:
    g_icon_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_CLEAR_DAY);
  }
  
  // Modify image by determining whether color is supported
  GColor* palette = gbitmap_get_palette(g_icon_image);
  
  #if defined(PBL_COLOR)
  palette[0] = persist_settings.color_main_stroke; // Main color
  palette[1] = GColorClear; // Background color
  #elif defined(PBL_BW)
  palette[0] = GColorBlack; // Foreground color
  palette[1] = GColorWhite; // Background color
  #endif

  bitmap_layer_set_bitmap(g_icon_block,g_icon_image); // Automatically marks dirty
}

static void clock_update_proc(Layer *layer, GContext *ctx) {
  
    graphics_context_set_fill_color(ctx,persist_settings.color_clock_bg);
    graphics_fill_circle(ctx,g_clock_center,g_clock_radius);
}


// Handler for clock hands
static void hands_update_proc(Layer *layer, GContext *ctx) {

  // hour hand
  {
    int32_t hour_angle = (TRIG_MAX_ANGLE * (((g_time->tm_hour % 12) * 6) + (g_time->tm_min / 10))) / (12 * 6);
    GPoint hour_hand = {
      .x = (int16_t)(sin_lookup(hour_angle) * (int32_t)g_hour_hand_length / TRIG_MAX_RATIO) + g_clock_center.x,
      .y = (int16_t)(-cos_lookup(hour_angle) * (int32_t)g_hour_hand_length / TRIG_MAX_RATIO) + g_clock_center.y,
    };
    graphics_context_set_stroke_color(ctx,persist_settings.color_hour_hand);
    graphics_context_set_stroke_width(ctx,g_hour_hand_width);
    graphics_draw_line(ctx,hour_hand, g_clock_center);
  }

  // minute hand
  {
   
    int32_t minute_angle = TRIG_MAX_ANGLE * g_time->tm_min / 60;
    GPoint minute_hand = {
      .x = (int16_t)(sin_lookup(minute_angle) * (int32_t)g_minute_hand_length / TRIG_MAX_RATIO) + g_clock_center.x,
      .y = (int16_t)(-cos_lookup(minute_angle) * (int32_t)g_minute_hand_length / TRIG_MAX_RATIO) + g_clock_center.y,
    };
    graphics_context_set_stroke_color(ctx,persist_settings.color_minute_hand);
    graphics_context_set_stroke_width(ctx,g_minute_hand_width);
    graphics_draw_line(ctx,minute_hand, g_clock_center);
  }

  // Center dot
  {
    graphics_context_set_fill_color(ctx,persist_settings.color_center_dot);
    graphics_fill_circle(ctx,g_clock_center,g_center_dot_radius);
  }

}

// Handler for minute ticks
static void handler_minute_tick(struct tm *tick_time, TimeUnits units_changed) {
  
  // Update global time
  time_t now = time(NULL);
  g_time = localtime(&now);
  
  update_date_block();
  
  update_day_block();

  layer_mark_dirty(g_hands_layer);
  
  // Get weather update every x minutes
  if(tick_time->tm_min % persist_settings.update_interval == 0) {
    
    // Begin dictionary
    DictionaryIterator *iter;
    app_message_outbox_begin(&iter);

    // Add a key-value pair
    dict_write_uint8(iter,0,0); // Send blank message
    dict_write_end(iter);

    // Send the message!
    app_message_outbox_send();
  }
}


// Loading the main window
static void window_load(Window *window) {

  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  // Create date block
  {
    g_date_block = text_layer_create(g_date_block_rect);
    text_layer_set_background_color(g_date_block,GColorClear);
    text_layer_set_text_color(g_date_block,persist_settings.color_main_stroke);
    text_layer_set_text(g_date_block,persist_settings.date);
    text_layer_set_text_alignment(g_date_block, GTextAlignmentRight);
    text_layer_set_font(g_date_block, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
    layer_add_child(window_layer,text_layer_get_layer(g_date_block));
  }
  
  // Create day block
  {
    g_day_block = text_layer_create(g_day_block_rect);
    text_layer_set_background_color(g_day_block,GColorClear);
    text_layer_set_text_color(g_day_block,persist_settings.color_main_stroke);
    text_layer_set_text(g_day_block,persist_settings.day);
    text_layer_set_text_alignment(g_day_block, GTextAlignmentLeft);
    text_layer_set_font(g_day_block, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
    layer_add_child(window_layer,text_layer_get_layer(g_day_block));
  }
  
  // Create temperature block
  {
    g_temp_block = text_layer_create(g_temp_block_rect);
    text_layer_set_background_color(g_temp_block,GColorClear);
    text_layer_set_text_color(g_temp_block,persist_settings.color_main_stroke);
    text_layer_set_text(g_temp_block,persist_settings.temperature);
    text_layer_set_text_alignment(g_temp_block, GTextAlignmentRight);
    text_layer_set_font(g_temp_block, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
    layer_add_child(window_layer,text_layer_get_layer(g_temp_block));
  }
  
  // Create icon block
  {
    // Create the layer
    g_icon_block = bitmap_layer_create(g_icon_block_rect);
    bitmap_layer_set_compositing_mode(g_icon_block,GCompOpSet); // For transperency in Basalt
    bitmap_layer_set_alignment(g_icon_block, GAlignBottomLeft);
    
    update_icon(persist_settings.icon_number); // Create the icon
    
    // Add to window
    layer_add_child(window_layer,bitmap_layer_get_layer(g_icon_block));
  }
  
  // Create clock layer
  {
    g_clock_layer = layer_create(bounds);
    layer_set_update_proc(g_clock_layer, clock_update_proc);
    layer_add_child(window_layer, g_clock_layer);
  }
  
  // Create hour ticks layer
  {
    g_hour_ticks_layer = bitmap_layer_create(g_watchface_rect);
    g_hour_ticks_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_HOUR_TICKS);
    bitmap_layer_set_compositing_mode(g_hour_ticks_layer,GCompOpSet); // For transperency in Basalt
    bitmap_layer_set_alignment(g_hour_ticks_layer, GAlignCenter);
    layer_add_child(window_layer,bitmap_layer_get_layer(g_hour_ticks_layer));
  }
  
  // Create minute ticks layer
  {
    g_minute_ticks_layer = bitmap_layer_create(g_watchface_rect);
    g_minute_ticks_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_MINUTE_TICKS);
    bitmap_layer_set_compositing_mode(g_minute_ticks_layer,GCompOpSet); // For transperency in Basalt
    bitmap_layer_set_alignment(g_minute_ticks_layer, GAlignCenter);
    layer_add_child(window_layer,bitmap_layer_get_layer(g_minute_ticks_layer));
  }
  
  // Create watchface numbers layer
  {
    g_watchface_numbers_layer = bitmap_layer_create(g_watchface_rect);
    g_watchface_numbers_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_WATCHFACE_NUMBERS);
    bitmap_layer_set_compositing_mode(g_watchface_numbers_layer,GCompOpSet); // For transperency in Basalt
    bitmap_layer_set_alignment(g_watchface_numbers_layer, GAlignCenter);
    layer_add_child(window_layer,bitmap_layer_get_layer(g_watchface_numbers_layer));
  }
  
  // Update the watchface
  update_watchface();

  // Create hands layer
  {
    g_hands_layer = layer_create(bounds);
    layer_set_update_proc(g_hands_layer, hands_update_proc);
    layer_add_child(window_layer, g_hands_layer);
  }
  
  // Create battery block 
  {
    g_battery_block = layer_create(bounds); // We need the frame to the whole window so there is no clipping
    layer_set_update_proc(g_battery_block, battery_update_proc); // Only works with stock layer
    layer_add_child(window_layer,g_battery_block);
  }
}

// Unloading the main window
static void window_unload(Window *window) {

  // Destroy date layer
  text_layer_destroy(g_date_block);
  
  // Destroy day layer
  text_layer_destroy(g_day_block);
  
  // Destroy temperature layer
  text_layer_destroy(g_temp_block);

  // Destroy hands layer
  layer_destroy(g_hands_layer);
  
  // Destroy clock layer
  layer_destroy(g_clock_layer);
  
  // Destroy icon block
  gbitmap_destroy(g_icon_image); // Destroy icon block
  bitmap_layer_destroy(g_icon_block); // Destroy PDC image
  
}

// Dealing with messages
static void inbox_received_callback(DictionaryIterator *iterator, void *context) {

  // Read tuples for data
  // They are prefixed with "MESSAGE_KEY_" and automatically included in pebble.h

  // Temperature
  Tuple *temp_tuple = dict_find(iterator, MESSAGE_KEY_TEMPERATURE);
  if(temp_tuple){
    snprintf(persist_settings.temperature, sizeof(persist_settings.temperature), "%d°", (int)temp_tuple->value->int32);
    text_layer_set_text(g_temp_block,persist_settings.temperature);
  }

  // conditions
  Tuple *conditions_tuple = dict_find(iterator, MESSAGE_KEY_CONDITIONS);
  if(conditions_tuple){
    snprintf(persist_settings.conditions, sizeof(persist_settings.conditions), "%s", conditions_tuple->value->cstring);
  }

  // icon
  Tuple *icon_tuple = dict_find(iterator, MESSAGE_KEY_ICON_NUMBER); // Get the icon number
  if(icon_tuple){
    // Update the icon only if it has changed
    if(persist_settings.icon_number != (int)icon_tuple->value->int32){
      persist_settings.icon_number = (int)icon_tuple->value->int32; // Update the global current icon number
      update_icon(persist_settings.icon_number);
    }
  }
  
  // show hour ticks
  Tuple *show_hour_ticks_tuple = dict_find(iterator, MESSAGE_KEY_SHOW_HOUR_TICKS); 
  if(show_hour_ticks_tuple){
    if(persist_settings.show_hour_ticks != (show_hour_ticks_tuple->value->int32==1)){
      persist_settings.show_hour_ticks = show_hour_ticks_tuple->value->int32==1; // get the bool value
      layer_set_hidden(bitmap_layer_get_layer(g_hour_ticks_layer),!persist_settings.show_hour_ticks); // Hide or show conditions layer
    }
  }
  
  // show minute ticks
  Tuple *show_minute_ticks_tuple = dict_find(iterator, MESSAGE_KEY_SHOW_MINUTE_TICKS); 
  if(show_minute_ticks_tuple){
    if(persist_settings.show_minute_ticks != (show_minute_ticks_tuple->value->int32==1)){
      persist_settings.show_minute_ticks = show_minute_ticks_tuple->value->int32==1; // get the bool value
      layer_set_hidden(bitmap_layer_get_layer(g_minute_ticks_layer),!persist_settings.show_minute_ticks); // Hide or show conditions layer
    }
  }
  
  // show watchface numbers
  Tuple *show_watchface_numbers_tuple = dict_find(iterator, MESSAGE_KEY_SHOW_WATCHFACE_NUMBERS); 
  if(show_watchface_numbers_tuple){
    if(persist_settings.show_watchface_numbers != (show_watchface_numbers_tuple->value->int32==1)){
      persist_settings.show_watchface_numbers = show_watchface_numbers_tuple->value->int32==1; // get the bool value
      layer_set_hidden(bitmap_layer_get_layer(g_watchface_numbers_layer),!persist_settings.show_watchface_numbers); // Hide or show conditions layer
    }
  }
  
  // show battery bar
  Tuple *show_battery_meter_tuple = dict_find(iterator, MESSAGE_KEY_SHOW_BATTERY_METER); // Get the watchface number
  if(show_battery_meter_tuple){
    if(persist_settings.show_battery_meter != (show_battery_meter_tuple->value->int32==1)){
      persist_settings.show_battery_meter = show_battery_meter_tuple->value->int32==1; // get the bool value
      layer_set_hidden(g_battery_block,!persist_settings.show_battery_meter); // Hide or show conditions layer
    }
  }

  // Vibrate on disconnect
  Tuple *vibrate_disconnect_tuple = dict_find(iterator, MESSAGE_KEY_VIBRATE_DISCONNECT);
  if(vibrate_disconnect_tuple){
    persist_settings.vibrate_disconnect = vibrate_disconnect_tuple->value->int32==1; // get bool value
  }

  // Update interval
  Tuple *update_interval_tuple = dict_find(iterator, MESSAGE_KEY_UPDATE_INTERVAL);
  if(update_interval_tuple){
    persist_settings.update_interval = atoi(update_interval_tuple->value->cstring); // Convert string to int
  }

  // Main background color
  Tuple *color_main_bg_tuple = dict_find(iterator, MESSAGE_KEY_COLOR_MAIN_BG);
  if(color_main_bg_tuple) {
    persist_settings.color_main_bg = GColorFromHEX(color_main_bg_tuple->value->int32);
    window_set_background_color(g_window,persist_settings.color_main_bg); // Set background color
  }
  
  // Clock background color
  Tuple *color_clock_bg_tuple = dict_find(iterator, MESSAGE_KEY_COLOR_CLOCK_BG);
  if(color_clock_bg_tuple){
    persist_settings.color_clock_bg = GColorFromHEX(color_clock_bg_tuple->value->int32);
    layer_mark_dirty(g_clock_layer);
  }
  
  // Battery bar color
  Tuple *color_battery_meter_tuple = dict_find(iterator, MESSAGE_KEY_COLOR_BATTERY_METER);
  if(color_battery_meter_tuple) {
    persist_settings.color_battery_meter = GColorFromHEX(color_battery_meter_tuple->value->int32);
    layer_mark_dirty(g_battery_block); // Set layer to be redrawn
  }
  
  // Hour hand color
  Tuple *color_hour_hand_tuple = dict_find(iterator, MESSAGE_KEY_COLOR_HOUR_HAND);
  if(color_hour_hand_tuple) {
    persist_settings.color_hour_hand = GColorFromHEX(color_hour_hand_tuple->value->int32);
    layer_mark_dirty(g_hands_layer);
  }
  
  // Minute hand color
  Tuple *color_minute_hand_tuple = dict_find(iterator, MESSAGE_KEY_COLOR_MINUTE_HAND);
  if(color_minute_hand_tuple) {
    persist_settings.color_minute_hand = GColorFromHEX(color_minute_hand_tuple->value->int32);
    layer_mark_dirty(g_hands_layer);
  }
  
  // Center dot color
  Tuple *color_center_dot_tuple = dict_find(iterator, MESSAGE_KEY_COLOR_CENTER_DOT);
  if(color_center_dot_tuple) {
    persist_settings.color_center_dot = GColorFromHEX(color_center_dot_tuple->value->int32);
    layer_mark_dirty(g_hands_layer);
  }
  
  // Main stroke color
  Tuple *color_main_stroke_tuple = dict_find(iterator, MESSAGE_KEY_COLOR_MAIN_STROKE);
  if(color_main_stroke_tuple) {
    persist_settings.color_main_stroke = GColorFromHEX(color_main_stroke_tuple->value->int32);
    text_layer_set_text_color(g_date_block,persist_settings.color_main_stroke);
    text_layer_set_text_color(g_day_block,persist_settings.color_main_stroke);
    text_layer_set_text_color(g_temp_block,persist_settings.color_main_stroke);
    update_icon(persist_settings.icon_number);
  }
  
  // Clock stroke color
  Tuple *color_clock_stroke_tuple = dict_find(iterator, MESSAGE_KEY_COLOR_CLOCK_STROKE);
  if(color_clock_stroke_tuple) {
    persist_settings.color_clock_stroke = GColorFromHEX(color_clock_stroke_tuple->value->int32);
    update_watchface();
  }
  
  // Date format
  Tuple *date_format_tuple = dict_find(iterator, MESSAGE_KEY_DATE_FORMAT);
  if(date_format_tuple) {
    snprintf(persist_settings.date_format, sizeof(persist_settings.date_format), "%s", date_format_tuple->value->cstring);
    update_date_block();
  }

  // Save all clay settings
  persist_write_data(MESSAGE_KEY_SETTINGS, &persist_settings, sizeof(persist_settings));

}

// Other callback functions
static void inbox_dropped_callback(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped!");
}
static void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed!");
}
static void outbox_sent_callback(DictionaryIterator *iterator, void *context) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Outbox send success!");
}


// Initialization
static void init() {

  // Create main window
  g_window = window_create();
  window_set_background_color(g_window,persist_settings.color_main_bg); // Set background color

  // Set handlers to manage the elements inside the Window
  window_set_window_handlers(g_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });

  // Show window on the watch, animated = true
  window_stack_push(g_window, true);

  // Register app message callback function
  app_message_register_inbox_received(inbox_received_callback);

  // Register other callbacks
  app_message_register_inbox_dropped(inbox_dropped_callback);
  app_message_register_outbox_failed(outbox_failed_callback);
  app_message_register_outbox_sent(outbox_sent_callback);

  // Open AppMessage
  app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());

  // Subscribe to tick service
  tick_timer_service_subscribe(MINUTE_UNIT, handler_minute_tick);
  
  // Use the initial time to update blocks that need it
  handler_minute_tick(g_time, MINUTE_UNIT);
  
  // Register the battery_callback function
  battery_state_service_subscribe(battery_callback);
  
  // Ensure that the battery level is displayed from the start
  battery_callback(battery_state_service_peek());
  
  // Register the bluetooth_callback function
  connection_service_subscribe((ConnectionHandlers) {
  .pebble_app_connection_handler = bluetooth_callback});
  
  // Show the currect state of the BT conneciton form the start
  bluetooth_callback(connection_service_peek_pebble_app_connection());
  
}

// Deinitialize
static void deinit() {
  tick_timer_service_unsubscribe();
  battery_state_service_unsubscribe();
  connection_service_unsubscribe();
  window_destroy(g_window);
}

// Main program
int main() {
  init_globals();
  init();
  app_event_loop();
  deinit();
}
