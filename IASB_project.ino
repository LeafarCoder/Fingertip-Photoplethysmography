#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>

// Test buttons
const int buttonDownPin = 8;
const int buttonUpPin = 9;
const int buttonOkPin = 10;
int buttonDownStates[2] = {0, 0};
int buttonUpStates[2] = {0, 0};
int buttonOkStates[2] = {0, 0};
boolean buttonDownActivatedQ = false;
boolean buttonUpActivatedQ = false;
boolean buttonOkActivatedQ = false;

// Buzzer
const int buzzerPin = 2;
boolean playBuzzerQ = true;         // should we play the buzzer?
int buzzerFreq = 640;               // buzzer frequency (Hertz)
int buzzerBipDuration = 200;        // buzzer bip duration (ms)

// PPG
int ppg_x_lim_px[2] = {0, 83};      // x limits (in the pixel Monitor coordinates) for the display of the PPG signal
int ppg_y_lim_px[2] = {9, 47};      // y limits (in the pixel Monitor coordinates) for the display of the PPG signal
const int ppg_sample_size = 20;                // number of samples from PPG kept stored
int last_ppg_val;                               // last PPG value obtained from analog input
double Fs = 200;                                // sampling frequency (controls the Arduino TIMER)
int ppg[ppg_sample_size];                       // stores the last PPG last_ppg_values recorded
long ppg_diff[ppg_sample_size];                  // stores de difference between adjacent PPG last_ppg_values (derivative signal)
int ppg_t_count;
boolean ppg_updated;                            // tells if update on the 'last_ppg_val' is needed (turned on when the TIMER is triggered and turned off once the value is updated)
int curr_screen_x;                              // screen time. swipes the screen while displaying ppg
int scaleFactor_PPG;                            // Digitally scale the signal
// for peak detection
boolean peakDetectedQ;                          // QRS complex peak was detected
long lastTimePeakDetected;                       // Last time a peak was detected (uses millis())
int minTimeSpanBetweenPeaks = 300;              // Minimum acceptable interval between QRS peaks (ms)
int dynamicThreshold;                           // Dynamic threshold for peak detection
const int averageOverNPeaks = 3;                // Size of 'lastPeaksIntensity' vector
int lastPeaksIntensity[averageOverNPeaks];      // Keeps the intensity of the 'averageOverNPeaks' last peaks
long lastPeaksTime[averageOverNPeaks];           // Used to calculate an average BPM (heart rate)
long averagedBPM;                                // current BPM value (averaged over the last 'averageOverNPeaks' intervals)
int averageOfPeakIntensities;                   // This is the average of 'lastPeaksIntensity'
double nextPeakDetectionThreshold = 0.6;        // To detect some peak its intensity must be higher than (nextPeakDetectionThreshold * averageOfPeakIntensities)
int lastMaxPeakTime;                            // Time at which the last peak was measured
boolean lookForTrueLocalMax = false;            // Set to true after a peak is detected to locally search for the true maximum
int timeSpanForLocalMaxSearch = 100;            // Time Window for searching the true maximum local value (ms)
int trueLocalMax;                               // True value for the local max (updated as window moves along)
boolean isSaturated = true;                     // Checks if the signal is saturated
double saturatedThreshold = 0.05;               // Threshold (in Volts) for the saturation
long lastTimeSaturated;                          // Time record of last time of saturation  
long saturationTimeThreshold = 1500;             // Minimum time span before acepting as not saturated (ms)
boolean saturationStateChanged = false;         // Is true if isSaturation just changed, otherwise is false (used for clear the screen before presenting the signal)

// Amplification (on graph)
double graph_amp;
int graph_min;
int graph_max;
int search_graph_min;
int search_graph_max;

// Timers
const uint16_t t1_load = 0;
const uint16_t t1_comp = (16e6 / (256 * Fs));

// GUI variables
int GUIpage;                                    // current page to be displayed in the Monitor
int GUIoption;                                  // current option selected in the Monitor
boolean optionSelected;                         // for some menus, to toggle selection state of the Menu Options
String mainMenuOptions[3] = {"PPG plot", "Settings", "Credits"};
String soundMenuOptions[4] = {"Bips", "Frequency", "Duration", "Return"};
String settingsMenuOptions[4] = {"Peak det.", "Screen", "Sound", "Return"};
String screenMenuOptions[3] = {"Light", "Contrast", "Return"};
String developers[2] = {"Jose Correia", "Andre Miranda"};

// Monitor object
// Hardware SPI (faster, but must use certain hardware pins):
// SCK is LCD serial clock (SCLK) - this is pin 13 on Arduino Uno
// MOSI is LCD DIN - this is pin 11 on an Arduino Uno
// pin 5 - Data/Command select (D/C)
// pin 4 - LCD chip select (CS)
// pin 3 - LCD reset (RST)
Adafruit_PCD8544 display = Adafruit_PCD8544(7, 6, 5, 4, 3);

// Screen
int contrast = 60;
int backLightPin = 11;
boolean backLightQ = false;

// ****************************************************************************************************************************
// ****************************************************    SETUP    ***********************************************************
// ****************************************************************************************************************************

void setup() {

  TCCR1A = 0;
  TCCR1B |= (1<<CS12);
  TCCR1B &= ~(1<<CS11);
  TCCR1B &= ~(1<<CS10);

  TCNT1 = t1_load;
  OCR1A = t1_comp;

  TIMSK1 = (1<<OCIE1A);

  sei();

  Serial.begin(9600);

  // Pin initializing 
  pinMode(backLightPin, OUTPUT);
  pinMode(buttonDownPin, INPUT);
  pinMode(buttonUpPin, INPUT);
  pinMode(buttonOkPin, INPUT);
  pinMode(buzzerPin, OUTPUT);
  
  // Display initializing
  display.begin();
  display.setContrast(contrast); //Set display contrast
  display.clearDisplay(); 
  display.display();

  ppg_t_count = 0;
  ppg_updated = true;
  curr_screen_x = ppg_x_lim_px[0];
  scaleFactor_PPG = 1;
  lastTimePeakDetected = millis();

  // GUI control
  GUIpage = 0;  // {0,1,2,3,4,5,6} <---> {mainMenu, plotPPG, generalSettings, screenSettings, soundSettings, credits, peak detections}
  GUIoption = 0;
  optionSelected = false;
}



void loop() {

  // PRINT VALUES
  int print_idx = 0;
  switch(print_idx){
    case 0: // PPG_diff & Threshold
      Serial.print(ppg_diff[ppg_sample_size- 1]);
      Serial.print(" ");
      Serial.println(dynamicThreshold);
      break;

    case 1: // PPG_diff & Threshold & max_local
      Serial.print(ppg_diff[ppg_sample_size- 1]);
      Serial.print(" ");
      Serial.print(dynamicThreshold);
      Serial.print(" ");
      Serial.println(trueLocalMax);
      break;
      
    case 2: // PPG_diff & PPG
      Serial.print(ppg_diff[ppg_sample_size- 1] / 100);
      Serial.print(" ");
      Serial.println(ppg[ppg_sample_size- 1]);
      break;

    case 3: // BPM
      Serial.println(averagedBPM);
      break;

    case 4:
      Serial.println(mapfloat(ppg[ppg_sample_size-1], 0, 1023, 0, 5));
      break;

    case 5:
      Serial.println(isSaturated);
      break;
  }

  checkControler();
  
  // Check if PPG vector must be updated (if timer has updated last_ppg_value)
  samplePPGIfNeeded();

  // saturation detection
  detect_saturation();
  
  // peak detection algorithm
  peak_detection();
  
  // draws GUI in the display
  GUI_draw();

  resetControler();
}


// ---------------------------------------------- GUI FUNCTIONS --------------------------------------------------------

void GUI_draw(){

  // check the current page and execute
  switch(GUIpage){
    case 0:       // Main menu
      GUI_mainMenu();
      break;
      
    case 1:       // PPG plot
      GUI_PPGplot();
      break;
      
    case 2:       // General settings (related with peak-detection algorithms, RR-interlast_ppg_val average, etc)
      GUI_generalSettings();
      break;

    case 3:       // Screen settings
      GUI_screenSettings();
      break;
      
    case 4:       // Sound settings
      GUI_soundSettings();
      break;

    case 5:       // Credits
      GUI_credits();
      break;

    case 6:
      GUI_peakDet();
      break;
  }
  
}

void GUI_mainMenu(){
  int nOptions = sizeof(mainMenuOptions)/sizeof(String);
  
  if(buttonDownActivatedQ){
    changeGUIOptionValue(1, 0, nOptions - 1);
  }
  if(buttonUpActivatedQ){
    changeGUIOptionValue(-1, 0, nOptions - 1);
  }
  if(buttonOkActivatedQ){
    // {mainMenu, plotPPG, generalSettings, screenSettings, soundSettings, credits, }
    // {"PPG plot", "Settings", "Credits"};
    
    switch(GUIoption){
      case 0:
        GUIpage = 1;
      break;

      case 1:
        GUIpage = 2;
      break;

      case 2:
        GUIpage = 5;
      break;
    }

    display.clearDisplay();
    resetMenuOptions();
    return;
  }
  display.setTextColor(BLACK, WHITE);
  display.setCursor(15, 0);
  display.print("MAIN MENU");
  display.drawFastHLine(0, 8, 84, BLACK);

  for(int i = 0; i < nOptions; i++){
    if(i == GUIoption){
      display.setTextColor(WHITE, BLACK);
    }else{
      display.setTextColor(BLACK, WHITE);
    }
    // write option centered
    int y = i*(47 - 8)/nOptions + 12;
    display.setCursor(int(44 - 6.*((2+mainMenuOptions[i].length())/2.)) , y);
    display.print(" "+mainMenuOptions[i]+" ");
  }

  display.display();
}

void GUI_PPGplot(){

  if(buttonOkActivatedQ){
    GUIpage = 0;
    display.clearDisplay();
    return;
  }

  if(isSaturated){    // if signal is saturated
    display.clearDisplay();
    display.setTextColor(BLACK, WHITE);
    display.setCursor(10,10);
    display.print("Looking for");
    display.setCursor(15,20);
    display.print("the signal");
    
    switch (int(millis()/500) % 4){
      case 0:
        display.setCursor(35,30);
        display.print("");
        break;

      case 1:
        display.setCursor(35,30);
        display.print(".");
        break;

      case 2:
        display.setCursor(35,30);
        display.print("..");
        break;
        
      case 3:
        display.setCursor(35,30);
        display.print("...");
        break;
    }
    
    display.display();
    
  }else{              // if signal is not saturated

    if(saturationStateChanged){
      display.clearDisplay();
      curr_screen_x = ppg_x_lim_px[0];
      dynamicThreshold = 0;
      search_graph_min = 2000;
      search_graph_max = 0;
      graph_min = 0;
      graph_max = 1023;
    }
    
    curr_screen_x = curr_screen_x + 1;
    if(curr_screen_x > ppg_x_lim_px[1]){
      curr_screen_x = ppg_x_lim_px[0];
      
      graph_min = - 50 + search_graph_min;
      graph_max = + 50 + search_graph_max;
      search_graph_min = 2000;
      search_graph_max = 0;
    }
    display.setTextColor(BLACK, WHITE);
    display.setTextSize(1);
    display.setCursor(0,0);
    display.print("BPM:");
  
    // Clear BPM annotation
    for(int i = 0; i <= 6; i++){
      display.drawFastHLine(20, i , 60, WHITE);
    }
  
    // Print BPM
    display.setCursor(20,0);
    if(averagedBPM < 100){
      display.print(String("0") + averagedBPM);
    }else{
      display.print(averagedBPM);
    }
    display.drawFastHLine(0, 8, 83, BLACK);
    display.drawFastVLine(40, 0, 8, BLACK);

    // amplified graph values
    int h = map(ppg[ppg_sample_size - 1], graph_max, graph_min, ppg_y_lim_px[0], ppg_y_lim_px[1]);     // value for ppg[ppg_sample_size - 1]
    int h2 = map(ppg[ppg_sample_size - 2], graph_max, graph_min, ppg_y_lim_px[0], ppg_y_lim_px[1]);     // value for ppg[ppg_sample_size - 1]
    h = max(ppg_y_lim_px[0], h);
    h = min(ppg_y_lim_px[1], h);
    h2 = max(ppg_y_lim_px[0], h2);
    h2 = min(ppg_y_lim_px[1], h2);
    
    // Plot PPG
    // get the y value (in pixels) at which the current PPG value should be plotted
    //int h = map(ppg[ppg_sample_size - 1], 1023, 0, ppg_y_lim_px[0], ppg_y_lim_px[1]);
    if(curr_screen_x == ppg_x_lim_px[0]){
      // clear first raw
      display.drawFastVLine(curr_screen_x, ppg_y_lim_px[0] , ppg_y_lim_px[1] - ppg_y_lim_px[0] + 1, WHITE);
      display.drawPixel(curr_screen_x, h, BLACK);
    }else{
      // get the y value (in pixels) at which the (current-1) PPG value should be plotted
      //int h2 = map(ppg[ppg_sample_size - 2], 1023, 0, ppg_y_lim_px[0], ppg_y_lim_px[1]);
      display.drawLine(curr_screen_x-1, h2, curr_screen_x, h , BLACK);
    }
  
    // Clear 5 vertical lines ahead to make space for the new last_ppg_values of PPG
    for(int i = 1; i < 5; i++){
      display.drawFastVLine(curr_screen_x+i, ppg_y_lim_px[0] , ppg_y_lim_px[1] - ppg_y_lim_px[0] + 1, WHITE);
    }
    
    display.display();
    
    // Play bip sound
    if(playBuzzerQ){      // if buzzer play is enabled
      if(peakDetectedQ){
        tone(buzzerPin, buzzerFreq, buzzerBipDuration);
      }
    }
  
    peakDetectedQ = false;
  }
}


void GUI_generalSettings(){
  int nOptions = sizeof(settingsMenuOptions)/sizeof(String);
  
  if(buttonOkActivatedQ){
    // {mainMenu, plotPPG, generalSettings, screenSettings, soundSettings, credits, peak detections}
    switch(GUIoption){
      case 0:
        GUIpage = 6;
        break;

      case 1:
        GUIpage = 3;
        break;

      case 2:
        GUIpage = 4;
        break;

      case 3:
        resetMenuOptions();
        GUIpage = 0;
        display.clearDisplay();
        break;
    }

    display.clearDisplay();
    resetMenuOptions();
    return;
  }
  if(buttonDownActivatedQ){
    changeGUIOptionValue(1, 0, nOptions - 1);
  }
  if(buttonUpActivatedQ){
    changeGUIOptionValue(-1, 0, nOptions - 1);
  }

  display.setTextColor(BLACK, WHITE);
  display.setCursor(20, 0);
  display.print("SETTINGS");
  display.drawFastHLine(0, 8, 84, BLACK);
  
  for(int i = 0; i < nOptions; i++){
    if(i == GUIoption){
      display.setTextColor(WHITE, BLACK);
    }else{
      display.setTextColor(BLACK, WHITE);
    }
    // write option centered
    int y= i*(47 - 8)/nOptions + 12;
    display.setCursor(int(44 - 6.*((2+settingsMenuOptions[i].length())/2.)) , y);
    display.print(" "+settingsMenuOptions[i]+" ");
  }

  display.display();
}

void GUI_screenSettings(){
  display.clearDisplay();
  // MENU:
  // Bips: ON/OFF
  // Frequency: 640
  // Duration: 200ms
  // Return
  int nOptions = sizeof(screenMenuOptions)/sizeof(String);

  if(buttonDownActivatedQ){
    if(optionSelected){
      if(GUIoption == 1){
        contrast -= 1;
        display.setContrast(contrast);
      }
    }else{
      changeGUIOptionValue(1, 0, nOptions - 1);
    }
  }
  if(buttonUpActivatedQ){
    if(optionSelected){
      if(GUIoption == 1){
        contrast += 1;
        display.setContrast(contrast);
      }
    }else{
      changeGUIOptionValue(-1, 0, nOptions - 1);
    }
  }
  if(buttonOkActivatedQ){
      switch(GUIoption){
        case 0:
          backLightQ = !backLightQ;
          if(backLightQ){
            digitalWrite(backLightPin, HIGH);
          }else{
            digitalWrite(backLightPin, LOW);
          }
          break;
        
        case 2:   // if RETURN
          resetMenuOptions();
          display.clearDisplay();
          GUIpage = 0;
          return;
          break;
          
        default:
          optionSelected = !optionSelected;
          break;
      }

  }

  display.setTextColor(BLACK, WHITE);
  display.setCursor(12, 0);
  display.print("SCREEN MENU");
  display.drawFastHLine(0, 8, 84, BLACK);

  
  for(int i = 0; i < nOptions; i++){
    if(i == GUIoption){
      display.setTextColor(WHITE, BLACK);
    }else{
      display.setTextColor(BLACK, WHITE);
    }
    // write option centered
    int y = i*(47 - 8)/nOptions + 12;
    display.setCursor(0 , y);
    display.print(">"+screenMenuOptions[i]);

    switch(i){
      case 0:
        if(backLightQ){
            display.setTextColor(WHITE, BLACK);
            display.setCursor(45 , y);
            display.print("ON");
            display.setTextColor(BLACK, WHITE);
            display.setCursor(57 , y);
            display.print("/OFF");
        }else{
            display.setTextColor(BLACK, WHITE);
            display.setCursor(45 , y);
            display.print("ON/");
            display.setTextColor(WHITE, BLACK);
            display.setCursor(62 , y);
            display.print("OFF");
        }
        break;

      case 1:
        if(optionSelected && GUIoption == 1){
          display.setTextColor(WHITE, BLACK);
        }else{
          display.setTextColor(BLACK, WHITE);
        }
        display.setCursor(65 , y);
        display.print(contrast);
        break;

    }
  }

  display.display();
}

void GUI_soundSettings(){
  display.clearDisplay();
  // MENU:
  // Bips: ON/OFF
  // Frequency: 640
  // Duration: 200ms
  // Return
  int nOptions = sizeof(soundMenuOptions)/sizeof(String);

  if(buttonDownActivatedQ){
    if(optionSelected){
      switch(GUIoption){
        case 1:
          buzzerFreq -= 10;
          if(buzzerFreq < 300){
            buzzerFreq = 300;
          }
          break;
  
        case 2:
          buzzerBipDuration -= 50;
          if(buzzerBipDuration < 50){
            buzzerBipDuration = 50;
          }
          break;
      }
      noTone(buzzerPin);
      tone(buzzerPin, buzzerFreq, buzzerBipDuration);
    }else{
      changeGUIOptionValue(1, 0, nOptions - 1);
    }
  }
  if(buttonUpActivatedQ){
    if(optionSelected){
      switch(GUIoption){
        case 1:
          buzzerFreq += 10;
          if(buzzerFreq > 800){
            buzzerFreq = 800;
          }
          break;
  
        case 2:
          buzzerBipDuration += 50;
          if(buzzerBipDuration > 500){
            buzzerBipDuration = 500;
          }
          break;
      }
      noTone(buzzerPin);
      tone(buzzerPin, buzzerFreq, buzzerBipDuration);
    }else{
      changeGUIOptionValue(-1, 0, nOptions - 1);
    }
  }
  if(buttonOkActivatedQ){
      switch(GUIoption){
        case 0:
          playBuzzerQ = !playBuzzerQ;
          break;
        
        case 3:   // if RETURN
          resetMenuOptions();
          display.clearDisplay();
          GUIpage = 0;
          return;
          break;
          
        default:
          optionSelected = !optionSelected;
          break;
      }

  }

  display.setTextColor(BLACK, WHITE);
  display.setCursor(12, 0);
  display.print("SOUND MENU");
  display.drawFastHLine(0, 8, 84, BLACK);

  
  for(int i = 0; i < nOptions; i++){
    if(i == GUIoption){
      display.setTextColor(WHITE, BLACK);
    }else{
      display.setTextColor(BLACK, WHITE);
    }
    // write option centered
    int y = i*(47 - 8)/nOptions + 12;
    display.setCursor(0 , y);
    display.print(">"+soundMenuOptions[i]);

    switch(i){
      case 0:
        if(playBuzzerQ){
            display.setTextColor(WHITE, BLACK);
            display.setCursor(45 , y);
            display.print("ON");
            display.setTextColor(BLACK, WHITE);
            display.setCursor(57 , y);
            display.print("/OFF");
        }else{
            display.setTextColor(BLACK, WHITE);
            display.setCursor(45 , y);
            display.print("ON/");
            display.setTextColor(WHITE, BLACK);
            display.setCursor(62 , y);
            display.print("OFF");
        }
        break;

      case 1:
        if(optionSelected && GUIoption == 1){
          display.setTextColor(WHITE, BLACK);
        }else{
          display.setTextColor(BLACK, WHITE);
        }
        display.setCursor(65 , y);
        display.print(buzzerFreq);
        break;

      case 2:
        if(optionSelected && GUIoption == 2){
          display.setTextColor(WHITE, BLACK);
        }else{
          display.setTextColor(BLACK, WHITE);
        }
        display.setCursor(65 , y);
        display.print(buzzerBipDuration);
        break;
    }
  }

  display.display();
}

void GUI_credits(){

  if(buttonOkActivatedQ){
    GUIpage = 0;
    display.clearDisplay();
    return;
  }
  
  display.setTextColor(BLACK, WHITE);
  display.setCursor(5, 0);
  display.print("Developed by:");
  display.drawFastHLine(0, 8, 84, BLACK);

  for(int i = 0; i < sizeof(developers)/sizeof(String); i++){
    // write option centered
    display.setCursor(int(44 - 6.*(developers[i].length()/2.)) , 12 + 12*i);
    display.print(developers[i]);
  }

  display.setCursor(30, 37);
  display.print("(IST)");

  //display.clearDisplay();
  //display.drawBitmap(10, 0,  IST_bitmap, IST_bitmap_WIDTH, IST_bitmap_HEIGHT, 1);
  
  display.display();
}

void GUI_peakDet(){
  
  
}


// ---------------------------------------------- END OF GUI FUNCTIONS --------------------------------------------------------

// Sample PPG signal if TIMER fired
void samplePPGIfNeeded(){
  if(!ppg_updated){
      for(int i = 0; i < ppg_sample_size-1; i++){
        ppg[i] = ppg[i+1];
        ppg_diff[i] = ppg_diff[i+1];
      }
      ppg[ppg_sample_size-1] = last_ppg_val;
      ppg_diff[ppg_sample_size-1] = ppg[ppg_sample_size-1] - ppg[ppg_sample_size-2];

      // processing for peak detection
      if(ppg_diff[ppg_sample_size-1] < 0){
        ppg_diff[ppg_sample_size-1] = 0;
      }else{
        ppg_diff[ppg_sample_size-1] = pow(ppg_diff[ppg_sample_size-1], 2);  
      }

      // update for graph amplitude
      if(last_ppg_val < search_graph_min){
        search_graph_min = last_ppg_val;
      }
      if(last_ppg_val > search_graph_max){
        search_graph_max = last_ppg_val;
      }
      
      ppg_updated = true;
  }
}

void resetMenuOptions(){
  GUIoption = 0;
  optionSelected = false;
}

void checkControler(){
  buttonDownStates[0] = buttonDownStates[1];
  buttonUpStates[0] = buttonUpStates[1];
  buttonOkStates[0] = buttonOkStates[1];
  buttonDownStates[1] = digitalRead(buttonDownPin);
  buttonUpStates[1] = digitalRead(buttonUpPin);
  buttonOkStates[1] = digitalRead(buttonOkPin);
  
  if(buttonDownStates[0] != buttonDownStates[1]){
    if(buttonDownStates[1] == HIGH){
        buttonDownActivatedQ = true;
      }
  }
  if(buttonUpStates[0] != buttonUpStates[1]){
    if(buttonUpStates[1] == HIGH){
        buttonUpActivatedQ = true;
      }
  }
  if(buttonOkStates[0] != buttonOkStates[1]){
    if(buttonOkStates[1] == HIGH){
        buttonOkActivatedQ = true;
      }
  }
}

void resetControler(){
  buttonDownActivatedQ = false;
  buttonUpActivatedQ = false;
  buttonOkActivatedQ = false;
}

void changeGUIOptionValue(int increment, int minVal, int maxVal){       // changes the value of GUIoption by the desired ammount
  GUIoption += increment;
  
  if(GUIoption < minVal){
    GUIoption = maxVal;
  }else if(GUIoption > maxVal){ 
    GUIoption = minVal;
  }
}

// TIMER (read ppg last_ppg_value)
ISR(TIMER1_COMPA_vect){
  last_ppg_val = analogRead(A0);
  // Try this to remove jitte and noise
  // last_ppg_value = alpha * analogRead(n) + (1-alpha) * last_ppg_value;      // e.g. alpha = 0.90 
  
  ppg_updated = false;
  
  TCNT1 = t1_load;
}

// saturation detection
void detect_saturation(){
  double ppg_V = mapfloat(ppg[ppg_sample_size-1], 0, 1023, 0, 5);
  
  if((ppg_V < saturatedThreshold) || (ppg_V > (5.0 - saturatedThreshold))){
    if(!isSaturated){
      saturationStateChanged = true;
    }else{
      saturationStateChanged = false;
    }
    isSaturated = true;
    lastTimeSaturated = millis();
  }else{
    if(millis() - lastTimeSaturated > saturationTimeThreshold){
      if(isSaturated){
        saturationStateChanged = true;
      }else{
        saturationStateChanged = false;
      }
      isSaturated = false;
    }
  }
}

// ******************************************************************************************************************************
// *******************************************           PEAK DETECTION           ***********************************************
// ******************************************************************************************************************************
void peak_detection(){

  // Check for peak detection
  if(millis() - lastTimePeakDetected > minTimeSpanBetweenPeaks){
    if(ppg_diff[ppg_sample_size-1] > dynamicThreshold){
      peakDetectedQ = true;
      lookForTrueLocalMax = true;
      trueLocalMax = 0;

      for(int i = 0; i < averageOverNPeaks - 1; i++){
        lastPeaksTime[i] = lastPeaksTime[i+1];
      }
      lastPeaksTime[averageOverNPeaks - 1] = millis() - lastTimePeakDetected;
      lastTimePeakDetected = millis();
    }
  }

  // If search for the local maximum is over then update
  if((millis() -  lastTimePeakDetected > timeSpanForLocalMaxSearch) && lookForTrueLocalMax){
    lookForTrueLocalMax = false;    // Don't look any longer
    for(int i = 0; i < averageOverNPeaks - 1; i++){
      lastPeaksIntensity[i] = lastPeaksIntensity[i+1];  // Push old values down the vector
    }
    lastPeaksIntensity[averageOverNPeaks - 1] = trueLocalMax; // Update the current value
    
    // Update the dynamic threshold and the BPM value
    dynamicThreshold = 0;
    averagedBPM = 0;
    double f = 2. / (averageOverNPeaks * (averageOverNPeaks + 1));
    for(int i = 0; i < averageOverNPeaks; i++){
      // The first index is actually the oldest peak value and vice-verse, so the 
      // formula slightly changes: instead of (averageOverNPeaks - i + 1) we only write (i)
      dynamicThreshold += f * (i+1) * lastPeaksIntensity[i];
      averagedBPM += lastPeaksTime[i];
    }
    dynamicThreshold *= nextPeakDetectionThreshold;
    // conversion from time to frequency (also convert ms to sec)
    averagedBPM = 1000 * (60. / (averagedBPM/averageOverNPeaks));
  }
  
  // If search for the local maximum is still on then relax the value
  if(lookForTrueLocalMax){
    if(trueLocalMax < ppg_diff[ppg_sample_size-1]){
      trueLocalMax = ppg_diff[ppg_sample_size-1]; 
    }
  }

}

// ----------------------------------------------- EXTRA FUNCTIONS -----------------------------------------------
float mapfloat(long x, long in_min, long in_max, long out_min, long out_max)
{
 return (float)(x - in_min) * (out_max - out_min) / (float)(in_max - in_min) + out_min;
}
