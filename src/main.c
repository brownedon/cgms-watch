/*
 * main.c
 * Sets up the Window, ClickConfigProvider and ClickHandlers.
 */

#include <pebble.h>
#include <calibration.h>
#include <readings.h>
#include <alerts.h>

static Window *s_main_window;
//number of slope values to accept from iphone
//before overwriting stored slope and intercept
int SLOPE_OVERRIDE=100;

#define _DATE_BUF_LEN 26
#define DATE_FORMAT "%l %B %e %T"
static TextLayer *glucose_layer;
static TextLayer *test_layer;
static TextLayer *timetolimit_layer;
static TextLayer *s_time_layer;
static TextLayer *date_layer;
static TextLayer *alert_layer;
char buf[5];
char glucbuf[15];
char testbuf[30];
static char glucose[16];
int slopecount=0;
int sleepCount=0;
static BitmapLayer *icon_layer;
static GBitmap *icon_bitmap = NULL;
int menuStatus=0;
static AppSync sync;
static uint8_t sync_buffer[256];
uint32_t last_reading = 0;
uint32_t this_reading = 0;
uint8_t miss_count = 0;
uint8_t sensor_miss_count = 0;
int32_t currentGlucose = 0;
int16_t lastGlucose = 0;

long calTime=0;
bool newCal=false;

long isig = 0;

int watchCallbackCount = 0;
int lastWatchCallbackCount = 0;

int slopeDirection = 0;

enum GlucoseKey {
  GLUCOSE_KEY = 0x1,
  LASTREADING_KEY = 0x5,
  slopeKey = 0x6,
  interceptKey = 0x7,
  isigKey = 0x8
};

bool readingAdded = false;

//

void calibrate(int gluc) {
   APP_LOG(APP_LOG_LEVEL_DEBUG,"calibrate");
    if (readings_arr[0].rawcounts > 0) {
        newCal = true;
        calTime = readings_arr[0].seconds;
        
        addCalibration(readings_arr[0].rawcounts, gluc);
        
        calcSlopeandInt();
        
        long rc1 = (readings_arr[0].rawcounts) - intercept;
        int glucose = (rc1 / slope);
        readings_arr[0].glucose=glucose;
    }
}

void reCalibrate(){
    APP_LOG(APP_LOG_LEVEL_DEBUG,"CurrentTime %ld",(readings_arr[0].seconds));
    APP_LOG(APP_LOG_LEVEL_DEBUG,"CalTime %ld",calTime);
    if ( readings_arr[0].seconds  - calTime > 1140) { //19 minutes missed readings, clear it
      newCal = false;
    }
   
   if (readings_arr[0].seconds  - calTime > 660) { //11 minutes
      APP_LOG(APP_LOG_LEVEL_DEBUG,"Recalc");
      newCal = false;
      updateRawcount(readings_arr[0].rawcounts);
      calcSlopeandInt();
  }
}




char *translate_error(AppMessageResult result) {
  APP_LOG(APP_LOG_LEVEL_DEBUG,"translate_error");
  switch (result) {
    case APP_MSG_OK: return "APP_MSG_OK";
    case APP_MSG_SEND_TIMEOUT: return "APP_MSG_SEND_TIMEOUT";
    case APP_MSG_SEND_REJECTED: return "APP_MSG_SEND_REJECTED";
    case APP_MSG_NOT_CONNECTED: return "APP_MSG_NOT_CONNECTED";
    case APP_MSG_APP_NOT_RUNNING: return "APP_MSG_APP_NOT_RUNNING";
    case APP_MSG_INVALID_ARGS: return "APP_MSG_INVALID_ARGS";
    case APP_MSG_BUSY: return "APP_MSG_BUSY";
    case APP_MSG_BUFFER_OVERFLOW: return "APP_MSG_BUFFER_OVERFLOW";
    case APP_MSG_ALREADY_RELEASED: return "APP_MSG_ALREADY_RELEASED";
    case APP_MSG_CALLBACK_ALREADY_REGISTERED: return "APP_MSG_CALLBACK_ALREADY_REGISTERED";
    case APP_MSG_CALLBACK_NOT_REGISTERED: return "APP_MSG_CALLBACK_NOT_REGISTERED";
    case APP_MSG_OUT_OF_MEMORY: return "APP_MSG_OUT_OF_MEMORY";
    case APP_MSG_CLOSED: return "APP_MSG_CLOSED";
    case APP_MSG_INTERNAL_ERROR: return "APP_MSG_INTERNAL_ERROR";
    default: return "UNKNOWN ERROR";
  }
}

static SimpleMenuLayer *s_simple_menu_layer;
static SimpleMenuSection s_menu_sections[1];
static SimpleMenuItem s_first_menu_items[20];

static char *s_options[15]={"Enter","Reset","Zzz","80","90","100","110","120","130","140","150","160","170","180","190"};

char *menuSelection="";

  void resetCal() {
    APP_LOG(APP_LOG_LEVEL_DEBUG,"resetCal");
    initCalibrations();

    addCalibration(30000, 0);
    slope = 703;
    intercept = 30003;
    
    persist_write_int(SLOPEKEY, slope);
    persist_write_int(INTERCEPTKEY, intercept);
}

void out_sent_handler(DictionaryIterator *sent, void *context) {
  // outgoing message was delivered
  APP_LOG(APP_LOG_LEVEL_DEBUG, "****DICTIONARY SENT SUCCESSFULLY!****");
}


void out_failed_handler(DictionaryIterator *failed, AppMessageResult reason, void *context) {
  // outgoing message failed
  APP_LOG(APP_LOG_LEVEL_DEBUG, "DICTIONARY NOT SENT! ERROR!");
  APP_LOG(APP_LOG_LEVEL_DEBUG, "In dropped: %i - %s", reason, translate_error(reason));
}


static void sync_error_callback(DictionaryResult dict_error, AppMessageResult app_message_error, void *context) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "App Message Sync Error: %d", app_message_error);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "In dropped: %i - %s", app_message_error, translate_error(app_message_error));
}



static void update_time() {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "update_time");
  // Get a tm structure
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);

  // Create a long-lived buffer
  static char buffer[] = "00:00";
  static char buffer1[] = "Mon     00:00";
  // Write the current hours and minutes into the buffer
  //Use 12 hour format
  strftime(buffer,  sizeof("00:00"), "%l:%M", tick_time);

  // Display this time on the TextLayer
  text_layer_set_text(s_time_layer, buffer);

  strftime(buffer1,  sizeof("Mon  00/00"), "%a  %m/%e", tick_time);
  // Display this time on the TextLayer
  text_layer_set_text(date_layer, buffer1);


}

static void sync_tuple_changed_callback(const uint32_t key, const Tuple* new_tuple, const Tuple* old_tuple, void* context) {
  int timeToLimit=100;
  APP_LOG(APP_LOG_LEVEL_DEBUG, "sync_tuple_callback");
  miss_count = 0;
  switch (key) {
    case LASTREADING_KEY:
      APP_LOG(APP_LOG_LEVEL_DEBUG, "LASTREADING KEY");
      this_reading = (new_tuple->value->int32);
      APP_LOG(APP_LOG_LEVEL_DEBUG, "%lu %lu", this_reading, last_reading);
      APP_LOG(APP_LOG_LEVEL_DEBUG, "Calling Alerts");
      sensor_miss_count = 0;
      readingAdded = true;
      text_layer_set_text(alert_layer, " ");
      last_reading = this_reading;
      break;
    //if we get a slope and intercept
    //from the iphone use it
    //otherwise we are stand alone
    case slopeKey:
      slopecount++;
      if(slopecount>SLOPE_OVERRIDE){
        APP_LOG(APP_LOG_LEVEL_DEBUG, "Slope KEY");
        slope = (new_tuple->value->int32);
        APP_LOG(APP_LOG_LEVEL_DEBUG, "Slope %lu", slope);
        persist_write_int(SLOPEKEY, slope);
      }
      break;
    case interceptKey:
      if(slopecount>SLOPE_OVERRIDE){
        APP_LOG(APP_LOG_LEVEL_DEBUG, "Intercept KEY");
        intercept = (new_tuple->value->uint32);
        APP_LOG(APP_LOG_LEVEL_DEBUG, "Intercept %lu", intercept);
        persist_write_int(INTERCEPTKEY, intercept);
      }
      break;
    case isigKey:
      APP_LOG(APP_LOG_LEVEL_DEBUG, "ISIG KEY");
      isig = (new_tuple->value->uint32);
      APP_LOG(APP_LOG_LEVEL_DEBUG, "ISIG %lu", isig);
      APP_LOG(APP_LOG_LEVEL_DEBUG, "GLUCOSE %lu", currentGlucose);
      APP_LOG(APP_LOG_LEVEL_DEBUG, "Intercept %lu", intercept);
      APP_LOG(APP_LOG_LEVEL_DEBUG, "Slope %lu", slope);

      if(newCal==true){
        reCalibrate();
      }

      if (isig != 0  && slope != 0 && intercept != 0 ) {
        currentGlucose = ((isig - intercept) / slope);
        snprintf(glucbuf, sizeof(glucbuf), "%lu", currentGlucose);
        snprintf(testbuf, sizeof(testbuf), "%lu %lu", slope, intercept);
        text_layer_set_text(test_layer, testbuf);
        addReading(currentGlucose, isig);
        double tmpSlope = getSlopeGlucose();

        if (tmpSlope < 0) {
          //45 down
          if (abs(tmpSlope) >= 1)
            snprintf(glucbuf, sizeof(glucbuf), "%lu  \\", currentGlucose);
          //straight down
          if (abs(tmpSlope) >= 2)
            snprintf(glucbuf, sizeof(glucbuf), "%lu  V", currentGlucose);
          if (abs(tmpSlope) >= 3) {
            snprintf(glucbuf, sizeof(glucbuf), "%lu  VV", currentGlucose);
            vibes_enqueue_custom_pattern(pat);
          }
        }

        if (tmpSlope > 0) {
          if (tmpSlope >= 1)
            snprintf(glucbuf, sizeof(glucbuf), "%lu  /", currentGlucose);
          if (tmpSlope >= 2)
            snprintf(glucbuf, sizeof(glucbuf), "%lu  ^", currentGlucose);
          if (tmpSlope >= 3) {
            snprintf(glucbuf, sizeof(glucbuf), "%lu  ^^", currentGlucose);
            vibes_enqueue_custom_pattern(pat);
          }
        }

        if (tmpSlope > 0 && currentGlucose < 180) {
          //how long until 180
          timeToLimit = abs((180 - currentGlucose) / tmpSlope);
          //since the dex is ~15 minutes behind reality
          timeToLimit = timeToLimit - 15;
        }
        if (tmpSlope < 0 && currentGlucose > 80) {
          timeToLimit = abs((currentGlucose - 80) / tmpSlope);
          //since the dex is ~15 minutes behind reality
          timeToLimit = timeToLimit - 15;
        }
        if (timeToLimit <= 0) {
          timeToLimit = 1;
        }

        if (timeToLimit < 99) {
          if (tmpSlope < 0) {
            snprintf(buf, sizeof(buf), "V %d", timeToLimit);
          }
          if (tmpSlope > 0) {
            snprintf(buf, sizeof(buf), "^ %d", timeToLimit);
          }
        } else {
          snprintf(buf, sizeof(buf), "   ");
        }
        text_layer_set_text(timetolimit_layer, buf);

        if (abs(lastGlucose - currentGlucose ) > 25 && lastGlucose != 0 && currentGlucose != 0) {
          text_layer_set_text(alert_layer, "???");
        }

        readingAdded = false;
        if (currentGlucose > 20) {
          if (sleepCount==0){
            alerts(currentGlucose,timeToLimit);
          }
        } else {
          text_layer_set_text(alert_layer, "?");
        }
        text_layer_set_text(glucose_layer, glucbuf);
        lastGlucose = currentGlucose;

        APP_LOG(APP_LOG_LEVEL_DEBUG, "Calc GLUCOSE %lu", currentGlucose);
      }
      break;
  }
}



static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  //this also seems to get called when ios connects
  //not just every minute...
  miss_count++;
  if (sleepCount>0){
    sleepCount--;
  }
  APP_LOG(APP_LOG_LEVEL_DEBUG, "In tick handler %i, %i", miss_count, sensor_miss_count);
  if (miss_count > 6) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "One Miss recorded");
    text_layer_set_text(alert_layer, "!");
  }
  if(miss_count > 11) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Miss recorded");
    vibes_enqueue_custom_pattern(pat);
    text_layer_set_text(alert_layer, "!!");
    miss_count = 0;
  }
  update_time();
}


static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "up_click_handler");
}

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "select_click_handler");
   s_simple_menu_layer=simple_menu_layer_create(GRect(100,1,50, 50),s_main_window,s_menu_sections,1,NULL);
   layer_add_child( text_layer_get_layer(s_time_layer),simple_menu_layer_get_layer(s_simple_menu_layer));
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "down_click_handler");
}

static void click_config_provider(void *context) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "click_config_provider");
  // Register the ClickHandlers
  window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
}


static void menu_select_callback(int index, void *ctx){
  APP_LOG(APP_LOG_LEVEL_DEBUG, "menu select callback");
  if( s_first_menu_items[index].subtitle==NULL){
    s_first_menu_items[index].subtitle="Selected";
  }else{
    s_first_menu_items[index].subtitle=NULL;
  }
  
  if (*s_first_menu_items[index].title=='E'){
    APP_LOG(APP_LOG_LEVEL_DEBUG, "enter was selected");
    for (int i=1;i<15;i++){
      if(s_first_menu_items[i].subtitle!=NULL && *s_first_menu_items[i].subtitle=='S'){
        APP_LOG(APP_LOG_LEVEL_DEBUG, s_first_menu_items[i].title);
        //
        slopecount=0;
        if(*s_first_menu_items[i].title=='R'){
          resetCal();
          break;
        }else if (*s_first_menu_items[i].title=='Z'){
          sleepCount=15; 
          break;
        }else{
          //its a calibration
          calibrate(atoi(s_first_menu_items[i].title));
          break;
        }
      }
    }
     //reset menu
      for (int i=0;i<15;i++){
          s_first_menu_items[i].subtitle=NULL;
      }
      //hide the layer now    
      layer_remove_child_layers(text_layer_get_layer(s_time_layer));     
      window_set_click_config_provider(s_main_window, click_config_provider);
  }
  layer_mark_dirty(simple_menu_layer_get_layer(s_simple_menu_layer));
}

void fill_menu(){
  int i=0;
  for(i=0;i<(int)(sizeof(s_options)/sizeof(s_options[0]));i++){
    s_first_menu_items[i]=(SimpleMenuItem){
      .title = s_options[i],
      .callback = menu_select_callback
    };
  }
}


static void window_load(Window *window) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Window Load");
  Layer *window_layer = window_get_root_layer(window);
  fill_menu();
  s_menu_sections[0]=(SimpleMenuSection){
    .title = "Calibrate",
    .num_items=sizeof(s_options)/sizeof(s_options[0]),
    .items = s_first_menu_items
  };

  retrieveReadings();  
  retrieveCal();

  //clock
  // Create time TextLayer
  s_time_layer = text_layer_create(GRect(1, 25, 144, 50));
  //text_layer_set_background_color(s_time_layer, GColorBlack);
  //text_layer_set_text_color(s_time_layer, GColorWhite);
  text_layer_set_text(s_time_layer, "00:00");
  text_layer_set_font(s_time_layer, fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD));
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentLeft);


  //date layer
  date_layer = text_layer_create(GRect(55, 1, 144, 50));
  text_layer_set_background_color(date_layer, GColorClear);
  text_layer_set_text_color(date_layer, GColorWhite);
  text_layer_set_text(date_layer, "00/00");
  text_layer_set_font(date_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24));
  text_layer_set_text_alignment(date_layer, GTextAlignmentLeft);

  //
  //
  //test
  test_layer = text_layer_create(GRect(1, 66, 144, 68));
  text_layer_set_background_color(test_layer, GColorClear);
  text_layer_set_text_color(test_layer, GColorWhite);
  text_layer_set_font(test_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24));
  text_layer_set_text_alignment(test_layer, GTextAlignmentLeft);

  //glucose
  glucose_layer = text_layer_create(GRect(10, 90, 144, 68));
  text_layer_set_background_color(glucose_layer, GColorClear);
  text_layer_set_text_color(glucose_layer, GColorWhite);
  text_layer_set_font(glucose_layer, fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK));
  text_layer_set_text_alignment(glucose_layer, GTextAlignmentLeft);
  text_layer_set_text(glucose_layer, glucose);

  //time to limit
  timetolimit_layer = text_layer_create(GRect(10, 130, 144, 68));
  text_layer_set_background_color(timetolimit_layer, GColorClear);
  text_layer_set_text_color(timetolimit_layer, GColorWhite);
  text_layer_set_font(timetolimit_layer, fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK));
  text_layer_set_text_alignment(timetolimit_layer, GTextAlignmentLeft);

  //alerts
  alert_layer = text_layer_create(GRect(110, 130, 144, 68));
  text_layer_set_background_color(alert_layer, GColorClear);
  text_layer_set_text_color(alert_layer, GColorWhite);
  text_layer_set_font(alert_layer, fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK));
  text_layer_set_text_alignment(alert_layer, GTextAlignmentLeft);

  if (persist_exists(SLOPEKEY)) {
    // Load stored count
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Using stored slope");
    slope = persist_read_int(SLOPEKEY);
  } else {
    slope = 702;
  }
  if (persist_exists(INTERCEPTKEY)) {
    // Load stored count
    intercept = persist_read_int(INTERCEPTKEY);
  } else {
    intercept = 30002;
  }

  Tuplet initial_values[] = {
    TupletInteger(GLUCOSE_KEY, 0),
    TupletInteger(LASTREADING_KEY, 0),
    TupletInteger(slopeKey, slope),
    TupletInteger(interceptKey, intercept),
    TupletInteger(isigKey, 0)
  };

  app_sync_init(&sync, sync_buffer, sizeof(sync_buffer), initial_values, ARRAY_LENGTH(initial_values),
                sync_tuple_changed_callback, sync_error_callback, NULL);

  layer_add_child(window_layer, text_layer_get_layer(s_time_layer));
  layer_add_child(window_layer, text_layer_get_layer(date_layer));
  layer_add_child(window_layer, text_layer_get_layer(test_layer));
  layer_add_child(window_layer, text_layer_get_layer(glucose_layer));
  layer_add_child(window_layer, text_layer_get_layer(timetolimit_layer));
  layer_add_child(window_layer, text_layer_get_layer(alert_layer));

  miss_count = 0;
  sensor_miss_count = 0;
  // Make sure the time is displayed from the start
  update_time();
}

static void window_unload(Window *window) {
 app_sync_deinit(&sync);
  simple_menu_layer_destroy(s_simple_menu_layer);
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
  APP_LOG(APP_LOG_LEVEL_DEBUG, "init");
  s_main_window = window_create();
  window_set_click_config_provider(s_main_window, click_config_provider);
  window_set_background_color(s_main_window, GColorBlack);
  #ifdef PBL_SDK_2
    window_set_fullscreen(s_main_window, true);
  #endif
  window_set_window_handlers(s_main_window, (WindowHandlers) {
     .load = window_load,
     .unload = window_unload
  });

  const int inbound_size = 128;
  const int outbound_size = 128;
  app_message_open(inbound_size, outbound_size);


  // Register with TickTimerService
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);

  const bool animated = true;
  window_stack_push(s_main_window, animated);
}

static void deinit() {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "deinit");
  persistReadings();
  // Destroy main Window
  window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
