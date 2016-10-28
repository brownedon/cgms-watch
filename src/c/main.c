/*
 * main.c
 * Sets up the Window, ClickConfigProvider and ClickHandlers.
 */

#include <pebble.h>
#include "calibration.h"
#include "readings.h"
#include "alerts.h"
#include "simple_analog.h"

//for testing in emulator
//#define TESTING


//Using smartstrap for limited debugging
//#define SMARTSTRAP 
#ifdef SMARTSTRAP
  #include "strap.h"
#endif

static Window *CalWindow;
static Layer *CalChromeLayer;
static TextLayer *CalTextLayer;

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
//static TextLayer *debug_layer;
char buf[6];
char glucbuf[15];
char testbuf[40];
char debugbuf[30];

//needs global scope or it won't show in menu
char intbuf[6];
char battbuf[6];

static char glucose[16];
int slopecount=0;
int sleepCount=0;
static BitmapLayer *icon_layer;
static GBitmap *icon_bitmap = NULL;
int menuStatus=0;
int showint=0;
static AppSync sync;
static uint8_t sync_buffer[256];
uint32_t last_reading = 0;
uint32_t this_reading = 0;
uint8_t miss_count = 0;
uint8_t sensor_miss_count = 0;
int currentGlucose = 0;
int lastGlucose = 0;

long calTime=0;
bool newCal=false;

long isig = 0;
int secondCount=0;
int watchCallbackCount = 0;
int lastWatchCallbackCount = 0;

int slopeDirection = 0;

enum GlucoseKey {
  GLUCOSE_KEY = 0x1,
  LASTREADING_KEY = 0x5,
  slopeKey = 0x6,
  isigKey = 0x8,
  battKey =0x7
};

bool readingAdded = false;

//analog watch interface
static Layer *s_simple_bg_layer, *s_date_layer, *s_hands_layer;
static TextLayer *s_day_label, *s_num_label;

static GPath *s_tick_paths[NUM_CLOCK_TICKS];
static GPath *s_minute_arrow, *s_hour_arrow;
static char s_num_buffer[4], s_day_buffer[6];

static SimpleMenuLayer *s_simple_menu_layer;
static SimpleMenuSection s_menu_sections[1];
static SimpleMenuItem s_first_menu_items[20];

static char *s_options[18]={"Enter","Reset","Zzz","80","90","100","110","120","130","140","150","160","170","180","190","200","30000","999"};

char *menuSelection="";
//
//function declarations
void addInterceptToMenu();

static void click_config_provider(void *context) ;

static void getSlopeAndInt(){
   APP_LOG(APP_LOG_LEVEL_DEBUG,"getSlopeAndInt");
  int calCount=0;
   retrieveCal();
  
  for (int i = 4; i > 0; i-- ) {
    if (calibrations_arr[i].rawcounts>0){
      calCount++;
    }
  }

   APP_LOG(APP_LOG_LEVEL_DEBUG,"calCount is %i",calCount);
  if (calCount>0){
     APP_LOG(APP_LOG_LEVEL_DEBUG,"Calc slope and int");
    calcSlopeandInt();
  }else{
    //APP_LOG(APP_LOG_LEVEL_DEBUG,"Use defaults");
    slope=700;
    intercept=30000;
  }
  
}


static void bg_update_proc(Layer *layer, GContext *ctx) {
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);
  graphics_context_set_fill_color(ctx, GColorWhite);
  for (int i = 0; i < NUM_CLOCK_TICKS; ++i) {
    const int x_offset = PBL_IF_ROUND_ELSE(18, 0);
    const int y_offset = PBL_IF_ROUND_ELSE(6, 0);
    gpath_move_to(s_tick_paths[i], GPoint(x_offset, y_offset));
    gpath_draw_filled(ctx, s_tick_paths[i]);
  }
}

static void hands_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);

  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  
  // minute/hour hand
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_context_set_stroke_color(ctx, GColorBlack);

  gpath_rotate_to(s_minute_arrow, TRIG_MAX_ANGLE * t->tm_min / 60);
  gpath_draw_filled(ctx, s_minute_arrow);
  gpath_draw_outline(ctx, s_minute_arrow);

  gpath_rotate_to(s_hour_arrow, (TRIG_MAX_ANGLE * (((t->tm_hour % 12) * 6) + (t->tm_min / 10))) / (12 * 6));
  gpath_draw_filled(ctx, s_hour_arrow);
  gpath_draw_outline(ctx, s_hour_arrow);

  // dot in the middle
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, GRect(bounds.size.w / 2 - 1, bounds.size.h / 2 - 1, 3, 3), 0, GCornerNone);
}

static void date_update_proc(Layer *layer, GContext *ctx) {
  time_t now = time(NULL);
  struct tm *t = localtime(&now);

  strftime(s_day_buffer, sizeof(s_day_buffer), "%a", t);
  text_layer_set_text(s_day_label, s_day_buffer);

  strftime(s_num_buffer, sizeof(s_num_buffer), "%d", t);
  text_layer_set_text(s_num_label, s_num_buffer);
}

  void resetCal() {
    APP_LOG(APP_LOG_LEVEL_DEBUG,"resetCal");
    initCalibrations();

    addCalibration(30000, 0);
    slope = 703;
    intercept = 30000;
    addInterceptToMenu();
    
    persistC(SLOPEKEY,slope);
    persistC(INTERCEPTKEY,intercept);
  }

void calibrate(int gluc) {
   APP_LOG(APP_LOG_LEVEL_DEBUG,"calibrate");
    if (readings_arr[0].rawcounts > 0) {
        newCal = true;
        calTime = readings_arr[0].minutes;
        
        addCalibration(readings_arr[0].rawcounts, gluc);
        
        calcSlopeandInt();
        
        long rc1 = (readings_arr[0].rawcounts) - intercept;
        int glucose = (rc1 / slope);
        readings_arr[0].glucose=glucose;
     
        addInterceptToMenu();
    }
}


static void menu_select_callback(int index, void *ctx){
  APP_LOG(APP_LOG_LEVEL_DEBUG, "menu select callback");
  addInterceptToMenu();
  //some debug for when paired with phone
  //get a dump of the cal array through cloud pebble
  for (int i = 0; i < 5; i++ ) {
    if (calibrations_arr[i].rawcounts > 0) {
      APP_LOG(APP_LOG_LEVEL_DEBUG,"record %i:glucose %i:rawcounts %ld",i,calibrations_arr[i].glucose, calibrations_arr[i].rawcounts);
    }
  }
  
  if( s_first_menu_items[index].subtitle==NULL){
    s_first_menu_items[index].subtitle="Selected";
  }else{
    s_first_menu_items[index].subtitle=NULL;
  }
  
  if (*s_first_menu_items[index].title=='E'){
    APP_LOG(APP_LOG_LEVEL_DEBUG, "enter was selected");
    for (int i=1;i<16;i++){
      if(s_first_menu_items[i].subtitle!=NULL && *s_first_menu_items[i].subtitle=='S'){
        //
        slopecount=0;
        if(*s_first_menu_items[i].title=='R'){
          resetCal();
          newCal = false;
          break;
        }else if (*s_first_menu_items[i].title=='Z'){
          sleepCount=15; 
          break;
        }else{
          //its a calibration
            if(i>=3 && i<=15){
              calibrate(atoi(s_first_menu_items[i].title));
            }
          break;
        }
      }
    }
     //reset menu
      for (int i=0;i<16;i++){
          s_first_menu_items[i].subtitle=NULL;
      }

    window_stack_pop(false);
  }
  layer_mark_dirty(simple_menu_layer_get_layer(s_simple_menu_layer));
}

void addInterceptToMenu(){
  APP_LOG(APP_LOG_LEVEL_DEBUG,"add addInterceptToMenu");

  snprintf(intbuf, sizeof(intbuf), "%d", intercept);
  s_first_menu_items[16].title=intbuf;
}


void reCalibrate(){
  APP_LOG(APP_LOG_LEVEL_DEBUG,"reCalibrate");
    APP_LOG(APP_LOG_LEVEL_DEBUG,"CurrentTime %ld",(readings_arr[0].minutes));
    APP_LOG(APP_LOG_LEVEL_DEBUG,"CalTime %ld",calTime);
    if ( readings_arr[0].minutes  - calTime > 20 && newCal) { //16 minutes missed readings, clear it
      newCal = false;
    }
   
   if (readings_arr[0].minutes  - calTime > 11 && newCal) { //11 minutes
      APP_LOG(APP_LOG_LEVEL_DEBUG,"Recalc");
      newCal = false;
      updateRawcount(readings_arr[0].rawcounts);
      calcSlopeandInt();
      addInterceptToMenu();
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


static void cgms_display(uint32_t isig){
   int timeToLimit=100;
   int tmpSlope=slope;
   APP_LOG(APP_LOG_LEVEL_DEBUG, "cgms_display");
   getSlopeAndInt();
  
   //slope changed alert user
   if(slope!=tmpSlope){ 
     vibes_enqueue_custom_pattern(pat);
   }
  
      APP_LOG(APP_LOG_LEVEL_DEBUG, "ISIG KEY");
    //  isig = (new_tuple->value->uint32);
      APP_LOG(APP_LOG_LEVEL_DEBUG, "ISIG %d", (int)isig);
      APP_LOG(APP_LOG_LEVEL_DEBUG, "GLUCOSE %d", currentGlucose);
      APP_LOG(APP_LOG_LEVEL_DEBUG, "Intercept %d", intercept);
      APP_LOG(APP_LOG_LEVEL_DEBUG, "Slope %d", slope);

      if(newCal==true){
        reCalibrate();
      }

      if (isig != 0  && slope != 0 && intercept != 0 ) {
        currentGlucose = ((isig - intercept) / slope);
        addReading(currentGlucose, isig);
        double readingSlope = getSlopeGlucose();
        //
        if(readingSlope==0){
          snprintf(glucbuf, sizeof(glucbuf), "%d  -", currentGlucose);
        }else{
          snprintf(glucbuf, sizeof(glucbuf), "%d  ", currentGlucose);
        }
        if (readingSlope < 0) {
          //45 down
          if (abs(readingSlope) >= 1)
            snprintf(glucbuf, sizeof(glucbuf), "%d  \\", currentGlucose);
          //straight down
          if (abs(readingSlope) >= 2)
            snprintf(glucbuf, sizeof(glucbuf), "%d V", currentGlucose);
          if (abs(readingSlope) >= 3) {
            snprintf(glucbuf, sizeof(glucbuf), "%d  VV", currentGlucose);
            vibes_enqueue_custom_pattern(pat);
          }
        }
         
        if (readingSlope > 0) {
          if (readingSlope >= 1)
            snprintf(glucbuf, sizeof(glucbuf), "%d  /", currentGlucose);
          if (readingSlope >= 2)
            snprintf(glucbuf, sizeof(glucbuf), "%d  ^", currentGlucose);
          if (readingSlope >= 3) {
            snprintf(glucbuf, sizeof(glucbuf), "%d  ^^", currentGlucose);
            vibes_enqueue_custom_pattern(pat);
          }
        }
        timeToLimit=100;
        if (readingSlope > 0 && currentGlucose < 180) {
          //how long until 180
          timeToLimit = abs((180 - currentGlucose) / readingSlope);
          //since the dex is ~15 minutes behind reality
          timeToLimit = timeToLimit - 15;
        }
        if (readingSlope < 0 && currentGlucose > 80) {
          timeToLimit = abs((currentGlucose - 80) / readingSlope);
          //since the dex is ~15 minutes behind reality
          timeToLimit = timeToLimit - 15;
        }
        if (timeToLimit <= 0) {
          timeToLimit = 1;
        }

        #ifdef TESTING
        timeToLimit=10;
        readingSlope=2;
        #endif
        
        if (timeToLimit < 99) {
          if (readingSlope < 0) {
            snprintf(buf, sizeof(buf), "V %d", timeToLimit);
          }
          if (readingSlope > 0) {
            snprintf(buf, sizeof(buf), "^ %d", timeToLimit);
          }
        } else {
          snprintf(buf, sizeof(buf), "   ");
        }

        if (abs(lastGlucose - currentGlucose ) > 25 && lastGlucose != 0 && currentGlucose != 0) {
          text_layer_set_text(alert_layer, "???");
        }

        readingAdded = false;
        if (currentGlucose > 20) {
          if (sleepCount==0){
            alerts(currentGlucose,timeToLimit);
          }
        } else {
            text_layer_set_text(alert_layer, "  ?");
        }
        lastGlucose = currentGlucose;
        char sign='+';
        if (readingSlope<0){
          sign='-';
        }
      
        if((abs((int)readingSlope)==0) && (abs((int)(readingSlope*10)%10)==0)){
             snprintf(testbuf, sizeof(testbuf), " ---\n%d", (int)slope);
        }else{         
             snprintf(testbuf, sizeof(testbuf), "%c%d.%d\n%d", sign,abs((int)readingSlope),abs((int)(readingSlope*10)%10),(int)slope);
        }
        
        snprintf(glucbuf, sizeof(glucbuf), "%d",(int)currentGlucose);
        if(timeToLimit<99){
          snprintf(buf, sizeof(buf), "%c%d", sign,timeToLimit);
        } else {
          snprintf(buf, sizeof(buf), "    ");
        }
        text_layer_set_text(timetolimit_layer, buf);
        
        text_layer_set_text(glucose_layer, glucbuf);
        text_layer_set_text(test_layer, testbuf);
        APP_LOG(APP_LOG_LEVEL_DEBUG, "Calc GLUCOSE %d", (int)currentGlucose);
      }
}

static void sync_tuple_changed_callback(const uint32_t key, const Tuple* new_tuple, const Tuple* old_tuple, void* context) {
 
  APP_LOG(APP_LOG_LEVEL_DEBUG, "sync_tuple_callback");
  
  switch (key) {
    case isigKey:
      text_layer_set_text(alert_layer, "   ");
      miss_count=0;
      cgms_display(new_tuple->value->uint32);
      break;
    case battKey:
        APP_LOG(APP_LOG_LEVEL_DEBUG,"add battery to menu");
        snprintf(battbuf, sizeof(battbuf), "B=%d",(int)new_tuple->value->int32);
        s_first_menu_items[17].title=battbuf;
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
  #ifdef SMARTSTRAP
    strap_request_data("In tick handler") ;
  #endif
  if (miss_count > 6 && miss_count<=11) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "One Miss recorded");
    text_layer_set_text(alert_layer, "  !");
  }
  if(miss_count > 11) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Miss recorded");
    vibes_enqueue_custom_pattern(pat);
    text_layer_set_text(alert_layer, " !!");
  }

  layer_mark_dirty(window_get_root_layer(s_main_window));

  #ifdef TESTING
   cgms_display(80000); // 71mg/dl
   //cgms_display(70000);  //56 mg/dl
   miss_count=0;
  #endif

}

static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "up_click_handler");
}

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "select_click_handler");
  window_stack_push(CalWindow, false /* Not animated */);
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "down_click_handler");
}

static void click_config_provider(void *context) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "click_config_provider");
  // Register the ClickHandlers
  window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
  window_long_click_subscribe(BUTTON_ID_SELECT, 0,(ClickHandler)select_click_handler,NULL);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
}


//static void displayCal(void) {
//  window_stack_push(CalWindow, false /* Not animated */);
//}

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
  
  GRect bounds = layer_get_bounds(window_layer);

  s_simple_bg_layer = layer_create(bounds);
  layer_set_update_proc(s_simple_bg_layer, bg_update_proc);
  layer_add_child(window_layer, s_simple_bg_layer);

  s_date_layer = layer_create(bounds);
  layer_set_update_proc(s_date_layer, date_update_proc);
  layer_add_child(window_layer, s_date_layer);
 
  s_day_label = text_layer_create(PBL_IF_ROUND_ELSE(
    GRect(63, 114, 27, 20),
    GRect(50, 100, 27, 20)));
  text_layer_set_text(s_day_label, s_day_buffer);
  text_layer_set_background_color(s_day_label, GColorBlack);
  text_layer_set_text_color(s_day_label, GColorWhite);
  text_layer_set_font(s_day_label, fonts_get_system_font(FONT_KEY_GOTHIC_18));
 
  layer_add_child(s_date_layer, text_layer_get_layer(s_day_label));

  s_num_label = text_layer_create(PBL_IF_ROUND_ELSE(
    GRect(90, 114, 18, 20),
    GRect(73, 100, 18, 20)));
   
  text_layer_set_text(s_num_label, s_num_buffer);
  text_layer_set_background_color(s_num_label, GColorBlack);
  text_layer_set_text_color(s_num_label, GColorWhite);
  text_layer_set_font(s_num_label, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  layer_add_child(s_date_layer, text_layer_get_layer(s_num_label));

  s_hands_layer = layer_create(bounds);
  layer_set_update_proc(s_hands_layer, hands_update_proc);
  layer_add_child(window_layer, s_hands_layer);
  
  test_layer = text_layer_create(PBL_IF_ROUND_ELSE(
    GRect(74, 15, 144, 68),
    GRect(57, 15, 144, 68)));
  
  text_layer_set_background_color(test_layer, GColorClear);
  text_layer_set_text_color(test_layer, GColorWhite);
  text_layer_set_font(test_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24));
  text_layer_set_text_alignment(test_layer, GTextAlignmentLeft);
  
  //glucose                                x  y
  //glucose_layer = text_layer_create(GRect(5, 70, 144, 68));
  glucose_layer = text_layer_create(PBL_IF_ROUND_ELSE(
    GRect(5, 70, 144, 68),
    GRect(1, 70, 144, 68)));
  
  text_layer_set_background_color(glucose_layer, GColorClear);
  text_layer_set_text_color(glucose_layer, GColorWhite);
  text_layer_set_font(glucose_layer, fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK));
  text_layer_set_text_alignment(glucose_layer, GTextAlignmentLeft);
  text_layer_set_text(glucose_layer, glucose);
  
  //time to limit
   timetolimit_layer = text_layer_create(PBL_IF_ROUND_ELSE(
    GRect(143, 70, 144, 68),
    GRect(110, 70, 144, 68)));
  
  text_layer_set_background_color(timetolimit_layer, GColorClear);
  text_layer_set_text_color(timetolimit_layer, GColorWhite);
  text_layer_set_font(timetolimit_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24));
  text_layer_set_text_alignment(timetolimit_layer, GTextAlignmentLeft);

  //alerts
  alert_layer = text_layer_create(PBL_IF_ROUND_ELSE(
    GRect(74, 130, 144, 68),
    GRect(50, 120, 144, 68)));
  
  text_layer_set_background_color(alert_layer, GColorClear);
  text_layer_set_text_color(alert_layer, GColorWhite);
  text_layer_set_font(alert_layer, fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK));
  text_layer_set_text_alignment(alert_layer, GTextAlignmentLeft);
  getSlopeAndInt();

  Tuplet initial_values[] = {
    TupletInteger(GLUCOSE_KEY, 0),
    TupletInteger(LASTREADING_KEY, 0),
    TupletInteger(slopeKey, slope),
    TupletInteger(isigKey, 0),
    TupletInteger(battKey, 0)
  };

  app_sync_init(&sync, sync_buffer, sizeof(sync_buffer), initial_values, ARRAY_LENGTH(initial_values),
                sync_tuple_changed_callback, sync_error_callback, NULL);
  layer_add_child(window_layer, text_layer_get_layer(test_layer));
  layer_add_child(window_layer, text_layer_get_layer(glucose_layer));
  layer_add_child(window_layer, text_layer_get_layer(timetolimit_layer));
  layer_add_child(window_layer, text_layer_get_layer(alert_layer));

  miss_count = 0;
  sensor_miss_count = 0;
  
  //calibration window  
   CalWindow = window_create();
  Layer *CalWindow_layer = window_get_root_layer(CalWindow);

  GRect CalWindow_frame = layer_get_frame(CalWindow_layer);
  CalWindow_frame.origin.x = 5;
  CalWindow_frame.origin.y = 5;
  layer_set_frame(CalWindow_layer, CalWindow_frame);

  GRect CalWindow_bounds = layer_get_bounds(window_layer);
  CalWindow_bounds.size.w -= 10;
  CalWindow_bounds.size.h -= 10;
  layer_set_bounds(CalWindow_layer, CalWindow_bounds);

  window_set_background_color(CalWindow, GColorClear);
  CalChromeLayer = layer_create(CalWindow_frame);
  layer_add_child(CalWindow_layer, CalChromeLayer);

  CalTextLayer = text_layer_create(GRect(20, 20, 110,65));
  text_layer_set_text_color(CalTextLayer, GColorWhite);
  text_layer_set_background_color(CalTextLayer, GColorBlack);

  s_simple_menu_layer=simple_menu_layer_create(GRect(20,20,100,50),CalWindow,s_menu_sections,1,NULL);
  layer_add_child( text_layer_get_layer(CalTextLayer),simple_menu_layer_get_layer(s_simple_menu_layer));
  layer_add_child(CalWindow_layer, text_layer_get_layer(CalTextLayer));
  window_set_click_config_provider(s_main_window, click_config_provider);
  
  #ifdef TESTING
  //display testing, displays bg 170
  cgms_display(150000);
  #endif
}

static void window_unload(Window *window) {
   APP_LOG(APP_LOG_LEVEL_DEBUG, "window_unload");
 app_sync_deinit(&sync);
  simple_menu_layer_destroy(s_simple_menu_layer);
  if (icon_bitmap) {
    gbitmap_destroy(icon_bitmap);
  }

  text_layer_destroy(glucose_layer);
  text_layer_destroy(test_layer);
  text_layer_destroy(alert_layer);
  text_layer_destroy(timetolimit_layer);

  bitmap_layer_destroy(icon_layer);
  
  text_layer_destroy(CalTextLayer);
  layer_destroy(CalChromeLayer);
   
  text_layer_destroy(s_time_layer);
  text_layer_destroy(date_layer);
  
  layer_destroy(s_simple_bg_layer);
  layer_destroy(s_date_layer);

  text_layer_destroy(s_day_label);
  text_layer_destroy(s_num_label);

  layer_destroy(s_hands_layer);
}

static void init() {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "init-new");
  retrieveReadings();
  retrieveCal(); 
  //
  #ifdef SMARTSTRAP
    strap_init();
  #endif
  //app_timer_register(1000, timer_handler, NULL);
  //
  
  s_main_window = window_create();
    
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
  //
 
   s_day_buffer[0] = '\0';
  s_num_buffer[0] = '\0';

  // init hand paths
  s_minute_arrow = gpath_create(&MINUTE_HAND_POINTS);
  s_hour_arrow = gpath_create(&HOUR_HAND_POINTS);

  Layer *window_layer = window_get_root_layer(s_main_window);
  GRect bounds = layer_get_bounds(window_layer);
  GPoint center = grect_center_point(&bounds);
  gpath_move_to(s_minute_arrow, center);
  gpath_move_to(s_hour_arrow, center);

  for (int i = 0; i < NUM_CLOCK_TICKS; ++i) {
    s_tick_paths[i] = gpath_create(&ANALOG_BG_POINTS[i]);
  }
  
}

static void deinit() {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "deinit");
  persistReadings();
  persistCalibration();
  persistC(SLOPEKEY, slope);
  persistC(INTERCEPTKEY, intercept);
  gpath_destroy(s_minute_arrow);
  gpath_destroy(s_hour_arrow);

  for (int i = 0; i < NUM_CLOCK_TICKS; ++i) {
    gpath_destroy(s_tick_paths[i]);
  }

  // Destroy main Window
  window_destroy(s_main_window);
  window_destroy(CalWindow);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
