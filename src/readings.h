#include <pebble.h>
#include <math.h>

#define readings_minuteskey1 20
#define readings_glucosekey1 21 
#define readings_rawcountskey1 22 
#define readings_minuteskey2 23 
#define readings_glucosekey2 24 
#define readings_rawcountskey2 25
#define readings_minuteskey3 26 
#define readings_glucosekey3 27 
#define readings_rawcountskey3 28 

struct readings {
  long minutes;
  long rawcounts;
  int glucose;
};

struct readings readings_arr[5];
struct readings reading;

void initReadings() {
  reading.minutes = 0;
  reading.glucose = 0;
  reading.rawcounts=0;
  for (int i = 0; i < 3; i++ ) {
    readings_arr[i].minutes = reading.minutes;
    readings_arr[i].glucose = reading.glucose;
    readings_arr[i].rawcounts = reading.rawcounts;
  }
}

void persistReadings() {
  APP_LOG(APP_LOG_LEVEL_DEBUG,"persistReadings");
  if (readings_arr[0].glucose != 0) {
    persist(readings_glucosekey1, readings_arr[0].glucose);
    persist(readings_minuteskey1,  readings_arr[0].minutes);
    persist(readings_rawcountskey1,  readings_arr[0].rawcounts);
    APP_LOG(APP_LOG_LEVEL_DEBUG,"%i : %ld",readings_arr[0].glucose,readings_arr[0].rawcounts);
  }

  if (readings_arr[1].glucose != 0) {
    persist(readings_glucosekey2, readings_arr[1].glucose);
    persist(readings_minuteskey2,  readings_arr[1].minutes);
    persist(readings_rawcountskey2,  readings_arr[1].rawcounts);
    APP_LOG(APP_LOG_LEVEL_DEBUG,"%i : %ld",readings_arr[1].glucose,readings_arr[1].rawcounts);
  }

  if (readings_arr[2].glucose != 0) {
    persist(readings_glucosekey3, readings_arr[2].glucose);
    persist(readings_minuteskey3,  readings_arr[2].minutes);
    persist(readings_rawcountskey3,  readings_arr[2].rawcounts);
    APP_LOG(APP_LOG_LEVEL_DEBUG,"%i : %ld",readings_arr[2].glucose,readings_arr[2].rawcounts);
  }
}

void addReading(int glucose,long rawcounts) {
  APP_LOG(APP_LOG_LEVEL_DEBUG,"addReading %i, %ld",glucose,rawcounts);
  //move everything in the array over 1 place
  //then add the new values
  for (int i = 2; i > 0; i-- ) {
    readings_arr[i].minutes = readings_arr[i - 1].minutes;
    readings_arr[i].glucose = readings_arr[i - 1].glucose;
    readings_arr[i].rawcounts = readings_arr[i - 1].rawcounts;
  }

  time_t sec1;
  uint16_t ms1;
  time_ms(&sec1, &ms1);
  readings_arr[0].minutes = (int)round(sec1/60);
  readings_arr[0].glucose = glucose;
  readings_arr[0].rawcounts = rawcounts;
}

void retrieveReadings() {
  APP_LOG(APP_LOG_LEVEL_DEBUG,"retrieveReadings");
  if (persist_exists(readings_glucosekey1)) {
    readings_arr[0].glucose = persist_read_int(readings_glucosekey1);
    readings_arr[0].minutes = persist_read_int(readings_minuteskey1);
    readings_arr[0].rawcounts = persist_read_int(readings_rawcountskey1);
  }

  if (persist_exists(readings_glucosekey2)) {
    readings_arr[1].glucose = persist_read_int(readings_glucosekey2);
    readings_arr[1].minutes = persist_read_int(readings_minuteskey2);
    readings_arr[1].rawcounts = persist_read_int(readings_rawcountskey2);
  }

  if (persist_exists(readings_glucosekey3)) {
    readings_arr[2].glucose = persist_read_int(readings_glucosekey3);
    readings_arr[2].minutes = persist_read_int(readings_minuteskey3);
    readings_arr[2].rawcounts = persist_read_int(readings_rawcountskey3);
  }

}

//using the actual minutes since the epoch value underreports the slope
//so adjust everything based on the first value
//should end up being 0,5 and 10 minutes 
double getSlopeGlucose() {
  APP_LOG(APP_LOG_LEVEL_DEBUG,"getSlopeGlucose");
  int count = 0;
  double sumx = 0.0, sumy = 0.0, sum1 = 0.0, sum2 = 0.0;
  
  for (int i = 0; i < 3; i++ ) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Slope Calc Glucose %d %lu", readings_arr[i].glucose, readings_arr[i].minutes);
    if (readings_arr[i].glucose > 20) {
      count++;
      sumx = sumx + readings_arr[i].minutes;
      sumy = sumy + readings_arr[i].glucose;
    }
  }

  double xmean = sumx / count;
  double ymean = sumy / count;

  for (int i = 0; i < count; i ++) {
    if (readings_arr[i].glucose > 20) {
      sum1 = sum1 + ((readings_arr[i].minutes - xmean) * (readings_arr[i].glucose - ymean));
      sum2 = sum2 + pow(readings_arr[i].minutes - xmean, 2);
    }
  }

  // derive the least squares equation
  if (sum2 == 0 || sum1 ==0) {
    return 0;
  }
  double slope = sum1 / sum2;
  return slope;
}
