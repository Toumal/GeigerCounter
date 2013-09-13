// Radiation Geiger Counter
// (c) 2013 Josef Jahn

#include <ITDB02_Graph16.h>
#include <UTouch.h>
#include <RadiationWatch.h>

// Declare which fonts we will be using
extern uint8_t SmallFont[];
extern uint8_t BigFont[];
extern uint8_t SevenSegNumFont[];

ITDB02 myGLCD(38,39,40,41);

UTouch myTouch(6,5,4,3,2);

RadiationWatch radiationWatch(20, 10, 3); // Signal pin=20 (which is IRQ 3 on the Mega2650), noise pin=10, signal irq = 3 (pin 20)

int MAXTIMEFACTOR = 5; // max zoom factor // data history
int MAXBUFFER = 318*MAXTIMEFACTOR;

int graph[318]; // Displayed graph pixel dat
int values[318*5]; // Measurement data buffer, with max time steps
double lastCPM = 0; // last measured CPM
double lastUS = 0; // last measured microsievert
double lastUSerr = 0; // last measured tolerance
char res[30];
char res2[30];
long startTime = 0;
boolean showBigCPM = false; // Toggle between showing microsievert or cpm in big digits
int timeFactor = 1; // Initially we're zoomed in completely to show fast updates
int maxValue;  //In a render loop this is the max encountered data value. For normalization

void setup()
{
  randomSeed(analogRead(0));
  
  // Setup the LCD
  myGLCD.InitLCD(LANDSCAPE);
  myGLCD.setFont(SmallFont);

  // Setup Touchscreen
  myTouch.InitTouch();
  myTouch.setPrecision(PREC_MEDIUM);

  radiationWatch.setup();
  
}

void loop()
{  
  beep();

  // Clear screen and draw frame
  myGLCD.clrScr();
  myGLCD.setColor(255, 0, 0);
  myGLCD.fillRect(0, 0, 319, 13);
  myGLCD.setColor(64, 64, 64);
  myGLCD.fillRect(0, 226, 319, 239);
  myGLCD.setColor(255, 255, 255);
  myGLCD.setBackColor(255, 0, 0);
  myGLCD.print("Beta/Gamma Radiation Dosimeter", CENTER, 1);
  myGLCD.setBackColor(64, 64, 64);
  myGLCD.setColor(255,255,0);
  myGLCD.print("Histogram", CENTER, 227);

  myGLCD.setColor(0, 0, 255);
  myGLCD.drawRect(0, 14, 319, 225);
  myGLCD.setColor(0,0,0);
  myGLCD.fillRect(1,15,318,224);
  myGLCD.setColor(0, 0, 255);
  myGLCD.setBackColor(0, 0, 0);
  myGLCD.drawLine(1, 119, 318, 119);
  drawButton(200, 20, "-", 0);
  drawButton(250, 20, "+", 0);
  drawButton(200, 70, "Unit", 90);
  
  // Draw scale bars
  for (int i=9; i<310; i+=10)
    myGLCD.drawLine(i, 117, i, 120);

  int loopsBetweenGraphUpdate = 0;
  while (true) {
    loopsBetweenGraphUpdate++;
    startTime = millis();
	// run measurements for one second and process keypresses during that time
    while (millis() - startTime < 1000) {
      processTouch();
      radiationWatch.loop();
    }
	// after one second
    ageData(); // move histogram
    calculateMax(); // Calculate histogram scaling
    appendMeasurementData(); // Add the latest measurement data
	// Is it time to update the graph?
    if (loopsBetweenGraphUpdate >= timeFactor) {
      recalculateGraph();
      drawGraph();
      loopsBetweenGraphUpdate = 0;
    }
	// Draw the measurement text
    drawText();

  }
}

// Clear a portion of the screen to erase the text
void clearText(){
  myGLCD.setColor(0, 0, 0);
  myGLCD.fillRect(5, 20, 190, 110);
}

// Draw measurement values
void drawText(){
  // This code displays CPM or uSv/h in big letters, depending on the mode.
  if (showBigCPM == false) {
    myGLCD.setColor(0, 0, 0);
    myGLCD.fillRect(5, 20, 50, 75);
    myGLCD.setColor(255, 255, 255);
    myGLCD.setBackColor(0, 0, 0);
    myGLCD.setFont(SevenSegNumFont);
    sprintf(res2, "%i", int(lastUS)); 
    int charWidth = myGLCD.getFontWidth();
    int stringLength = strlen(res2);
    int stringWidth = charWidth * stringLength;
    myGLCD.print(res2, 84-stringWidth, 22);
    myGLCD.fillRect(90, 65, 95, 70);
    char *tempres = printDouble(res, lastUS, 100, true);
    if (strlen(tempres) == 1)
      sprintf(res2, "%s0", tempres);
    else
      sprintf(res2, "%s", tempres);  
    myGLCD.print(res2, 100, 22);
  
    myGLCD.setFont(SmallFont);
    myGLCD.print("uSv/h", 161, 60);

    myGLCD.print("cpm:              ", 20, 85);
    myGLCD.printNumI(lastCPM, 45, 85, false);
  } else {
    myGLCD.setColor(0, 0, 0);
    myGLCD.fillRect(5, 20, 83, 75);
    myGLCD.setColor(255, 255, 255);
    myGLCD.setBackColor(0, 0, 0);
    myGLCD.setFont(SevenSegNumFont);
    myGLCD.printNumI(lastCPM, 84, 22, true);
    myGLCD.setFont(SmallFont);
    myGLCD.print("cpm", 161, 60);

    char *tempres3 = printDouble(res, lastUS, 100, false);
    sprintf(res2, "%s uSv/h    ", tempres3); 
    myGLCD.print(res2, 20, 85);
  }
  char *tempres2 = printDouble(res, lastUSerr, 100, false);
  sprintf(res2, "+/- %s uSv/h    ", tempres2); 
  myGLCD.print(res2, 20, 100);
}


// Draw the actual histogram from raw pixel data
void drawGraph(){
    
  for (int i=1; i<318; i++) {
    myGLCD.setColor(0,40,0);
    myGLCD.drawLine(i,120,i,224-graph[i]+1);     
    myGLCD.setColor(151+graph[i],255-graph[i],0);
    myGLCD.drawLine(i,224-graph[i],i,224);     
  }   
  myGLCD.setFont(SmallFont);
  myGLCD.setBackColor(64, 64, 64);
  myGLCD.setColor(255,255,0);
  myGLCD.print("   minutes", 5, 227, false);
  myGLCD.printNumI(timeFactor*5, 5, 227, false);
}

// Age data array
void ageData() {
  for (int i=1; i<MAXBUFFER; i++) {
    values[i-1] = values[i];
  }
}

// Calculate the maximum measured value for histogram scaling
void calculateMax() {
  maxValue = 0;
  for (int i=1; i<MAXBUFFER; i++) {
    //Here we find out what the max measured value is for our selected time window
    if (i > ((MAXTIMEFACTOR-timeFactor) * 318) && values[i] > maxValue)
      maxValue = values[i];
  }
  if (maxValue < 40)
    maxValue = 40;
}

// Add the latest measured data to the end of the data array
void appendMeasurementData() {
  //new measurement data
  if (radiationWatch.isAvailable()) {
     lastCPM = radiationWatch.cpm();
     lastUS = radiationWatch.uSvh();
     lastUSerr = radiationWatch.uSvhError();
     values[MAXBUFFER-1] = (int)(lastUS*100.0f);
  }
  if (values[MAXBUFFER-1] > maxValue)
    maxValue = values[MAXBUFFER-1];
}

// This calculates pixel data for the histogram from the big measurement data history
void recalculateGraph() {
  //graph value goes from 0 to 104
  int valCounter = 0;
  for (int i=0; i<318; i++) {
    int sum = 0;
    for (int o=0; o<timeFactor; o++) {
       sum+= (int)((float)values[(318 * (MAXTIMEFACTOR-timeFactor)) + valCounter++] / (float)maxValue * 104.0); // Sum up and normalize the data for display
    }
    sum = sum / timeFactor; // Divide through the zoom factor. The further we zoom out, the more values are added together and then divided through their number.
    graph[i] = sum;
  }
}


void processTouch() {
  if (myTouch.dataAvailable())
  {
    myTouch.read();
    int bx=myTouch.getX();
    int by=myTouch.getY();
    if (bx > 200 && by > 20 && bx < 240 && by<60) {
      if (timeFactor < MAXTIMEFACTOR) {
        timeFactor += 1;
        myGLCD.setColor(255, 255, 0);
        myGLCD.drawRoundRect(200, 20, 240, 60);
        beep();
        calculateMax();
        recalculateGraph();
        drawGraph();
        drawButton(200, 20, "-", 0);
      } else {
        beepError();
      }
    } else if (bx > 250 && by > 20 && bx < 290 && by<60) {
      if (timeFactor > 1) {
        timeFactor -=1;
        myGLCD.setColor(255, 255, 0);
        myGLCD.drawRoundRect(250, 20, 290, 60);
        beep();
        calculateMax();
        recalculateGraph();
        drawGraph();
        drawButton(250, 20, "+", 0);
      } else {
        beepError();
      }
    } else if (bx > 200 && by > 70 && bx < 290 && by<110) {
      myGLCD.setColor(255, 255, 0);
      myGLCD.drawRoundRect(200, 70, 290, 110);
      showBigCPM = !showBigCPM;
      beep2();
      delay(200);
      clearText();
      drawText();
      drawButton(200, 70, "Unit", 90);
    }
    delay(200);
  }
}


void drawButton(int x, int y, char *text, int minWidth) {
  myGLCD.setFont(BigFont);
  int charWidth = myGLCD.getFontWidth();
  int charHeight = myGLCD.getFontHeight();
  int stringLength = strlen(text);
  int stringWidth = charWidth * stringLength;
  int buttonWidth = stringWidth + 4;
  if (buttonWidth < 40)
    buttonWidth = 40;
  if (buttonWidth < minWidth)
    buttonWidth = minWidth;
  int buttonHeight = 40;
  myGLCD.setColor(0, 0, 200);
  myGLCD.fillRoundRect(x, y, x+buttonWidth, y+buttonHeight);
  myGLCD.setColor(255, 255, 255);
  myGLCD.setBackColor(0, 0, 200);
  myGLCD.print(text, x + (buttonWidth/2) - (stringWidth/2), y + (buttonHeight/2) - (charHeight/2));
}


char *printDouble( char result[], double val, unsigned int precision, boolean returnOnlyFrac){
// prints val with number of decimal places determine by precision
// NOTE: precision is 1 followed by the number of zeros for the desired number of decimial places
// example: printDouble( 3.1415, 100); // prints 3.14 (two decimal places)

    unsigned int frac;
    if(val >= 0)
        frac = (val - int(val)) * precision;
    else
        frac = (int(val)- val ) * precision;
    if (returnOnlyFrac == false)
      sprintf(result, "%i.%i", int(val), frac); 
    else
      sprintf(result, "%i", frac); 
    
    return result;
} 

void beep() {
  tone(8, 800, 40); 
}

void beepError() {
  tone(8, 400, 20); 
  delay(30);
  tone(8, 400, 20); 
  delay(30);
  tone(8, 400, 20); 
}

void beep2() {
  tone(8, 1000, 20); 
  delay(50);
  tone(8, 1000, 20); 
}

