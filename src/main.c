#include "pebble.h"

static Window *window;
#define _DATE_BUF_LEN 26
static char _DATE_BUFFER[_DATE_BUF_LEN];
#define DATE_FORMAT "%l %B %e %T"
static TextLayer *glucose_layer;
static TextLayer *test_layer;
static TextLayer *timetolimit_layer;
static TextLayer *s_time_layer;
static TextLayer *date_layer;
static TextLayer *alert_layer;
char buf[5];
char glucbuf[15];
static char glucose[16];

static BitmapLayer *icon_layer;
static GBitmap *icon_bitmap = NULL;

static AppSync sync;
static uint8_t sync_buffer[64];
 long last_reading=0;
 uint8_t miss_count=0;
 uint8_t sensor_miss_count=0;
int16_t currentGlucose=0;
int16_t timeToLimit=0;

int watchCallbackCount=0;
int lastWatchCallbackCount=0;

//slope direction
static int SLOPE_DOWN = 0x01;
static int SLOPE_UP = 0x02;
int slopeDirection=0;
//
enum GlucoseKey {
  GLUCOSESTRING_KEY = 0x0,  
  GLUCOSE_KEY = 0x1,  
  ARROW_KEY=0x2,
  SLOPEDIRECTION_KEY=0x3,
  TIMETOLIMIT_KEY=0x4,
  LASTREADING_KEY=0x5
};

//arrows
static int ARROW_45_UP = 0x01;
static int ARROW_UP = 0x02;
static int ARROW_UP_UP = 0x03;
static int ARROW_45_DOWN = 0x04;
static int ARROW_DOWN = 0x05;
static int ARROW_DOWN_DOWN = 0x06;

static uint8_t ARROW=0x0;
static uint16_t alertCount=0;


static void sync_error_callback(DictionaryResult dict_error, AppMessageResult app_message_error, void *context) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "App Message Sync Error: %d", app_message_error);
}


static void alerts(){
  APP_LOG(APP_LOG_LEVEL_DEBUG,"In Alerts");
  // Vibe pattern: ON for 200ms, OFF for 100ms, ON for 400ms:
uint32_t  segments[] = { 200, 100, 200 };
VibePattern pat = {
  .durations = segments,
  .num_segments = ARRAY_LENGTH(segments),
};
  
uint32_t  segments1[] = { 200, 100, 200,100,200 };
VibePattern pat1 = {
  .durations = segments1,
  .num_segments = ARRAY_LENGTH(segments1),
};
  
  
                //for rapid rise or fall notify every time it occurs
                if(ARROW==ARROW_DOWN_DOWN){
                    vibes_enqueue_custom_pattern(pat);
                }
            
                if(ARROW==ARROW_UP_UP){
                    vibes_enqueue_custom_pattern(pat);
                }
            
                //
                if (currentGlucose<80 && alertCount==0){
                   vibes_enqueue_custom_pattern(pat1);
                }
                
                if(currentGlucose<80 && alertCount>0){
                    alertCount++;
                    if(alertCount==3){
                        alertCount=0;
                    }
                }
        
                if (currentGlucose<60 && alertCount==0){
                    alertCount++;
                   vibes_enqueue_custom_pattern(pat1);
            
                }
                if(currentGlucose<60 && alertCount>0){
                    alertCount++;
                    if(alertCount>2){
                        alertCount=0;
                    }
                }
            
                if(currentGlucose>180 && alertCount==0)
                {
                    alertCount++;
                    vibes_enqueue_custom_pattern(pat1);
                }
                
                if(currentGlucose>180 && alertCount>0){
                    alertCount++;
                    if(alertCount==24){
                        alertCount=0;
                    }
                }	
        
                if(currentGlucose>80 &&currentGlucose<180){
                    alertCount=0;
                }

}

static void sync_tuple_changed_callback(const uint32_t key, const Tuple* new_tuple, const Tuple* old_tuple, void* context) {
  long this_reading=0;

  APP_LOG(APP_LOG_LEVEL_DEBUG,"IN Sync Callback"); 
  //watchCallbackCount++;
  miss_count=0;
  text_layer_set_text(alert_layer, " ");
  switch (key) {
    //
     //case GLUCOSESTRING_KEY:
     //   APP_LOG(APP_LOG_LEVEL_DEBUG,"GLUCOSE STRING KEY");  
        // App Sync keeps new_tuple in sync_buffer, so we may use it directly
     //   text_layer_set_text(test_layer, new_tuple->value->cstring);
     //   break;
    //
     case GLUCOSE_KEY:
        APP_LOG(APP_LOG_LEVEL_DEBUG,"GLUCOSE KEY");       
        currentGlucose=(new_tuple->value->int16);
        APP_LOG(APP_LOG_LEVEL_DEBUG,"GLUCOSE %i",currentGlucose); 
        snprintf(glucbuf, sizeof(glucbuf), "%i", currentGlucose);
    
        if (ARROW==ARROW_45_UP){
              snprintf(glucbuf, sizeof(glucbuf), "%i  /", currentGlucose);
        }
    
        if (ARROW==ARROW_UP){
              snprintf(glucbuf, sizeof(glucbuf), "%i  ^", currentGlucose);
        }
    
        if (ARROW==ARROW_UP_UP){
              snprintf(glucbuf, sizeof(glucbuf), "%i  ^^", currentGlucose);
        }
    
        if (ARROW==ARROW_45_DOWN){
              snprintf(glucbuf, sizeof(glucbuf), "%i  \\", currentGlucose);
        }
    
        if (ARROW==ARROW_DOWN){
              snprintf(glucbuf, sizeof(glucbuf), "%i  V", currentGlucose);
        }
    
       if (ARROW==ARROW_DOWN_DOWN){
              snprintf(glucbuf, sizeof(glucbuf), "%i  VV", currentGlucose);
       }                
                       
        text_layer_set_text(glucose_layer, glucbuf);
        break;
    //
     case LASTREADING_KEY:
        APP_LOG(APP_LOG_LEVEL_DEBUG,"LASTREADING KEY");   
        this_reading=atol(new_tuple->value->cstring);
        //if watch hasn't received an update in ~6 minutes, alert user
        if (this_reading==last_reading){
          sensor_miss_count++;
          if(sensor_miss_count>7){
            //buzz
            vibes_double_pulse();
            text_layer_set_text(alert_layer, "!!");
          }
        }else{
          APP_LOG(APP_LOG_LEVEL_DEBUG,"Calling Alerts");   
          sensor_miss_count=0;
          alerts();
        }
        last_reading=this_reading;
        break;
    case ARROW_KEY:
    //
      APP_LOG(APP_LOG_LEVEL_DEBUG,"ARROW KEY");
      ARROW=(new_tuple->value->int8);
      break;
    case SLOPEDIRECTION_KEY:
      APP_LOG(APP_LOG_LEVEL_DEBUG,"SLOPEDIRECTION KEY");
      slopeDirection=(new_tuple->value->int8);
      break;
    //
    case TIMETOLIMIT_KEY:
     APP_LOG(APP_LOG_LEVEL_DEBUG,"TIMETOLIMIT KEY");
     timeToLimit=(new_tuple->value->int16);
     APP_LOG(APP_LOG_LEVEL_DEBUG,"Timetolimit %d",timeToLimit);
     if (timeToLimit<99  && timeToLimit>0){
        if (slopeDirection==SLOPE_DOWN){
          snprintf(buf, sizeof(buf), "V %d", timeToLimit);
        }
        if (slopeDirection==SLOPE_UP){
          snprintf(buf, sizeof(buf), "^ %d", timeToLimit);
        }
     }else{
       snprintf(buf, sizeof(buf), "   ");
     }
     text_layer_set_text(timetolimit_layer, buf);
     break;
  }
}

static void update_time() {
  // Get a tm structure
  time_t temp = time(NULL); 
  struct tm *tick_time = localtime(&temp);

  // Create a long-lived buffer
  static char buffer[] = "00:00";
  static char buffer1[] = "00:00";
  // Write the current hours and minutes into the buffer
  if(clock_is_24h_style() == true) {
    //Use 2h hour format
    strftime(buffer, sizeof("00:00"), "%H:%M", tick_time);
  } else {
    //Use 12 hour format
    strftime(buffer,  sizeof("00:00"), "%l:%M", tick_time);
    
    // Display this time on the TextLayer
    text_layer_set_text(s_time_layer, buffer);
    
    strftime(buffer1,  sizeof("00/00"), "%m/%e", tick_time);
    // Display this time on the TextLayer
    text_layer_set_text(date_layer, buffer1);
  }


}
  
static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_time();
  miss_count++;

  if(miss_count>7){
    vibes_double_pulse();
    text_layer_set_text(alert_layer, "!");
    miss_count=0;
  }
}


static void window_load(Window *window) {
  APP_LOG(APP_LOG_LEVEL_DEBUG,"Window Load");
  Layer *window_layer = window_get_root_layer(window);
  
  //clock
  // Create time TextLayer
  s_time_layer = text_layer_create(GRect(5, 20, 144, 50));
  text_layer_set_background_color(s_time_layer, GColorClear);
  text_layer_set_text_color(s_time_layer, GColorWhite);
  text_layer_set_text(s_time_layer, "00:00");
  text_layer_set_font(s_time_layer, fonts_get_system_font(FONT_KEY_BITHAM_42_MEDIUM_NUMBERS));
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentLeft);

  //date layer
  date_layer = text_layer_create(GRect(90, 20, 144, 50));
  text_layer_set_background_color(date_layer, GColorClear);
  text_layer_set_text_color(date_layer, GColorWhite);
  text_layer_set_text(date_layer, "00/00");
  text_layer_set_font(date_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24));
  text_layer_set_text_alignment(date_layer, GTextAlignmentLeft);
 
  //
  //
  //test
  test_layer = text_layer_create(GRect(10, 60, 144, 68));
  text_layer_set_background_color(test_layer, GColorClear);
  text_layer_set_text_color(test_layer, GColorWhite);
  text_layer_set_font(test_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24));
  text_layer_set_text_alignment(test_layer, GTextAlignmentLeft);

  //glucose
  glucose_layer = text_layer_create(GRect(10, 90, 144, 68));
  text_layer_set_background_color(glucose_layer, GColorClear);
  text_layer_set_text_color(glucose_layer, GColorWhite);
  text_layer_set_font(glucose_layer, fonts_get_system_font(FONT_KEY_BITHAM_34_MEDIUM_NUMBERS));
  text_layer_set_text_alignment(glucose_layer, GTextAlignmentLeft);
  text_layer_set_text(glucose_layer, glucose);
  
  //time to limit
  timetolimit_layer = text_layer_create(GRect(10, 130, 144, 68));
  text_layer_set_background_color(timetolimit_layer, GColorClear);
  text_layer_set_text_color(timetolimit_layer, GColorWhite);
  text_layer_set_font(timetolimit_layer, fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK));
  text_layer_set_text_alignment(timetolimit_layer, GTextAlignmentLeft);

  //alerts
  alert_layer = text_layer_create(GRect(120, 130, 144, 68));
  text_layer_set_background_color(alert_layer, GColorClear);
  text_layer_set_text_color(alert_layer, GColorWhite);
  text_layer_set_font(alert_layer, fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK));
  text_layer_set_text_alignment(alert_layer, GTextAlignmentLeft);
  
  Tuplet initial_values[] = {
    TupletCString(GLUCOSESTRING_KEY,"000" ),  
    TupletInteger(GLUCOSE_KEY,0),
    TupletInteger(ARROW_KEY,0),
    TupletInteger(SLOPEDIRECTION_KEY,0),
    TupletInteger(TIMETOLIMIT_KEY,0),
    TupletCString(LASTREADING_KEY,"0")
  };
  
  app_sync_init(&sync, sync_buffer, sizeof(sync_buffer), initial_values, ARRAY_LENGTH(initial_values),
                sync_tuple_changed_callback, sync_error_callback, NULL);
  
  layer_add_child(window_layer, text_layer_get_layer(s_time_layer));
  layer_add_child(window_layer, text_layer_get_layer(date_layer));
  layer_add_child(window_layer, text_layer_get_layer(test_layer));
  layer_add_child(window_layer, text_layer_get_layer(glucose_layer));
  layer_add_child(window_layer, text_layer_get_layer(timetolimit_layer));
  layer_add_child(window_layer, text_layer_get_layer(alert_layer));
   
  // Make sure the time is displayed from the start
  update_time();
  
}

static void window_unload(Window *window) {
  app_sync_deinit(&sync);

  if (icon_bitmap) {
    gbitmap_destroy(icon_bitmap);
  } 

  text_layer_destroy(glucose_layer);
  text_layer_destroy(test_layer);
  text_layer_destroy(alert_layer);
  text_layer_destroy(timetolimit_layer);
  text_layer_destroy(s_time_layer);
  text_layer_destroy(date_layer);
  bitmap_layer_destroy(icon_layer);
}

static void init() {
  window = window_create();
  window_set_background_color(window, GColorBlack);
  window_set_fullscreen(window, true);
  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload
  });

  const int inbound_size = 64;
  const int outbound_size = 16;
  app_message_open(inbound_size, outbound_size);

  // Register with TickTimerService
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  
  const bool animated = true;
  window_stack_push(window, animated);
}

static void deinit() {
  window_destroy(window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
