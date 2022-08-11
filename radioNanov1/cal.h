/* cal.h - set calibration of oscillator
 *  
 *  While pressing and holding the VF/CH button, turn ON the
 *  radio. This will enter this calibration routine. 
 *  When this routine is entered, it pulls the current calibration
 *  from the eeprom and displays it. When the user turns the
 *  rotary controller the cal value will increase or decrease
 *  and the cal factor will be updated to the si5351. The user
 *  will hear the change in the receiver (or see it on a frequency 
 *  counter). When you're happy with the setting press the VF/CH 
 *  button to set. Turn OFF the radio to exit and turn ON to resume 
 *  normal operation. If you don't want to save the new value, turn
 *  OFF the radio w/o pressing VF/CH.
 *  
 *  Pressing the STEP button will change the tuning step. 
 *  
 *  When the EEPROM is re-written (back to defaults) the offset
 *  is set back to 0 and this will need to be run again.
 *  
 *  It is advised to let the receiver warm up for a few minutes before
 *  saving the cal value as it takes a few minutes for the xtal to
 *  achieve a stable frequency. The calibration number displayed is the amount 
 *  in hertz that the frequency is HIGH (positive values) or LOW (negative
 *  values). 
 */


#define ENCODERPULSE 2
#define ENCODERDIR 3
#define VFOMEM 8
#define STEP 10
#define DEBOUNCE 80

extern LiquidCrystal lcd;
extern Si5351 si5351;
int32_t calvalue;
int stepSIZE = 1;

void setCal(void) {
    
    detachInterrupt(digitalPinToInterrupt(ENCODERPULSE));
    interrupts();
    if (digitalRead(ENCODERPULSE)==LOW && digitalRead(ENCODERDIR)==LOW) { // down
        calvalue -= stepSIZE;
    }
    if (digitalRead(ENCODERPULSE)==LOW && digitalRead(ENCODERDIR)==HIGH) { // up
        calvalue += stepSIZE;
    }
    while (1) {
        if (digitalRead(ENCODERPULSE)==HIGH && digitalRead(ENCODERDIR)==HIGH) break;
        delay(1);
    }
    delay(1);   // value determined by trial & error
    
    lcd.setCursor(0,1);
    lcd.print(calvalue,DEC);
    si5351.set_correction(calvalue * 100, SI5351_PLL_INPUT_XO);
    attachInterrupt(digitalPinToInterrupt(ENCODERPULSE),setCal,FALLING);
    return;
}

void calibrate_osc(void) {
     
    attachInterrupt(digitalPinToInterrupt(ENCODERPULSE),setCal,FALLING);
    interrupts();

    si5351.set_freq(10000000*100ULL, SI5351_CLK0);  // set at 10 MHz
       
    lcd.clear();
    lcd.print("CAL 10.0000 Mhz");       // show whats happening

    while (digitalRead(VFOMEM)==0) {
        delay(DEBOUNCE);
    }
    delay(DEBOUNCE);

    // now show cal factor
    EEPROM.get(1000,calvalue);      // read from EEPROM
    if (calvalue == 0xffff) calvalue = 0;
    lcd.setCursor(0,1);             // lower line
    lcd.print(calvalue,DEC);        // show offset

    // and show step size
    lcd.setCursor(10,1);
    lcd.print(stepSIZE,DEC);
    
    while (1) {
        // wait for VF/CH button push
        if (digitalRead(VFOMEM)==0) {
            EEPROM.put(1000,calvalue);      // save offset to eeprom
            lcd.setCursor(0,0); lcd.print("Cal Stored      ");
            while (digitalRead(VFOMEM)==0) {
                delay(DEBOUNCE);
            }
            delay(DEBOUNCE);
            lcd.setCursor(0,0); lcd.print("CAL 10.0000 Mhz");
            continue;
        }
        // or wait for STEP button push
        if (digitalRead(STEP)==0) {
            stepSIZE *= 10;
            if (stepSIZE > 1000) stepSIZE = 1;
            // show stepsize
            lcd.setCursor(10,1);
            lcd.print("      ");
            lcd.setCursor(10,1);
            lcd.print(stepSIZE,DEC);
            while (digitalRead(STEP)==0) {
                delay(DEBOUNCE);
            }
            delay(DEBOUNCE);
            continue;
        }
        continue;       // no other tests
    }
 }
