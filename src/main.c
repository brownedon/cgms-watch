#include "pebble.h"
#include<math.h>

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
char testbuf[15];
static char glucose[16];

static BitmapLayer *icon_layer;
static GBitmap *icon_bitmap = NULL;

static AppSync sync;
//static uint8_t sync_buffer[128];
static uint8_t sync_buffer[256];
 int32_t last_reading=0;
 uint8_t miss_count=0;
 uint8_t sensor_miss_count=0;
int32_t currentGlucose=0;
int16_t lastGlucose=0;
int16_t timeToLimit=0;

int32_t slope=700;
int32_t intercept=30000;
long isig=0;

int watchCallbackCount=0;
int lastWatchCallbackCount=0;

//slope direction
static int SLOPE_DOWN = 0x01;
static int SLOPE_UP = 0x02;
int slopeDirection=0;
//

//persist slope and intercept
//values provided when paired with phone
#define SLOPEKEY 700
#define INTERCEPTKEY 30000

enum GlucoseKey {
  GLUCOSE_KEY = 0x1,  
  ARROW_KEY=0x2,
  SLOPEDIRECTION_KEY=0x3,
  TIMETOLIMIT_KEY=0x4,
  LASTREADING_KEY=0x5,
  slopeKey=0x6,
  interceptKey=0x7,
  isigKey=0x8
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

 // Vibe pattern: ON for 400ms, OFF for 400ms ect:
uint32_t  segments[] = { 400, 400, 400,400, 400, 400, 400, 400, 400,400, 400, 400 };
VibePattern pat = {
  .durations = segments,
  .num_segments = ARRAY_LENGTH(segments),
};

uint32_t  segments_short[] = { 400, 400 };
VibePattern pat_short = {
  .durations = segments_short,
  .num_segments = ARRAY_LENGTH(segments_short),
};

bool msg_run = false;

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

struct readings {
  long seconds;
  int glucose;
};

struct readings readings_arr[76];
struct readings reading;

void initReadings(){
  reading.seconds=0;
  reading.glucose=0;
  for (int i=0; i>12; i++ ) {  
    readings_arr[i].seconds=reading.seconds;
    readings_arr[i].glucose=reading.glucose;
  }
}

void addReading(int glucose) {
  //move everything in the array over 1 place
  //then add the new values
  for (int i = 12; i > 0; i-- ) {
    readings_arr[i].seconds = readings_arr[i - 1].seconds;
    readings_arr[i].glucose = readings_arr[i - 1].glucose;
  }

  time_t sec1;
  uint16_t ms1;
  time_ms(&sec1, &ms1);
  
  readings_arr[0].seconds = ms1/1000;
  readings_arr[0].glucose = glucose;

}

float getSlopeGlucose() {

  int count = 0;
  float sumx = 0.0, sumy = 0.0, sum1 = 0.0, sum2 = 0.0;

  for (int i = 0; i < 3; i++ ) {
    if (readings_arr[i].glucose > 20) {
      count++;
      sumx = sumx + readings_arr[i].seconds / 60;
      sumy = sumy + readings_arr[i].glucose;
    }
  }

  float xmean = sumx / count;
  float ymean = sumy / count;

  for (int i = 0; i < count; i ++) {
    if (readings_arr[i].glucose > 20) {
      sum1 = sum1 + (readings_arr[i].seconds / 60 - xmean) * (readings_arr[i].glucose - ymean);
      sum2 = sum2 + pow((readings_arr[i].seconds / 60 - xmean), 2);
    }
  }

  // derive the least squares equation
  if (sum2 == 0) {
    return 0;
  }
  float slope = sum1 / sum2;
 
  return slope;
};

void out_sent_handler(DictionaryIterator *sent, void *context) {
    // outgoing message was delivered
    APP_LOG(APP_LOG_LEVEL_DEBUG, "****DICTIONARY SENT SUCCESSFULLY!****");
    msg_run = false;
}


void out_failed_handler(DictionaryIterator *failed, AppMessageResult reason, void *context) {
    // outgoing message failed
    APP_LOG(APP_LOG_LEVEL_DEBUG, "DICTIONARY NOT SENT! ERROR!");
    APP_LOG(APP_LOG_LEVEL_DEBUG, "In dropped: %i - %s", reason, translate_error(reason));
   //text_layer_set_text(load_layer, "ERROR!!!!");
    msg_run = false;

}


static void sync_error_callback(DictionaryResult dict_error, AppMessageResult app_message_error, void *context) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "App Message Sync Error: %d", app_message_error);
   APP_LOG(APP_LOG_LEVEL_DEBUG, "In dropped: %i - %s", app_message_error, translate_error(app_message_error));
}


  //should be called every ~5 minutes
  //triggered by change in reading time from dexcom
static void alerts(){

  APP_LOG(APP_LOG_LEVEL_DEBUG,"In Alerts");
  
                //for rapid rise or fall notify every time it occurs
                if(ARROW==ARROW_DOWN_DOWN){
                    vibes_enqueue_custom_pattern(pat);
                }
            
                if(ARROW==ARROW_UP_UP){
                    vibes_enqueue_custom_pattern(pat);
                }
            
                //
                if (currentGlucose<80 && alertCount==0 && currentGlucose>60){
                   alertCount++;
                   vibes_enqueue_custom_pattern(pat);
                }
                
                if(currentGlucose<80 && alertCount>0  && currentGlucose>60){
                    alertCount++;
                    if(alertCount==3){
                        alertCount=0;
                    }
                }
        
                if (currentGlucose<60 && alertCount==0){
                    alertCount++;
                    vibes_enqueue_custom_pattern(pat);
                }
                if(currentGlucose<60 && alertCount>0){
                    alertCount++;
                    if(alertCount==2){
                        alertCount=0;
                    }
                }
            
                if(currentGlucose>180 && alertCount==0)
                {
                    alertCount++;
                    vibes_enqueue_custom_pattern(pat);
                }
                
                //quick alert whenever predicted to be at high or low limit
                if(timeToLimit==1)
                {
                    vibes_enqueue_custom_pattern(pat_short);
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

static void update_time() {
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
  int32_t this_reading=0;

  APP_LOG(APP_LOG_LEVEL_DEBUG,"IN Sync Callback"); 
  //APP_LOG(APP_LOG_LEVEL_DEBUG,"Pedometer count %i",pedometerCount);
  //watchCallbackCount++;
  miss_count=0;
  //text_layer_set_text(alert_layer, " ");
  switch (key) {
     case GLUCOSE_KEY:
        if(isig==0){
          APP_LOG(APP_LOG_LEVEL_DEBUG,"GLUCOSE KEY");       
          currentGlucose=(new_tuple->value->int32);
          
          APP_LOG(APP_LOG_LEVEL_DEBUG,"GLUCOSE %lu",currentGlucose); 
          snprintf(glucbuf, sizeof(glucbuf), "%lu", currentGlucose);
    
          if (ARROW==ARROW_45_UP){
                snprintf(glucbuf, sizeof(glucbuf), "%lu  /", currentGlucose);
          }
    
          if (ARROW==ARROW_UP){
                snprintf(glucbuf, sizeof(glucbuf), "%lu  ^", currentGlucose);
          }
    
          if (ARROW==ARROW_UP_UP){
                snprintf(glucbuf, sizeof(glucbuf), "%lu  ^^", currentGlucose);
          }
    
          if (ARROW==ARROW_45_DOWN){
                snprintf(glucbuf, sizeof(glucbuf), "%lu  \\", currentGlucose);
          }
    
          if (ARROW==ARROW_DOWN){
                snprintf(glucbuf, sizeof(glucbuf), "%lu  V", currentGlucose);
          }
    
         if (ARROW==ARROW_DOWN_DOWN){
                snprintf(glucbuf, sizeof(glucbuf), "%lu  VV", currentGlucose);
         }                
         if(abs(lastGlucose-currentGlucose )>25 && lastGlucose!=0 &&currentGlucose!=0){
            text_layer_set_text(alert_layer, "???");
          }
    
          text_layer_set_text(glucose_layer, glucbuf);
          addReading( currentGlucose);
          lastGlucose=currentGlucose;
        }
        break;
    //
     case LASTREADING_KEY:
        APP_LOG(APP_LOG_LEVEL_DEBUG,"LASTREADING KEY"); 
        //APP_LOG(APP_LOG_LEVEL_DEBUG,new_tuple->value->int32); 
       this_reading=(new_tuple->value->int32);
       // this_reading=atol(new_tuple->value->cstring);
        APP_LOG(APP_LOG_LEVEL_DEBUG,"%lu %lu",this_reading,last_reading);  
        //  text_layer_set_text(test_layer,(new_tuple->value->cstring));
        //if watch hasn't received an update in ~6 minutes, alert user
        if (this_reading==last_reading){
          sensor_miss_count++;
          if(sensor_miss_count>9){
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
    //
    case slopeKey:
      APP_LOG(APP_LOG_LEVEL_DEBUG,"Slope KEY");
      slope=(new_tuple->value->int32);
      APP_LOG(APP_LOG_LEVEL_DEBUG,"Slope %lu",slope);
      persist_write_int(SLOPEKEY, slope);
      break;
    case interceptKey:
      APP_LOG(APP_LOG_LEVEL_DEBUG,"Intercept KEY");
      intercept=(new_tuple->value->uint32);
      APP_LOG(APP_LOG_LEVEL_DEBUG,"Intercept %lu",intercept);
      persist_write_int(INTERCEPTKEY, intercept);
      break;
    case isigKey:
      APP_LOG(APP_LOG_LEVEL_DEBUG,"ISIG KEY");
      isig=(new_tuple->value->uint32);
      APP_LOG(APP_LOG_LEVEL_DEBUG,"ISIG %lu",isig);

      APP_LOG(APP_LOG_LEVEL_DEBUG,"GLUCOSE %lu",currentGlucose); 
      APP_LOG(APP_LOG_LEVEL_DEBUG,"Intercept %lu",intercept);
      APP_LOG(APP_LOG_LEVEL_DEBUG,"Slope %lu",slope);
      
      if(isig!=0  &&slope!=0 &&intercept!=0){
        
        currentGlucose=((isig-intercept)/slope);
        addReading(currentGlucose);
        snprintf(glucbuf, sizeof(glucbuf), "%lu", currentGlucose);
        
        //snprintf(testbuf, sizeof(testbuf), "%lu", isig);


        float tmpSlope=getSlopeGlucose();
        
        if (tmpSlope < 0) {
          //SLOPE_DIRECTION = SLOPE_DOWN;
          //45 down
          if (abs(tmpSlope) >= 1)
            ARROW = ARROW_45_DOWN;
            //straight down
          if (abs(tmpSlope) >= 2)
            ARROW = ARROW_DOWN;
          if (abs(tmpSlope) >= 3) {
            ARROW = ARROW_DOWN_DOWN;
          }
        }
    
        if (tmpSlope > 0) {
          //SLOPE_DIRECTION = SLOPE_UP;
          if (tmpSlope >= 1)
            ARROW = ARROW_45_UP;
          if (tmpSlope >= 2)
            ARROW = ARROW_UP;
          if (tmpSlope >= 3) {
            ARROW = ARROW_UP_UP;
          }
        }
        
    
          if (ARROW==ARROW_45_UP){
                snprintf(glucbuf, sizeof(glucbuf), "%lu  /", currentGlucose);
          }
    
          if (ARROW==ARROW_UP){
                snprintf(glucbuf, sizeof(glucbuf), "%lu  ^", currentGlucose);
          }
    
          if (ARROW==ARROW_UP_UP){
                snprintf(glucbuf, sizeof(glucbuf), "%lu  ^^", currentGlucose);
          }
    
          if (ARROW==ARROW_45_DOWN){
                snprintf(glucbuf, sizeof(glucbuf), "%lu  \\", currentGlucose);
          }
    
          if (ARROW==ARROW_DOWN){
                snprintf(glucbuf, sizeof(glucbuf), "%lu  V", currentGlucose);
          }
    
         if (ARROW==ARROW_DOWN_DOWN){
                snprintf(glucbuf, sizeof(glucbuf), "%lu  VV", currentGlucose);
         }                
         if(abs(lastGlucose-currentGlucose )>25 && lastGlucose!=0 &&currentGlucose!=0){
            text_layer_set_text(alert_layer, "???");
          }
    
      int test=(tmpSlope*100);
      snprintf(testbuf, sizeof(testbuf), "%i", test);
        
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
        
     if (timeToLimit<99  && timeToLimit>0){
        if (tmpSlope>0){
          snprintf(buf, sizeof(buf), "V %d", timeToLimit);
        }
        if (slopeDirection==SLOPE_UP){
          snprintf(buf, sizeof(buf), "^ %d", timeToLimit);
        }
     }else{
       snprintf(buf, sizeof(buf), "   ");
     }
        
     text_layer_set_text(timetolimit_layer, buf);
        
        if(abs(lastGlucose-currentGlucose )>25 && lastGlucose!=0 &&currentGlucose!=0){
            text_layer_set_text(alert_layer, "???");
          }
        
        text_layer_set_text(glucose_layer, glucbuf);
        text_layer_set_text(test_layer,testbuf);
        lastGlucose=currentGlucose;
        
        APP_LOG(APP_LOG_LEVEL_DEBUG,"Calc GLUCOSE %lu",currentGlucose); 
      }
      break;
   break;
  }
}


  
static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  //this also seems to get called when ios connects
  //not just every minute...
  miss_count++;
 APP_LOG(APP_LOG_LEVEL_DEBUG,"In tick handler %i, %i",miss_count,sensor_miss_count); 
  if(miss_count>11){
    APP_LOG(APP_LOG_LEVEL_DEBUG,"Miss recorded"); 
    vibes_enqueue_custom_pattern(pat);
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
  
    if (persist_exists(SLOPEKEY)) {
            // Load stored count
            APP_LOG(APP_LOG_LEVEL_DEBUG,"Using stored slope");
            slope = persist_read_int(SLOPEKEY);
    }else{
      slope=700;
    }
          if (persist_exists(INTERCEPTKEY)) {
           // Load stored count
           intercept = persist_read_int(INTERCEPTKEY);
          }else{
            intercept=30000;
          }
  
  Tuplet initial_values[] = {
    TupletInteger(GLUCOSE_KEY,0),
    TupletInteger(ARROW_KEY,0),
    TupletInteger(SLOPEDIRECTION_KEY,0),
    TupletInteger(TIMETOLIMIT_KEY,0),
    TupletCString(LASTREADING_KEY,"0"),
    TupletInteger(slopeKey,slope),
    TupletInteger(interceptKey,intercept),
    TupletInteger(isigKey,0)
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
  initReadings();
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
  
  const int inbound_size = 128;
  const int outbound_size = 128;
  app_message_open(inbound_size, outbound_size);
  

  // Register with TickTimerService
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  
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
