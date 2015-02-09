#include "pebble.h"

static Window *window;
#define _DATE_BUF_LEN 26
//static char _DATE_BUFFER[_DATE_BUF_LEN];
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
static uint8_t sync_buffer[128];
 long last_reading=0;
 uint8_t miss_count=0;
 uint8_t sensor_miss_count=0;
int16_t currentGlucose=0;
int16_t lastGlucose=0;
int16_t timeToLimit=0;

int watchCallbackCount=0;
int lastWatchCallbackCount=0;


//slope direction
static int SLOPE_DOWN = 0x01;
static int SLOPE_UP = 0x02;
int slopeDirection=0;
//
enum GlucoseKey {
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

//accel

// Total Steps (TS)
#define TS 1
// Total Steps Default (TSD)
#define TSD 1

// Timer used to determine next step check
static AppTimer *timer;

// interval to check for next step (in ms)
const int ACCEL_STEP_MS = 475;
// value to auto adjust step acceptance 
const int PED_ADJUST = 2;
// steps required per calorie
const int STEPS_PER_CALORIE = 22;
// value by which step goal is incremented
const int STEP_INCREMENT = 50;
// values for max/min number of calibration options 
const int MAX_CALIBRATION_SETTINGS = 3;
const int MIN_CALIBRATION_SETTINGS = 1;

int X_DELTA = 45;
int Y_DELTA, Z_DELTA = 235;
int YZ_DELTA_MIN = 225;
int YZ_DELTA_MAX = 245; 
int X_DELTA_TEMP, Y_DELTA_TEMP, Z_DELTA_TEMP = 0;
int lastX, lastY, lastZ, currX, currY, currZ = 0;
int sensitivity = 1;

long stepGoal = 0;
int  pedometerCount = 0;
long caloriesBurned = 0;
long tempTotal = 0;

bool did_pebble_vibrate = false;
bool validX, validY, validZ = false;
bool SID;
bool isDark;
bool startedSession = true;
//
//
bool msg_run = false;

//
//
char *translate_error(AppMessageResult result) {
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
    pedometerCount=0;
    msg_run = false;
}


void out_failed_handler(DictionaryIterator *failed, AppMessageResult reason, void *context) {
    // outgoing message failed
    APP_LOG(APP_LOG_LEVEL_DEBUG, "DICTIONARY NOT SENT! ERROR!");
    APP_LOG(APP_LOG_LEVEL_DEBUG, "In dropped: %i - %s", reason, translate_error(reason));
   //text_layer_set_text(load_layer, "ERROR!!!!");
    msg_run = false;

}

static void appmsg_in_dropped(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "In dropped: %i", reason);
   APP_LOG(APP_LOG_LEVEL_DEBUG, "In dropped: %i - %s", reason, translate_error(reason));
}


static void sync_error_callback(DictionaryResult dict_error, AppMessageResult app_message_error, void *context) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "App Message Sync Error: %d", app_message_error);
   APP_LOG(APP_LOG_LEVEL_DEBUG, "In dropped: %i - %s", app_message_error, translate_error(app_message_error));
}


void accel_data_handler (){
 
   //memset(&sync_buffer,0,sizeof(sync_buffer));
  DictionaryIterator *iter;
    // create dictionary object
    app_message_outbox_begin(&iter);
    
    Tuplet val = TupletInteger(0,pedometerCount);
    dict_write_tuplet(iter, &val);
    pedometerCount=0;

    // send dictionary to phone
    app_message_outbox_send();
   // app_sync_deinit(&sync);
}


////
////
////
//accel
void autoCorrectZ(){
	if (Z_DELTA > YZ_DELTA_MAX){
		Z_DELTA = YZ_DELTA_MAX; 
	} else if (Z_DELTA < YZ_DELTA_MIN){
		Z_DELTA = YZ_DELTA_MIN;
	}
}

void autoCorrectY(){
	if (Y_DELTA > YZ_DELTA_MAX){
		Y_DELTA = YZ_DELTA_MAX; 
	} else if (Y_DELTA < YZ_DELTA_MIN){
		Y_DELTA = YZ_DELTA_MIN;
	}
}

void pedometer_update() {
	if (startedSession) {
		X_DELTA_TEMP = abs(abs(currX) - abs(lastX));
		if (X_DELTA_TEMP >= X_DELTA) {
			validX = true;
		}
		Y_DELTA_TEMP = abs(abs(currY) - abs(lastY));
		if (Y_DELTA_TEMP >= Y_DELTA) {
			validY = true;
			if (Y_DELTA_TEMP - Y_DELTA > 200){
				autoCorrectY();
				Y_DELTA = (Y_DELTA < YZ_DELTA_MAX) ? Y_DELTA + PED_ADJUST : Y_DELTA;
			} else if (Y_DELTA - Y_DELTA_TEMP > 175){
				autoCorrectY();
				Y_DELTA = (Y_DELTA > YZ_DELTA_MIN) ? Y_DELTA - PED_ADJUST : Y_DELTA;
			}
		}
		Z_DELTA_TEMP = abs(abs(currZ) - abs(lastZ));
		if (abs(abs(currZ) - abs(lastZ)) >= Z_DELTA) {
			validZ = true;
			if (Z_DELTA_TEMP - Z_DELTA > 200){
				autoCorrectZ();
				Z_DELTA = (Z_DELTA < YZ_DELTA_MAX) ? Z_DELTA + PED_ADJUST : Z_DELTA;
			} else if (Z_DELTA - Z_DELTA_TEMP > 175){
				autoCorrectZ();
				Z_DELTA = (Z_DELTA < YZ_DELTA_MAX) ? Z_DELTA + PED_ADJUST : Z_DELTA;
			}
		}
	} else {
		startedSession = true;
	}
}

void resetUpdate() {
	lastX = currX;
	lastY = currY;
	lastZ = currZ;
	validX = false;
	validY = false;
	validZ = false;
}


static void timer_callback(void *data) {
  //APP_LOG(APP_LOG_LEVEL_DEBUG,"timer_callback");
	AccelData accel = (AccelData ) { .x = 0, .y = 0, .z = 0 };
	accel_service_peek(&accel);
  //APP_LOG(APP_LOG_LEVEL_DEBUG,"xyz %i,%i,%i",accel.x,accel.y,accel.z);
	if (!startedSession) {
		lastX = accel.x;
		lastY = accel.y;
		lastZ = accel.z;
	} else {
		currX = accel.x;
		currY = accel.y;
		currZ = accel.z;
	}
	
	did_pebble_vibrate = accel.did_vibrate;

	pedometer_update();
	//update_ui_callback();
	if ((validX && validY && !did_pebble_vibrate) || (validX && validZ && !did_pebble_vibrate)) {
    //APP_LOG(APP_LOG_LEVEL_DEBUG,"Update UI Callback");
		pedometerCount++;
		tempTotal++;

		caloriesBurned = (int) (pedometerCount / STEPS_PER_CALORIE);
		static char calBuf[] = "123456890abcdefghijkl";
		snprintf(calBuf, sizeof(calBuf), "%ld Calories", caloriesBurned);
		//text_layer_set_text(calories, calBuf);

		static char buf[] = "123456890abcdefghijkl";
		snprintf(buf, sizeof(buf), "%i", pedometerCount);
		//text_layer_set_text(pedCount, buf);

		static char buf2[] = "123456890abcdefghijkl";
		snprintf(buf2, sizeof(buf2), "%ld in Total", tempTotal);
		//menu_items[2].subtitle = buf2;

		static char buf3[] = "1234567890abcdefg";
		snprintf(buf3, sizeof(buf3), "%ld Burned",
				(long) (tempTotal / STEPS_PER_CALORIE));

	}

	resetUpdate();
  //
	//layer_mark_dirty(window_get_root_layer(pedometer));
	timer = app_timer_register(ACCEL_STEP_MS, timer_callback, NULL);
}


void handle_deinit(void) {
	//totalSteps += pedometerCount;
	//persist_write_int(TS, totalSteps);
	//persist_write_bool(SID, isDark);
	accel_data_service_unsubscribe();
	//window_destroy(menu_window);
}



  //should be called every ~5 minutes
  //triggered by change in reading time from dexcom
static void alerts(){

  APP_LOG(APP_LOG_LEVEL_DEBUG,"In Alerts");
  
  // accel_data_handler ();
  
  // Vibe pattern: ON for 200ms, OFF for 100ms, ON for 400ms:
uint32_t  segments[] = { 200, 100, 200,100, 200, 100 };
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
                if (currentGlucose<80 && alertCount==0 && currentGlucose>60){
                   vibes_enqueue_custom_pattern(pat1);
                }
                
                if(currentGlucose<80 && alertCount>0  && currentGlucose>60){
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
  //APP_LOG(APP_LOG_LEVEL_DEBUG,"Pedometer count %i",pedometerCount);
  //watchCallbackCount++;
  miss_count=0;
  //text_layer_set_text(alert_layer, " ");
  switch (key) {
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
       if(abs(lastGlucose-currentGlucose )>25 && lastGlucose!=0 &&currentGlucose!=0){
          text_layer_set_text(alert_layer, "???");
        }
    
        text_layer_set_text(glucose_layer, glucbuf);
        lastGlucose=currentGlucose;
        break;
    //
     case LASTREADING_KEY:
        APP_LOG(APP_LOG_LEVEL_DEBUG,"LASTREADING KEY"); 
        //APP_LOG(APP_LOG_LEVEL_DEBUG,new_tuple->value->cstring); 
        this_reading=atol(new_tuple->value->cstring);
        APP_LOG(APP_LOG_LEVEL_DEBUG,"%lu %lu",this_reading,last_reading);  
        //if watch hasn't received an update in ~6 minutes, alert user
        if (this_reading==last_reading){
          sensor_miss_count++;
          if(sensor_miss_count>6){
            APP_LOG(APP_LOG_LEVEL_DEBUG,"Sensor Miss recorded"); 
            //vibes_double_pulse();
            text_layer_set_text(alert_layer, "!");
            sensor_miss_count=0;
          }
        }else{
          APP_LOG(APP_LOG_LEVEL_DEBUG,"Calling Alerts");   
          sensor_miss_count=0;
          text_layer_set_text(alert_layer, " ");
          if(currentGlucose >20){
            alerts();
          }else{
            text_layer_set_text(alert_layer, "?");
          }
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
  //

}

static void update_time() {
  // Get a tm structure
  time_t temp = time(NULL); 
  struct tm *tick_time = localtime(&temp);

  // Create a long-lived buffer
  static char buffer[] = "00:00";
  static char buffer1[] = "Mon     00:00";
  // Write the current hours and minutes into the buffer
  if(clock_is_24h_style() == true) {
    //Use 2h hour format
    strftime(buffer, sizeof("00:00"), "%H:%M", tick_time);
  } else {
    //Use 12 hour format
    strftime(buffer,  sizeof("00:00"), "%l:%M", tick_time);
    
    // Display this time on the TextLayer
    text_layer_set_text(s_time_layer, buffer);
    
    strftime(buffer1,  sizeof("Mon  00/00"), "%a  %m/%e", tick_time);
    // Display this time on the TextLayer
    text_layer_set_text(date_layer, buffer1);
  }


}
  
static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  //this also seems to get called when ios connects
  //not just every minute...
  update_time();
  miss_count++;
 APP_LOG(APP_LOG_LEVEL_DEBUG,"In tick handler %i, %i",miss_count,sensor_miss_count); 
  if(miss_count>7){
    APP_LOG(APP_LOG_LEVEL_DEBUG,"Miss recorded"); 
    vibes_double_pulse();
    text_layer_set_text(alert_layer, "!!");
    miss_count=0;
  }
  
}


static void window_load(Window *window) {
  APP_LOG(APP_LOG_LEVEL_DEBUG,"Window Load");
  Layer *window_layer = window_get_root_layer(window);
  
  //clock
  // Create time TextLayer
  s_time_layer = text_layer_create(GRect(1, 25, 144, 50));
  text_layer_set_background_color(s_time_layer, GColorClear);
  text_layer_set_text_color(s_time_layer, GColorWhite);
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
  test_layer = text_layer_create(GRect(10, 60, 144, 68));
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
  
  Tuplet initial_values[] = {
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
  //
   miss_count=0;
  sensor_miss_count=0;
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

  //app_comm_set_sniff_interval(SNIFF_INTERVAL_REDUCED);
  //app_message_register_inbox_dropped(appmsg_in_dropped);
  //app_message_register_outbox_sent(out_sent_handler);
  //app_message_register_outbox_failed(out_failed_handler);
  
  const int inbound_size = 64;
  const int outbound_size = 64;
  app_message_open(inbound_size, outbound_size);
  

  // Register with TickTimerService
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  
  //accel timer  
  //accel_data_service_subscribe(0, NULL);
  //timer = app_timer_register(ACCEL_STEP_MS, timer_callback, NULL);
  
  const bool animated = true;
  window_stack_push(window, animated);
    

}

static void deinit() {
  //app_comm_set_sniff_interval(SNIFF_INTERVAL_NORMAL);
  //accel_data_service_unsubscribe();
  window_destroy(window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
