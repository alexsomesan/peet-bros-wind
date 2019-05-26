#define windSpeedPin 34
#define windDirPin 35

void readWindSpeed();
void readWindDir();
boolean checkDirDev(long knots, int dev);
boolean checkSpeedDev(long knots, int dev);
void calcWindSpeedAndDir();

extern const float filterGain;
extern volatile boolean newData;