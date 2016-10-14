#include <pebble.h>

uint16_t alertCount=0;

// Vibe pattern: ON for 400ms, OFF for 400ms ect:
uint32_t  segments[] = { 400, 400, 400, 400, 400, 400, 400, 400, 400, 400, 400, 400 };
VibePattern pat = {
  .durations = segments,
  .num_segments = ARRAY_LENGTH(segments),
};

uint32_t  segments_short[] = { 400, 400 };
VibePattern pat_short = {
  .durations = segments_short,
  .num_segments = ARRAY_LENGTH(segments_short),
};

uint32_t  segments_restart[] = { 400, 200,400,200 };
VibePattern pat_restart = {
  .durations = segments_restart,
  .num_segments = ARRAY_LENGTH(segments_restart),
};

//should be called every ~5 minutes
//triggered by change in reading time from dexcom
static void alerts(int currentGlucose, int timeToLimit) {

  APP_LOG(APP_LOG_LEVEL_DEBUG, "In Alerts");

  //
  if (currentGlucose > 80 && currentGlucose < 180) {
    alertCount = 0;
  }

  if (currentGlucose < 80 && alertCount == 0 && currentGlucose > 60) {
     APP_LOG(APP_LOG_LEVEL_DEBUG, "between 60 and 80, alert");
     vibes_enqueue_custom_pattern(pat);
  }

  if (currentGlucose < 80  && currentGlucose > 60) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "between 60 and 80, increment");
    alertCount++;
    if (alertCount >= 2) {
      alertCount = 0;
    }
  }

  if (currentGlucose < 60 && alertCount == 0) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "below 60, alert");
    vibes_enqueue_custom_pattern(pat);
  }
  
  if (currentGlucose < 60) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "below 60, increment");
    alertCount++;
    if (alertCount >= 1) {
      alertCount = 0;
    }
  }

  if (currentGlucose > 180 && alertCount == 0)
  {
    vibes_enqueue_custom_pattern(pat);
  }

  if (currentGlucose > 180 ) {
    alertCount++;
    if (alertCount >= 24) {
      alertCount = 0;
    }
  }
  
  //quick alert whenever predicted to be at high or low limit
  if (timeToLimit == 1)
  {
    vibes_enqueue_custom_pattern(pat_short);
  }

 
}
