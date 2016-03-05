#include <pebble.h>
#include <math.h>

#define calibrations_secondskey1 1 
#define calibrations_glucosekey1 2 
#define calibrations_rawcountkey1 3 
#define calibrations_secondskey2 4 
#define calibrations_glucosekey2 5
#define calibrations_rawcountkey2 6 
#define calibrations_secondskey3 7 
#define calibrations_glucosekey3 8 
#define calibrations_rawcountkey3 9 
#define calibrations_secondskey4 10 
#define calibrations_glucosekey4 11 
#define calibrations_rawcountkey4 12 
#define calibrations_secondskey5 13 
#define calibrations_glucosekey5 14 
#define calibrations_rawcountkey5 15 

//persist slope and intercept
//values provided when paired with phone
#define SLOPEKEY 16
#define INTERCEPTKEY 17 

int32_t slope = 705;
int32_t intercept = 30005;
int32_t tmpSlope;
int32_t tmpIntercept;


struct calibrations {
  long seconds;
  long rawcounts;
  int glucose;
};

struct calibrations calibrations_arr[5];
struct calibrations calibration;

void initCalibrations() {
  APP_LOG(APP_LOG_LEVEL_DEBUG,"initCalibrations");
  calibration.glucose = 0;
  calibration.rawcounts = 0;
  calibration.seconds = 0;
  for (int i = 0; i < 5; i++ ) {
    calibrations_arr[i].glucose = calibration.glucose;
    calibrations_arr[i].rawcounts = calibration.rawcounts;
    calibrations_arr[i].seconds = calibration.seconds;
  }
}

void persist(int key,long value){
  while(persist_write_int(key, value)<0){
    persist_write_int(key,value);
  }
}

void persistCalibration() {
  APP_LOG(APP_LOG_LEVEL_DEBUG,"persistCalibration");
  if (calibrations_arr[0].rawcounts != 0) {
    persist(calibrations_glucosekey1, calibrations_arr[0].glucose);
    persist(calibrations_secondskey1,  calibrations_arr[0].seconds);
    persist(calibrations_rawcountkey1,  calibrations_arr[0].rawcounts);
  }

  if (calibrations_arr[1].rawcounts != 0) {
    persist(calibrations_glucosekey2, calibrations_arr[1].glucose);
    persist(calibrations_secondskey2,  calibrations_arr[1].seconds);
    persist(calibrations_rawcountkey2,  calibrations_arr[1].rawcounts);
  }

  if (calibrations_arr[2].rawcounts != 0) {
    persist(calibrations_glucosekey3, calibrations_arr[2].glucose);
    persist(calibrations_secondskey3,  calibrations_arr[2].seconds);
    persist(calibrations_rawcountkey3,  calibrations_arr[2].rawcounts);
  }
  if (calibrations_arr[3].rawcounts != 0) {
    persist(calibrations_glucosekey4, calibrations_arr[3].glucose);
    persist(calibrations_secondskey4,  calibrations_arr[3].seconds);
    persist(calibrations_rawcountkey4,  calibrations_arr[3].rawcounts);
  }

  if (calibrations_arr[4].rawcounts != 0) {
    persist(calibrations_glucosekey5, calibrations_arr[4].glucose);
    persist(calibrations_secondskey5,  calibrations_arr[4].seconds);
    persist(calibrations_rawcountkey5,  calibrations_arr[4].rawcounts);
  }
}

void retrieveCal() {
  APP_LOG(APP_LOG_LEVEL_DEBUG,"retrieveCal");
  if (persist_exists(calibrations_glucosekey1)) {
    calibrations_arr[0].glucose = persist_read_int(calibrations_glucosekey1);
    calibrations_arr[0].seconds = persist_read_int(calibrations_secondskey1);
    calibrations_arr[0].rawcounts = persist_read_int(calibrations_rawcountkey1);
  }

  if (persist_exists(calibrations_glucosekey2)) {
    calibrations_arr[1].glucose = persist_read_int(calibrations_glucosekey2);
    calibrations_arr[1].seconds = persist_read_int(calibrations_secondskey2);
    calibrations_arr[1].rawcounts = persist_read_int(calibrations_rawcountkey2);
  }

  if (persist_exists(calibrations_glucosekey3)) {
    calibrations_arr[2].glucose = persist_read_int(calibrations_glucosekey3);
    calibrations_arr[2].seconds = persist_read_int(calibrations_secondskey3);
    calibrations_arr[2].rawcounts = persist_read_int(calibrations_rawcountkey3);
  }

  if (persist_exists(calibrations_glucosekey4)) {
    calibrations_arr[3].glucose = persist_read_int(calibrations_glucosekey4);
    calibrations_arr[3].seconds = persist_read_int(calibrations_secondskey4);
    calibrations_arr[3].rawcounts = persist_read_int(calibrations_rawcountkey4);
  }

  if (persist_exists(calibrations_glucosekey5)) {
    calibrations_arr[4].glucose = persist_read_int(calibrations_glucosekey5);
    calibrations_arr[4].seconds = persist_read_int(calibrations_secondskey5);
    calibrations_arr[4].rawcounts = persist_read_int(calibrations_rawcountkey5);
  }
}

void addCalibration(long rawcounts, int glucose) {
  APP_LOG(APP_LOG_LEVEL_DEBUG,"addCalibration %ld,%i",rawcounts,glucose);
  //move everything in the array over 1 place
  //then add the new values

  //don't allow the bootstrap value in more than once in a row
  if ((rawcounts == 30000 && calibrations_arr[0].rawcounts != 30000) || (rawcounts>30000)) {
    for (int i = 4; i > 0; i-- ) {
      calibrations_arr[i].seconds = calibrations_arr[i - 1].seconds;
      calibrations_arr[i].rawcounts = calibrations_arr[i - 1].rawcounts;
      calibrations_arr[i].glucose = calibrations_arr[i - 1].glucose;
    }

    time_t sec1;
    uint16_t ms1;
    time_ms(&sec1, &ms1);
    calibrations_arr[0].seconds = sec1;
    calibrations_arr[0].glucose = glucose;
    calibrations_arr[0].rawcounts = rawcounts;
  }
}

void getCalSlopeAndIntercept() {
  APP_LOG(APP_LOG_LEVEL_DEBUG,"getCalSlopeAndIntercept");
  int count = 0;
  double sumx = 0.0, sumy = 0.0, sum1 = 0.0, sum2 = 0.0;

  for (int i = 0; i < 5; i++ ) {
    if (calibrations_arr[i].rawcounts > 0) {
      count++;
      sumx = sumx + calibrations_arr[i].glucose;
      sumy = sumy + calibrations_arr[i].rawcounts;
      APP_LOG(APP_LOG_LEVEL_DEBUG,"glucose %i rawcounts %ld",calibrations_arr[i].glucose, calibrations_arr[i].rawcounts);
    }
  }

  double xmean = sumx / count;
  double ymean = sumy / count;

  for (int i = 0; i < count; i ++) {
    if (calibrations_arr[i].rawcounts > 0) {
      sum1 = sum1 + ((calibrations_arr[i].glucose - xmean) * (calibrations_arr[i].rawcounts - ymean));
      sum2 = sum2 + pow(calibrations_arr[i].glucose - xmean, 2);
    }
  }
  if (sum2 == 0) {
    tmpSlope = 0;
    tmpIntercept = 0;
  } else {
    // derive the least squares equation
    tmpSlope = sum1 / sum2;
    tmpIntercept = (long)ymean - ((long)tmpSlope * (long)xmean);
  }
}

void calcSlopeandInt() {
  APP_LOG(APP_LOG_LEVEL_DEBUG,"calcSlopeandInt");
     tmpSlope = 0;
     tmpIntercept = 0;
    
    getCalSlopeAndIntercept();
    APP_LOG(APP_LOG_LEVEL_DEBUG,"Cal %ld:%ld",tmpSlope,tmpIntercept);
    if ((tmpSlope > 300 && tmpSlope < 2000) && (tmpIntercept < 60000 && tmpIntercept > 10000)) {
        APP_LOG(APP_LOG_LEVEL_DEBUG,"Normal Calc");
        slope = tmpSlope;
        intercept = tmpIntercept;
    } else {
        //fall back to bootstrap value and most recent calibration
        APP_LOG(APP_LOG_LEVEL_DEBUG,"Exception Calc");
        int tmpGlucose = calibrations_arr[0].glucose;
        int tmpRawcount=calibrations_arr[0].rawcounts;
        initCalibrations();
        addCalibration(30000, 0);
        addCalibration(tmpRawcount, tmpGlucose);
        getCalSlopeAndIntercept();
        //
       APP_LOG(APP_LOG_LEVEL_DEBUG,"Cal %ld:%ld",tmpSlope,tmpIntercept);
        if ((tmpSlope > 300 && tmpSlope < 2000) && (tmpIntercept < 60000 && tmpIntercept > 10000)) {
            slope = tmpSlope;
            intercept = tmpIntercept;
        } else {
            //this is a bad sensor error that should be handled
            //reject calibration and fall back to original cal array
           APP_LOG(APP_LOG_LEVEL_DEBUG,"well this was unexpected");
           retrieveCal();
        }
    }
    persistCalibration();
    persist(SLOPEKEY,slope);
    persist(INTERCEPTKEY,intercept);
}

void updateRawcount(long rawcount){
  calibrations_arr[0].rawcounts = rawcount;
}
