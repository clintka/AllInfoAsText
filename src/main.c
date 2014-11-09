#include <pebble.h>

// Keys to link Javascript code to C code.
#define KEY_TEMPERATURE 0
#define KEY_CONDITIONS 1
#define KEY_TEMP_MIN 2
#define KEY_TEMP_MAX 3
#define KEY_WIND_SPEED 4
#define KEY_WIND_DIRECTION 5
#define KEY_HUMIDITY 6
#define KEY_DESCRIPTION 7
#define KEY_DAY1_CONDITIONS 8
#define KEY_DAY1_TEMP_MIN 9
#define KEY_DAY1_TEMP_MAX 10
#define KEY_DAY1_TIME 11
#define KEY_DAY2_CONDITIONS 12
#define KEY_DAY2_TEMP_MIN 13
#define KEY_DAY2_TEMP_MAX 14
#define KEY_DAY2_TIME 15
#define KEY_DAY3_CONDITIONS 16
#define KEY_DAY3_TEMP_MIN 17
#define KEY_DAY3_TEMP_MAX 18
#define KEY_DAY3_TIME 19

// Keys for configuration.
#define CONFIG_KEY_TEMPERATURE_UNITS 50
#define CONFIG_KEY_WINDSPEED_UNITS 51
#define CONFIG_KEY_WEEKNUMBER_ENABLED 52
#define CONFIG_KEY_MONDAY_FIRST 53

// Keys to reference persistent storage.
#define STORAGE_KEY_CURRENT_TEMPERATURE_C 100
#define STORAGE_KEY_CURRENT_CONDITIONS 101
#define STORAGE_KEY_CURRENT_LOW_C 102
#define STORAGE_KEY_CURRENT_HIGH_C 103
#define STORAGE_KEY_CURRENT_WIND_DIR_DEG 104
#define STORAGE_KEY_CURRENT_WIND_SPD_KTS 105
#define STORAGE_KEY_CURRENT_DAY 106
#define STORAGE_KEY_FORECAST_LOW_C 107
#define STORAGE_KEY_FORECAST_HIGH_C 108
#define STORAGE_KEY_FORECAST_CONDITIONS 109
#define STORAGE_KEY_CURRENT_DAY_FORECAST_CONDITIONS 110
#define STORAGE_KEY_TEMPERATURE_UNITS 111
#define STORAGE_KEY_WINDSPEED_UNITS 112
#define STORAGE_KEY_WEEKNUMBER_ENABLED 113
#define STORAGE_KEY_MONDAY_FIRST 114

// Durations for updates and time outs. Set as desired.
#define NUMBER_OF_SECONDS_BETWEEN_WEATHER_UPDATES 1800
#define NUMBER_OF_SECONDS_UNTIL_DATA_CONSIDERED_LOST 60
#define NUMBER_OF_SECONDS_TO_SHOW_SECONDS_AFTER_TAP 180

// Constants for Settings
#define TEMPERATURE_UNITS_F 0
#define TEMPERATURE_UNITS_C 1
#define WINDSPEED_UNITS_KNOTS 0
#define WINDSPEED_UNITS_MPH 1
#define WINDSPEED_UNITS_KPH 2
#define FALSE 0
#define TRUE 1
  
// Persistent storage variables (must be global).
static int currentTemperature_c;
static char currentConditions[32];
static char currentDayForecastConditions[32];
static int currentLowTemperature_c;
static int currentHighTemperature_c;
static int currentWindDirection_deg;
static int currentWindSpeed_kts;
static int currentDate;
static int forecastLowTemperature_c;
static int forecastHighTemperature_c;
static char forecastConditions[32];
static int temperatureUnits; // 0 = F, 1 = C
static int windSpeedUnits; // 0 = KNOTS, 1 = MPH, 2 = KPH
static int weekNumberEnabled; // 0 = FALSE, 1 = TRUE
static int mondayFirst; // 0 = FALSE, 1 = TRUE

// Status variables.
bool isShowingSeconds = false;
bool connectedToBluetooth = false;
bool connectedToData = false;
time_t timeOfLastDataResponse = 0;
time_t timeOfLastDataRequest = 0;
time_t timeOfLastTap = 0;
int lastCalendarDateUpdatedTo = -1;

// Watch layers.
static Window *s_main_window;
static TextLayer *s_battery_layer;
static TextLayer *s_linkStatus_layer;
static TextLayer *s_time_layer;
static TextLayer *s_time_am_pm_layer;
static TextLayer *s_time_seconds_layer;
static TextLayer *s_date_layer;
static TextLayer *s_weather_current_layer;
static TextLayer *s_weather_label1_layer;
static TextLayer *s_weather_forecast1_layer;
static TextLayer *s_weather_label2_layer;
static TextLayer *s_weather_forecast2_layer;
static TextLayer *s_calendarDay_layer[14];

static float getFahrenheitFromCelsius(float temp_celsius)
{
  return ((temp_celsius * 9 / 5) + 32.0);
}

static float getKnotsFromMetersPerSecond(float speed_metersPerSecond)
{
  return (speed_metersPerSecond * 1.94384);
}

static float getPreferedWindSpeed(float windSpeed_kts)
{
  switch(windSpeedUnits)
  {
    case WINDSPEED_UNITS_KNOTS:
      return windSpeed_kts;
    case WINDSPEED_UNITS_MPH:
      return (windSpeed_kts * 1.15077945);
    case WINDSPEED_UNITS_KPH:
      return (windSpeed_kts * 1.85200);    
  }
  
  // Invalid windspeed units.
  return windSpeed_kts;
}

static void update_link_label()
{
  static char bluetoothBuffer[8];
  
  if (connectedToBluetooth)
  {
    if (connectedToData)
    {
      // Connection is good!
      bluetoothBuffer[0] = 0;
    }
    else
    {
      // Bluetooth Connection, but failed to get Data.
      strncpy(bluetoothBuffer, "No Data", 7);
    }
  }
  else
  {
    // No Bluetooth Connection
    strncpy(bluetoothBuffer, "No Link", 7);
  }
  text_layer_set_text(s_linkStatus_layer, bluetoothBuffer);
}

static void update_time(struct tm *tick_time)
{
  static char timeBuffer[6]; // = "24:00";
  static char timeAmPmBuffer[3]; // = "am";
  static char timeSecondsBuffer[3];

  // Update the Time
  if (clock_is_24h_style())
  {
    strftime(timeBuffer, sizeof("00:00"), "%H:%M", tick_time);
    text_layer_set_text(s_time_layer, timeBuffer);

    // If using 24 hours, then don't draw AM/PM and seconds.
    // (At 2400 time, it will run across the AM/PM and seconds).
    timeSecondsBuffer[0] = 0;
    text_layer_set_text(s_time_seconds_layer, timeSecondsBuffer);
    timeAmPmBuffer[0] = 0;
    text_layer_set_text(s_time_am_pm_layer, timeAmPmBuffer);
  }
  else
  {
    strftime(timeBuffer, sizeof("00:00"), "%l:%M", tick_time);
    text_layer_set_text(s_time_layer, timeBuffer);
  
    // Update the seconds field.
    if (weekNumberEnabled == TRUE)
    {
      // Show Week Number in place of the seconds field.
      if (mondayFirst == TRUE)
      {
        strftime(timeSecondsBuffer, sizeof("00"), "%W", tick_time);
        text_layer_set_text(s_time_seconds_layer, timeSecondsBuffer);
      }
      else
      {
        strftime(timeSecondsBuffer, sizeof("00"), "%U", tick_time);
        text_layer_set_text(s_time_seconds_layer, timeSecondsBuffer);
      }
    }
    else if (isShowingSeconds)
    {
      // isShowingSeconds is toggled by a watch bump to save battery.
      strftime(timeSecondsBuffer, sizeof("00"), "%S", tick_time);
      text_layer_set_text(s_time_seconds_layer, timeSecondsBuffer);
    }
    else
    {
      // Clear out the seconds field.
      timeSecondsBuffer[0] = 0;
      text_layer_set_text(s_time_seconds_layer, timeSecondsBuffer);
    }
    
    if (tick_time->tm_hour < 12)
    {
      strncpy(timeAmPmBuffer, "AM", 2);
    }
    else
    {
      strncpy(timeAmPmBuffer, "PM", 2);
    }
    text_layer_set_text(s_time_am_pm_layer, timeAmPmBuffer);
  }
}

static void update_date(struct tm *tick_time)
{
  static char dateBuffer[20];
  static char calendarDayBuffer[14][3];

  // Keep track of the last date we updated to so we only have to
  // update when it changes.
  lastCalendarDateUpdatedTo = tick_time->tm_mday;
  
  // Update the Date
  strftime(dateBuffer, sizeof(dateBuffer), "%A, %b %e", tick_time); 
  text_layer_set_text(s_date_layer, dateBuffer);

  // Update the Calendar
  char dayOfWeekString[2];
  strftime(dayOfWeekString, sizeof(dayOfWeekString), "%w", tick_time);
  int dayOfWeek = atoi(dayOfWeekString);
  if (mondayFirst == TRUE)
  {
    // %w reports Sunday first, subtract by 1 to have Monday first.
    dayOfWeek = dayOfWeek - 1;
  }
  
  // Subtract back to Sunday of this week.
  time_t calendarDate = time(NULL) - (86400 * dayOfWeek);

  if ((mondayFirst == TRUE) && (dayOfWeek == -1))
  {
    // If configured for Monday first and we're on Sunday, then
    // we need to wrap around to the end of the week for it to be
    // displayed properly.
    calendarDate -= (86400 * 7);
    dayOfWeek = 6;
  }

  struct tm *calendarDate_time = localtime(&calendarDate);

  // Update labels for the next 2 weeks.
  for (int dayLoop = 0; dayLoop < 14; dayLoop++)
  {
    // If we're on our current day of the week, then bold it.
    if (dayLoop == dayOfWeek)
    {
      text_layer_set_font(s_calendarDay_layer[dayLoop], fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
    }
    else
    {
      text_layer_set_font(s_calendarDay_layer[dayLoop], fonts_get_system_font(FONT_KEY_GOTHIC_18));
    }
    strftime(calendarDayBuffer[dayLoop], 3, "%e", calendarDate_time);
    text_layer_set_text(s_calendarDay_layer[dayLoop], calendarDayBuffer[dayLoop]);

    calendarDate += 86400;
    calendarDate_time = localtime(&calendarDate);
  }
}

static void update_weather()
{
  static char current_weather_layer_buffer[64];
  static char day1_label_layer_buffer[8];
  static char day1_layer_buffer[64];
  static char day2_label_layer_buffer[8];
  static char day2_layer_buffer[64];

  // Update Current Weather Condition
  char currentTemperatureString[10];
  switch(temperatureUnits)
  {
    case TEMPERATURE_UNITS_F:
      snprintf(currentTemperatureString, sizeof(currentTemperatureString), "%dF",
          (int)getFahrenheitFromCelsius(currentTemperature_c));     
      break;
    case TEMPERATURE_UNITS_C:
      snprintf(currentTemperatureString, sizeof(currentTemperatureString), "%dC",
          currentTemperature_c);     
      break;
  }
  char windDirectionString[3];
  if (currentWindDirection_deg >= 337.5 || currentWindDirection_deg <= 22.5)
  {
    strcpy(windDirectionString, "N");    
  }
  else if (currentWindDirection_deg < 67.5) // currentWindDirection_deg > 22.5 &&
  {
    strcpy(windDirectionString, "NE");    
  }
  else if (currentWindDirection_deg <= 112.5) // currentWindDirection_deg >= 67.5 &&
  {
    strcpy(windDirectionString, "E");
  }
  else if (currentWindDirection_deg < 157.5) // currentWindDirection_deg > 112.5 &&
  {
    strcpy(windDirectionString, "SE");
  }
  else if (currentWindDirection_deg <= 202.5) // currentWindDirection_deg >= 157.5 &&
  {
    strcpy(windDirectionString, "S");
  }
  else if (currentWindDirection_deg < 247.5) // currentWindDirection_deg > 202.5 &&
  {
    strcpy(windDirectionString, "SW");
  }
  else if (currentWindDirection_deg <= 292.5) // currentWindDirection_deg >= 247.5 &&
  {
    strcpy(windDirectionString, "W");
  }
  else if (currentWindDirection_deg < 337.5) // currentWindDirection_deg > 292.5 &&
  {
    strcpy(windDirectionString, "NW");
  }
  snprintf(current_weather_layer_buffer, sizeof(current_weather_layer_buffer), "%s %d%s %s", 
          currentTemperatureString, 
          (int)getPreferedWindSpeed(currentWindSpeed_kts), 
          windDirectionString,
          currentConditions);
  text_layer_set_text(s_weather_current_layer, current_weather_layer_buffer);

  // Update Labels for which Forecast Day
  if (currentDate > 0)
  {
    time_t currentDate_t = currentDate;
    struct tm *currentCalendarTime = localtime(&currentDate_t);
    strftime(day1_label_layer_buffer, sizeof(day1_label_layer_buffer), "%a", currentCalendarTime);
    // Cut off the 3rd letter to show a 2 character day abbreviation. Just as clear and saves space.
    day1_label_layer_buffer[2] = 0;
    text_layer_set_text(s_weather_label1_layer, day1_label_layer_buffer);

    time_t forecastDate_t = currentDate_t + 86400;
    struct tm *forecastCalendarTime = localtime(&forecastDate_t);
    strftime(day2_label_layer_buffer, sizeof(day2_label_layer_buffer), "%a", forecastCalendarTime);
    // Cut off the 3rd letter to show a 2 character day abbreviation. Just as clear and saves space.
    day2_label_layer_buffer[2] = 0;
    text_layer_set_text(s_weather_label2_layer, day2_label_layer_buffer);
  }

  // Update Today's Weather Condition
  switch(temperatureUnits)
  {
    case TEMPERATURE_UNITS_F:
      snprintf(day1_layer_buffer, sizeof(day1_layer_buffer), "%d/%dF %s", 
              (int)getFahrenheitFromCelsius(currentLowTemperature_c), (int)getFahrenheitFromCelsius(currentHighTemperature_c),
              currentDayForecastConditions);
      break;
    case TEMPERATURE_UNITS_C:
      snprintf(day1_layer_buffer, sizeof(day1_layer_buffer), "%d/%dC %s", 
              currentLowTemperature_c, currentHighTemperature_c,
              currentDayForecastConditions);
      break;
  }
  text_layer_set_text(s_weather_forecast1_layer, day1_layer_buffer);

  // Update Tomorrow's Weather Condition
  switch(temperatureUnits)
  {
    case TEMPERATURE_UNITS_F:
      snprintf(day2_layer_buffer, sizeof(day2_layer_buffer), "%d/%dF %s", 
               (int)getFahrenheitFromCelsius(forecastLowTemperature_c), (int)getFahrenheitFromCelsius(forecastHighTemperature_c),
               forecastConditions);
      break;
    case TEMPERATURE_UNITS_C:
      snprintf(day2_layer_buffer, sizeof(day2_layer_buffer), "%d/%dC %s", 
               forecastLowTemperature_c, forecastHighTemperature_c,
               forecastConditions);
      break;
  }
  text_layer_set_text(s_weather_forecast2_layer, day2_layer_buffer);
}

static void update_battery_state(BatteryChargeState charge_state)
{
  static char batteryBuffer[8];
  uint8_t raw_percent = charge_state.charge_percent;
 
  snprintf(batteryBuffer, sizeof(batteryBuffer), " %i%%", (int)raw_percent);
  if (charge_state.is_charging)
  {
    strcat(batteryBuffer, "+");
  }
  //else if (charge_state.is_plugged)
  //{
  //  strcat(batteryBuffer, "*");
  //}
  text_layer_set_text(s_battery_layer, batteryBuffer);
}

static void update_bluetooth_state(bool bluetoothConnected)
{
  connectedToBluetooth = bluetoothConnected;
  update_link_label();
}

static void request_weather()
{
  timeOfLastDataRequest = time(NULL);
  
  // Begin dictionary
  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);

  // Add a key-value pair
  dict_write_uint8(iter, 0, 0);

  // Send the message!
  app_message_outbox_send();
}

static void main_window_load(Window *window)
{
  // Recover saved weather conditions and configuration options.
  // We will re-query weather, but this is important if we don't
  // have a data connection when the app reopens.
  if (persist_exists(STORAGE_KEY_CURRENT_TEMPERATURE_C))
  {
    currentTemperature_c = persist_read_int(STORAGE_KEY_CURRENT_TEMPERATURE_C);
  }
  else
  {
    currentTemperature_c = 0;
  }

  if (persist_exists(STORAGE_KEY_CURRENT_CONDITIONS))
  {
    persist_read_string(STORAGE_KEY_CURRENT_CONDITIONS, currentConditions, 32);
  }
  else
  {
    currentConditions[0] = 0;
  }
  
  if (persist_exists(STORAGE_KEY_CURRENT_DAY_FORECAST_CONDITIONS))
  {
    persist_read_string(STORAGE_KEY_CURRENT_DAY_FORECAST_CONDITIONS, currentDayForecastConditions, 32);
  }
  else
  {
    currentDayForecastConditions[0] = 0;
  }
  
  if (persist_exists(STORAGE_KEY_CURRENT_LOW_C))
  {
    currentLowTemperature_c = persist_read_int(STORAGE_KEY_CURRENT_LOW_C);
  }
  else
  {
    currentLowTemperature_c = 0;
  }
  
  if (persist_exists(STORAGE_KEY_CURRENT_HIGH_C))
  {
    currentHighTemperature_c = persist_read_int(STORAGE_KEY_CURRENT_HIGH_C);
  }
  else
  {
    currentHighTemperature_c = 0;
  }
  
  if (persist_exists(STORAGE_KEY_CURRENT_WIND_DIR_DEG))
  {
    currentWindDirection_deg = persist_read_int(STORAGE_KEY_CURRENT_WIND_DIR_DEG);
  }
  else
  {
    currentWindDirection_deg = 0;
  }
  
  if (persist_exists(STORAGE_KEY_CURRENT_WIND_SPD_KTS))
  {
    currentWindSpeed_kts = persist_read_int(STORAGE_KEY_CURRENT_WIND_SPD_KTS);
  }
  else
  {
    currentWindSpeed_kts = 0;
  }
  
  if (persist_exists(STORAGE_KEY_CURRENT_DAY))
  {
    currentDate = persist_read_int(STORAGE_KEY_CURRENT_DAY);
  }
  else
  {
    currentDate = 0;
  }
  
  if (persist_exists(STORAGE_KEY_FORECAST_LOW_C))
  {
    forecastLowTemperature_c = persist_read_int(STORAGE_KEY_FORECAST_LOW_C);
  }
  else
  {
    forecastLowTemperature_c = 0;
  }
  
  if (persist_exists(STORAGE_KEY_FORECAST_HIGH_C))
  {
    forecastHighTemperature_c = persist_read_int(STORAGE_KEY_FORECAST_HIGH_C);
  }
  else
  {
    forecastHighTemperature_c = 0;
  }
  
  if (persist_exists(STORAGE_KEY_FORECAST_CONDITIONS))
  {
    persist_read_string(STORAGE_KEY_FORECAST_CONDITIONS, forecastConditions, 32);
  }
  else
  {
    forecastConditions[0] = 0;
  }

  if (persist_exists(STORAGE_KEY_TEMPERATURE_UNITS))
  {
    temperatureUnits = persist_read_int(STORAGE_KEY_TEMPERATURE_UNITS); 
  }
  else
  {
    temperatureUnits = TEMPERATURE_UNITS_F;
  }
  
  if (persist_exists(STORAGE_KEY_WINDSPEED_UNITS))
  {
    windSpeedUnits = persist_read_int(STORAGE_KEY_WINDSPEED_UNITS);
  }
  else
  {
    windSpeedUnits = WINDSPEED_UNITS_KNOTS;
  }
  
  if (persist_exists(STORAGE_KEY_WEEKNUMBER_ENABLED))
  {
    weekNumberEnabled = persist_read_int(STORAGE_KEY_WEEKNUMBER_ENABLED);
  }
  else
  {
    weekNumberEnabled = FALSE;
  }
  
  if (persist_exists(STORAGE_KEY_MONDAY_FIRST))
  {
    mondayFirst = persist_read_int(STORAGE_KEY_MONDAY_FIRST);
  }
  else
  {
    mondayFirst = FALSE;
  }

  // 144 wide
  // GRect: x position, y position, x size, y size
  
  // Create Battery TextLayer
  s_battery_layer = text_layer_create(GRect(0, 0, 50, 16));
  text_layer_set_background_color(s_battery_layer, GColorBlack);
  text_layer_set_text_color(s_battery_layer, GColorWhite);
  text_layer_set_font(s_battery_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD));
  text_layer_set_text_alignment(s_battery_layer, GTextAlignmentLeft);
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_battery_layer));
    
  // Create Link Status TextLayer
  s_linkStatus_layer = text_layer_create(GRect(50, 0, 94, 16));
  text_layer_set_background_color(s_linkStatus_layer, GColorBlack);
  text_layer_set_text_color(s_linkStatus_layer, GColorWhite);
  text_layer_set_font(s_linkStatus_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD));
  text_layer_set_text_alignment(s_linkStatus_layer, GTextAlignmentLeft);
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_linkStatus_layer));
  
  // Create Time TextLayer
  if (clock_is_24h_style())
  {
    s_time_layer = text_layer_create(GRect(0, 10, 125, 50));
  }
  else
  {
    s_time_layer = text_layer_create(GRect(0, 10, 118, 50));
  }
  text_layer_set_background_color(s_time_layer, GColorClear);
  text_layer_set_text_color(s_time_layer, GColorBlack);
  text_layer_set_font(s_time_layer, fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD));
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentRight);
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_time_layer));
  
  // Create AM/PM TextLayer
  s_time_am_pm_layer = text_layer_create(GRect(120, 16, 24, 18));
  text_layer_set_background_color(s_time_am_pm_layer, GColorClear);
  text_layer_set_text_color(s_time_am_pm_layer, GColorBlack);
  text_layer_set_font(s_time_am_pm_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_alignment(s_time_am_pm_layer, GTextAlignmentLeft);
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_time_am_pm_layer));
  
  // Create Seconds TextLayer
  s_time_seconds_layer = text_layer_create(GRect(120, 33, 24, 18));
  text_layer_set_background_color(s_time_seconds_layer, GColorClear);
  text_layer_set_text_color(s_time_seconds_layer, GColorBlack);
  text_layer_set_font(s_time_seconds_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_alignment(s_time_seconds_layer, GTextAlignmentLeft);
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_time_seconds_layer));
  
  // Create Date TextLayer
  s_date_layer = text_layer_create(GRect(0, 48, 144, 28)); // 0, 43, 144, 28
  text_layer_set_background_color(s_date_layer, GColorClear);
  text_layer_set_text_color(s_date_layer, GColorBlack);
  text_layer_set_font(s_date_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(s_date_layer, GTextAlignmentCenter);
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_date_layer));
  
  // Create Current Weather Layer
  s_weather_current_layer = text_layer_create(GRect(2, 73, 142, 20)); // 0, 65, 144, 20
  text_layer_set_background_color(s_weather_current_layer, GColorClear);
  text_layer_set_text_color(s_weather_current_layer, GColorBlack);
  text_layer_set_font(s_weather_current_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_text_alignment(s_weather_current_layer, GTextAlignmentLeft);
  text_layer_set_text(s_weather_current_layer, "");
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_weather_current_layer));
  
  // Create Today's Forecast Label Layer
  s_weather_label1_layer = text_layer_create(GRect(2, 90, 25, 20)); // 0, 113, 35, 20
  text_layer_set_background_color(s_weather_label1_layer, GColorClear);
  text_layer_set_text_color(s_weather_label1_layer, GColorBlack);
  text_layer_set_font(s_weather_label1_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_alignment(s_weather_label1_layer, GTextAlignmentLeft);
  text_layer_set_text(s_weather_label1_layer, "");
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_weather_label1_layer));
 
  // Create Today's Forecast Layer
  s_weather_forecast1_layer = text_layer_create(GRect(25, 90, 119, 20)); // 0, 81, 144, 20
  text_layer_set_background_color(s_weather_forecast1_layer, GColorClear);
  text_layer_set_text_color(s_weather_forecast1_layer, GColorBlack);
  text_layer_set_font(s_weather_forecast1_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_text_alignment(s_weather_forecast1_layer, GTextAlignmentLeft);
  text_layer_set_text(s_weather_forecast1_layer, "");
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_weather_forecast1_layer));

  // Create Tomorrow's Forecast Label Layer
  s_weather_label2_layer = text_layer_create(GRect(2, 107, 25, 20)); // 0, 113, 35, 20
  text_layer_set_background_color(s_weather_label2_layer, GColorClear);
  text_layer_set_text_color(s_weather_label2_layer, GColorBlack);
  text_layer_set_font(s_weather_label2_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_alignment(s_weather_label2_layer, GTextAlignmentLeft);
  text_layer_set_text(s_weather_label2_layer, "");
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_weather_label2_layer));

  // Create Tomorrow's Forecast Layer
  s_weather_forecast2_layer = text_layer_create(GRect(25, 107, 119, 20)); // 35, 113, 109, 20
  text_layer_set_background_color(s_weather_forecast2_layer, GColorClear);
  text_layer_set_text_color(s_weather_forecast2_layer, GColorBlack);
  text_layer_set_font(s_weather_forecast2_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_text_alignment(s_weather_forecast2_layer, GTextAlignmentLeft);
  text_layer_set_text(s_weather_forecast2_layer, "");
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_weather_forecast2_layer));
  
  // Create Calendar Layers
  int calendarX = 0;
  int calendarY = 132;
  for (int dayLoop = 0; dayLoop < 14; dayLoop++)
  {
    if (mondayFirst)
    {
      // Monday through Sunday Layout
      // Have a gap between Friday/Saturday
      if (dayLoop == 5 || dayLoop == 12)
      {
        calendarX = 104;
      }
      else if (dayLoop == 6 || dayLoop == 13)
      {
        calendarX = 124;
      }
      else if (dayLoop < 5)
      {
        calendarX = dayLoop * 20;
      }
      else
      {
        calendarX = (dayLoop - 7) * 20;
      }
    }
    else
    {
      // Sunday through Saturday Layout
      // Have a gap between Sunday/Monday and Friday/Saturday
      if (dayLoop == 0 || dayLoop == 7)
      {
        calendarX = 0;
      }
      else if (dayLoop == 6 || dayLoop == 13)
      {
        calendarX = 124;
      }
      else if (dayLoop < 7)
      {
        calendarX = 2 + dayLoop * 20;
      }
      else
      {
        calendarX = 2 + (dayLoop - 7) * 20;
      }
    }

    if (dayLoop < 7)
    {
      calendarY = 132;
    }
    else
    {
      calendarY = 150;
    }
    
    s_calendarDay_layer[dayLoop] = text_layer_create(GRect(calendarX, calendarY, 20, 20));
    text_layer_set_background_color(s_calendarDay_layer[dayLoop], GColorBlack);
    text_layer_set_text_color(s_calendarDay_layer[dayLoop], GColorWhite);
    text_layer_set_font(s_calendarDay_layer[dayLoop], fonts_get_system_font(FONT_KEY_GOTHIC_18)); // FONT_KEY_GOTHIC_18
    text_layer_set_text_alignment(s_calendarDay_layer[dayLoop], GTextAlignmentCenter);
    text_layer_set_text(s_calendarDay_layer[dayLoop], "-");
    layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_calendarDay_layer[dayLoop]));
    calendarX += 22;
  }
  
  // Do an immediate update for all the layers.
  time_t currentTime = time(NULL);
  struct tm *tick_time = localtime(&currentTime);
  update_time(tick_time);
  update_date(tick_time);
  update_battery_state(battery_state_service_peek());
  update_bluetooth_state(bluetooth_connection_service_peek());
  update_weather();
  
  // Cannot do a request_weather here, crashes the Pebble Watch.
}

static void main_window_unload(Window *window)
{
  // Record current weather conditions so they can be restored
  // immediately when the app reopens. Very important if we don't
  // have a data connection when the app reopens.
  persist_write_int(STORAGE_KEY_CURRENT_TEMPERATURE_C, currentTemperature_c);
  persist_write_string(STORAGE_KEY_CURRENT_CONDITIONS, currentConditions);
  persist_write_string(STORAGE_KEY_CURRENT_DAY_FORECAST_CONDITIONS, currentDayForecastConditions);
  persist_write_int(STORAGE_KEY_CURRENT_LOW_C, currentLowTemperature_c);
  persist_write_int(STORAGE_KEY_CURRENT_HIGH_C, currentHighTemperature_c);
  persist_write_int(STORAGE_KEY_CURRENT_WIND_DIR_DEG, currentWindDirection_deg);
  persist_write_int(STORAGE_KEY_CURRENT_WIND_SPD_KTS, currentWindSpeed_kts);
  persist_write_int(STORAGE_KEY_CURRENT_DAY, currentDate);
  persist_write_int(STORAGE_KEY_FORECAST_LOW_C, forecastLowTemperature_c);
  persist_write_int(STORAGE_KEY_FORECAST_HIGH_C, forecastHighTemperature_c);
  persist_write_string(STORAGE_KEY_FORECAST_CONDITIONS, forecastConditions);
  
  // Record configuration settings.
  persist_write_int(STORAGE_KEY_TEMPERATURE_UNITS, temperatureUnits);
  persist_write_int(STORAGE_KEY_WINDSPEED_UNITS, windSpeedUnits);
  persist_write_int(STORAGE_KEY_WEEKNUMBER_ENABLED, weekNumberEnabled);
  persist_write_int(STORAGE_KEY_MONDAY_FIRST, mondayFirst);

  // Destroy Layers
  text_layer_destroy(s_battery_layer);
  text_layer_destroy(s_linkStatus_layer);
  text_layer_destroy(s_time_layer);
  text_layer_destroy(s_time_am_pm_layer);
  text_layer_destroy(s_time_seconds_layer);
  text_layer_destroy(s_date_layer);
  text_layer_destroy(s_weather_current_layer);
  text_layer_destroy(s_weather_label1_layer);
  text_layer_destroy(s_weather_forecast1_layer);
  text_layer_destroy(s_weather_label2_layer);
  text_layer_destroy(s_weather_forecast2_layer);
  for (int dayLoop = 0; dayLoop < 14; dayLoop++)
  {
    text_layer_destroy(s_calendarDay_layer[dayLoop]);
  }
}

// tick_handler may be called once per second or once per minute
// depending on if the watch has been tapped and if we're showing
// the 12 HR clock.
static void tick_handler(struct tm *tick_time, TimeUnits units_changed)
{
  if (isShowingSeconds)
  {
    if (difftime(time(NULL), timeOfLastTap) > NUMBER_OF_SECONDS_TO_SHOW_SECONDS_AFTER_TAP)
    {
      // We are showing seconds, but it has been more than 5
      // minutes since our wrist was tapped. To save processing,
      // stop showing seconds (revert back to one minute updates).
      isShowingSeconds = false;
      tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
    }
  }

  // Update the time.
  update_time(tick_time);

  // Update the date only when the day changes.
  if (lastCalendarDateUpdatedTo != tick_time->tm_mday)
  {
    update_date(tick_time);

    // Also request the weather again so we get the next day's forecast.
    request_weather();
  }

  // Update the weather every 30 minutes.
  if (difftime(time(NULL), timeOfLastDataRequest) > NUMBER_OF_SECONDS_BETWEEN_WEATHER_UPDATES)
  {
    request_weather();
  }

  // If we haven't received our weather request within 1 minute,
  // assume that we have lost our data connection.
  if (connectedToData && 
      (difftime(timeOfLastDataResponse, timeOfLastDataRequest) > NUMBER_OF_SECONDS_UNTIL_DATA_CONSIDERED_LOST))
  {
    connectedToData = false;
    update_link_label();
  }
}

static void inbox_received_callback(DictionaryIterator *iterator, void *context)
{
  // We received data, update the time stamp / link label.
  timeOfLastDataResponse = time(NULL);
  connectedToData = true;
  update_link_label();

  // Read first item
  Tuple *t = dict_read_first(iterator);

  int day1Date = 0;
  int day2Date = 0;
  //int day3Date = 0;
  int day1LowTemperature_c = 0;
  int day2LowTemperature_c = 0;
  int day3LowTemperature_c = 0;
  int day1HighTemperature_c = 0;
  int day2HighTemperature_c = 0;
  int day3HighTemperature_c = 0;
  char day1Conditions[32];
  char day2Conditions[32];
  char day3Conditions[32];

  // For all items
  while(t != NULL)
  {
    // Which key was received?
    switch(t->key)
    {
      case KEY_TEMPERATURE:
        currentTemperature_c = t->value->int32;
        break;
//      case KEY_CONDITIONS:
//        // Current conditions (abbreviated).
//        snprintf(conditions_buffer, sizeof(conditions_buffer), "%s", t->value->cstring);
//        break;
//      case KEY_TEMP_MIN:
//        // Not reliably low temperature.
//        snprintf(temperatureMin_buffer, sizeof(temperatureMin_buffer), "%d", (int)getFahrenheitFromCelsius((float)t->value->int32));
//        break;
//      case KEY_TEMP_MAX:
//        // Not reliably high temperature.
//        snprintf(temperatureMax_buffer, sizeof(temperatureMax_buffer), "%dF", (int)getFahrenheitFromCelsius((float)t->value->int32));
//        break;
      case KEY_WIND_SPEED:
        // Reported in meters per second, convert to knots.
        currentWindSpeed_kts = (int)getKnotsFromMetersPerSecond((float)t->value->int32);
        break;
      case KEY_WIND_DIRECTION:
        currentWindDirection_deg = t->value->int32;
        break;
//      case KEY_HUMIDITY:
//        snprintf(humidity_buffer, sizeof(humidity_buffer), "%d%%", (int)t->value->int32);
//        break;
      case KEY_DESCRIPTION:
        // Similar to conditions, but far more descriptive.
        strncpy(currentConditions, t->value->cstring, 32);
        break;
      case KEY_DAY1_TIME:
        // Usually today's date, but in the morning it's yesterday's!
        day1Date = t->value->int32;
        break;
      case KEY_DAY1_CONDITIONS:
        // Today's condition (abbreviated).
        //snprintf(day1_conditions_buffer, sizeof(day1_conditions_buffer), "%s", t->value->cstring);
        strncpy(day1Conditions, t->value->cstring, 32);
        break;
      case KEY_DAY1_TEMP_MIN:
        //currentLowTemperature_c = t->value->int32;
        day1LowTemperature_c = t->value->int32;
        break;
      case KEY_DAY1_TEMP_MAX:
        //currentHighTemperature_c = t->value->int32;
        day1HighTemperature_c = t->value->int32;
        break;
      case KEY_DAY2_TIME:
        // Usually it's tomorrow's date, but in the morning it's today's!.
        //forecastDate = t->value->int32;
        day2Date = t->value->int32;
        break;
      case KEY_DAY2_CONDITIONS:
        // Forecast condition (abbreviated).
        //strncpy(forecastConditions, t->value->cstring, 32);
        strncpy(day2Conditions, t->value->cstring, 32);
        break;
      case KEY_DAY2_TEMP_MIN:
        //forecastLowTemperature_c = t->value->int32;
        day2LowTemperature_c = t->value->int32;
        break;
      case KEY_DAY2_TEMP_MAX:
        //forecastHighTemperature_c = t->value->int32;
        day2HighTemperature_c = t->value->int32;
        break;
      case KEY_DAY3_TIME:
        // Usually it's in two days, but in the morning it's tomorrow's!
        //day3Date = t->value->int32;
        break;
      case KEY_DAY3_CONDITIONS:
        // Forecast condition (abbreviated).
        strncpy(day3Conditions, t->value->cstring, 32);
        break;
      case KEY_DAY3_TEMP_MIN:
        //day3LowTemperature_c = t->value->int32;
        day3LowTemperature_c = t->value->int32;
        break;
      case KEY_DAY3_TEMP_MAX:
        day3HighTemperature_c = t->value->int32;
        break;
      case CONFIG_KEY_TEMPERATURE_UNITS:
        if (strcmp(t->value->cstring, "F") == 0)
        {
          temperatureUnits = TEMPERATURE_UNITS_F;
        }
        else if (strcmp(t->value->cstring, "C") == 0)
        {
          temperatureUnits = TEMPERATURE_UNITS_C;
        }
        break;
      case CONFIG_KEY_WINDSPEED_UNITS:
        if (strcmp(t->value->cstring, "KNOTS") == 0)
        {
          windSpeedUnits = WINDSPEED_UNITS_KNOTS;
        }
        else if (strcmp(t->value->cstring, "MPH") == 0)
        {
          windSpeedUnits = WINDSPEED_UNITS_MPH;
        }
        else if (strcmp(t->value->cstring, "KPH") == 0)
        {
          windSpeedUnits = WINDSPEED_UNITS_KPH;
        }
        break;
      case CONFIG_KEY_WEEKNUMBER_ENABLED:
        if (strcmp(t->value->cstring, "DISABLED") == 0)
        {
          weekNumberEnabled = FALSE;
        }
        else if (strcmp(t->value->cstring, "ENABLED") == 0)
        {
          weekNumberEnabled = TRUE;
        }
        break;
      case CONFIG_KEY_MONDAY_FIRST:
        if (strcmp(t->value->cstring, "DISABLED") == 0)
        {
          mondayFirst = FALSE;
        }
        else if (strcmp(t->value->cstring, "ENABLED") == 0)
        {
          mondayFirst = TRUE;
        }
        break;
      default:
        APP_LOG(APP_LOG_LEVEL_ERROR, "Key %d not recognized!", (int)t->key);
        break;
    }

    // Look for next item
    t = dict_read_next(iterator);
  }

  if (day1Date > 0)
  {
    // Forecast Response
    time_t currentTime = time(NULL);
    struct tm *currentCalendarTime = localtime(&currentTime);
    int dayOfMonthCurrent = currentCalendarTime->tm_mday;
    
    time_t day1Date_t = day1Date;
    struct tm *day1CalendarTime = localtime(&day1Date_t);
    int dayOfMonth1 = day1CalendarTime->tm_mday;
    
    time_t day2Date_t = day2Date;
    struct tm *day2CalendarTime = localtime(&day2Date_t);
    int dayOfMonth2 = day2CalendarTime->tm_mday;
    
    if (dayOfMonthCurrent == dayOfMonth1)
    {
      // Day 1 is Today's Date
      currentDate = day1Date;
      
      strncpy(currentDayForecastConditions, day1Conditions, 32);
      currentLowTemperature_c = day1LowTemperature_c;
      currentHighTemperature_c = day1HighTemperature_c;
      
      // So Day 2 will be the forecast.
      strncpy(forecastConditions, day2Conditions, 32);
      forecastLowTemperature_c = day2LowTemperature_c;
      forecastHighTemperature_c = day2HighTemperature_c;
    }
    else if (dayOfMonthCurrent == dayOfMonth2)
    {
      // Day 2 is Today's Date 
      currentDate = day2Date;
      
      strncpy(currentDayForecastConditions, day2Conditions, 32);
      currentLowTemperature_c = day2LowTemperature_c;
      currentHighTemperature_c = day2HighTemperature_c;

      // So Day 3 will be the forecast.
      strncpy(forecastConditions, day3Conditions, 32);
      forecastLowTemperature_c = day3LowTemperature_c;
      forecastHighTemperature_c = day3HighTemperature_c;
    }
  } // (day1Date > 0)

  update_weather();
}

static void inbox_dropped_callback(AppMessageResult reason, void *context)
{
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped!");  
}

static void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context)
{
  APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed!");
}

static void outbox_sent_callback(DictionaryIterator *iterator, void *context)
{
  APP_LOG(APP_LOG_LEVEL_INFO, "Outbox send success!");
}

static void accel_tap_handler(AccelAxisType axis, int32_t direction)
{
  // In testing, showing seconds all the time resulted in a battery
  // life of barely 3 days. Not showing seconds (updating only once
  // per minute) resulted in a battery life of 5 days.
  // As a compromise, if our watch gets a tap then we'll show seconds
  // for 3 minutes. In the middle of the night we won't waste
  // processing on seconds but when we need seconds for exact timing
  // it's just a flick of a wrist to see them.
  
  timeOfLastTap = time(NULL);
  
  // The 24 HR clock never shows seconds.
  if ((weekNumberEnabled == FALSE) && !clock_is_24h_style())
  {
    if (!isShowingSeconds)
    {
      // We aren't showing seconds, let's show them and switch
      // to the second_unit timer subscription.
      isShowingSeconds = true;
      
      // Immediatley update the time so our tap looks very responsive.
      struct tm *tick_time = localtime(&timeOfLastTap);
      update_time(tick_time);

      // Resubsrcibe to the tick timer at every second.
      tick_timer_service_subscribe(SECOND_UNIT, tick_handler);
    }
  }
}

static void init(void)
{
  // Create main Window element and assign to pointer
  s_main_window = window_create();

  // Set handlers to manage the elements inside the Window
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
    });

  // Show the window on the watch, with animated=true
  window_stack_push(s_main_window, true);

  // 24-Hour clock doesn't show seconds, so save processing and
  // only refresh once a minute.
  if (clock_is_24h_style())
  {
    tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  }
  else
  {
    tick_timer_service_subscribe(SECOND_UNIT, tick_handler);
  }

  battery_state_service_subscribe(update_battery_state);
  bluetooth_connection_service_subscribe(update_bluetooth_state);
  accel_tap_service_subscribe(accel_tap_handler);
  
  app_message_register_inbox_received(inbox_received_callback);
  app_message_register_inbox_dropped(inbox_dropped_callback);
  app_message_register_outbox_failed(outbox_failed_callback);
  app_message_register_outbox_sent(outbox_sent_callback);
  
  app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());
}

void deinit(void)
{
  tick_timer_service_unsubscribe();
  battery_state_service_unsubscribe();
  bluetooth_connection_service_unsubscribe();
  
  window_destroy(s_main_window);
}

int main(void)
{
  init();
  app_event_loop();
  deinit();
}
