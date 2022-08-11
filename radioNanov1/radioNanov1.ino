

/* radioNano v1
 *  
 *  (C) Kurt Theis 2022 WA6YDQ
 *  This is a controller for an HF radio using qrp-labs.com 
 *  receiver module, polyphasor module and their si5351 osc.
 *  For an audio amp I use an LM386 with no extra gain (the poly-
 *  phasor has plenty of audio output).
 *  
 *  This is the replacement to Radio Uno, previously published
 *  by me on github. 
 *  In this version I use the etherkit si5351 drivers. Easy to 
 *  setup and use, ability to enable/disable indiv outputs.
 *  
 *  I also use (instead of the Arduino Uno) the Arduino Nano,
 *  hence the name change. Gives me a couple of extra analog
 *  inputs and is 1/3 the price.
 *  
 *  EEPROM addresses: 
 *  1000 offset to xtal osc freq (4 bytes) (calibration offset in Hz)
 *  1004 saved frequency for turn-on (4 bytes) (in Hz)
 *  1008 saved mode for turn-on (1 byte)
 *  0-600 - banks 1-3/chan 1-50 for channel memory (600 bytes)
 *  600-750 - banks 1-3/chan 1-50 for channel mode (150 bytes)
 *  
 */

#include <LiquidCrystal.h>
#include <EEPROM.h>
#include <Wire.h>           // for si5351 and MCP23008
#include <si5351.h>         // new w/options
#include <stdio.h>          // for sprintf()
#include "menu.h"           // menu sub-system
#include "radioNano.h"      // default frequencies/modes for eeprom, etc
#include "cal.h"            // calibration routine

/* define pin numbers  */
#define VFOMEM 8            // D8 short press vfm/mem  long press store/recall
#define MODERIT 9           // D9 short press mode     long press rit
#define KEY 12              // D13 cw key (input)
#define SIDETONE A0         // sidetone out A0
#define SAVEMEM A0          // in rx mode, pressing saves current freq/mode to eeprom
#define VOLTMETER A7        // analog in dc voltmeter
#define TRRELAY A3          // output, tr relay (active high)
#define TXKEY A2            // output, tx keyline (active low)
#define SSBSEL A1           // output, 1=USB, 0=LSB (relay control)
#define STEP 10             // input, set step size
#define ENCODERDIR 3        // input, shaft decoder direction
#define ENCODERPULSE 2      // input, shaft encoder pulse (pin 2 is int pin)

/* define settings */
#define REFOSC 27000000     // si5351 reference osc frequency Hz (from crystal)
#define DEBOUNCE 80         // time in ms for switch debounce 
#define MINFREQ 100000L     // minimum frequency for osc (100 khz)
#define MAXFREQ 30000000L   // maximum frequency for osc (30 MHz)
#define MAXMODE 4           // maximum mode value

/* instantiate display */
LiquidCrystal lcd(11, 13, 4, 5, 6, 7);     // LCD RS, E, D4, D5, D6, D7

/* instantiate oscillator */
Si5351 si5351;          // set up osc

/* define external routines */
extern void calibrate_osc(void);
extern void menu(void);

/* define global variables */

const char modetext[5][5] = {"CW-L", "CW-U", "LSB ", "USB ", "AM  "};
long mode_frequency[5] = {-700L,+700L,0L,0L,0L}; // offsets to rx freq, mode-based
unsigned char modeval = 0;              // default mode
unsigned char oldModeval;               // saved in MEM mode (restored in VF mode)
long frequency = 7047500L;              // initial frequency in hertz.
long oldFrequency = 0L;                 // save value when in memory mode
int sidetonefreq=700;                   // initial sidetone frequency (modified in menu())
char lcdstr[16];                        // for lcd printing w/sprintf
long STEPSIZE = 100L;                   // intitial stepsize in Hz
unsigned char vfoChannelMode = 0;       // 0 if in vfo mode, 1 if in channel mode
unsigned char channel = 1;              // initial channel number, 1-50
unsigned char bank = 1;                 // initial bank number, 1-3
int LOADEEPROM = 0;             // set to 1 to load eeprom w/defaults in setup(), 0 otherwise
int storeChan = 0;              // value determines bank or channel in store routine
unsigned char RELAY_KEYED = 0;



/*** Subroutines ***/



/* updateDisplay() - read the various vars and update the LCD display */
void updateDisplay(void) {
    updateFreqDisplay();            // show current frequency
    updateModeDisplay();            // show current mode
    readDcVoltage();                // show dc voltmeter
    updateVfoMemDisplay();          // show vfo/mem channel/bank
    showStep();                     // show step size
    return;
}



/* Display current frequency */
void updateFreqDisplay(void) {  

    long display_freq = frequency+mode_frequency[modeval];  // account for cw offsets
    int fkc = display_freq/1000L;
    long fhz = display_freq - (fkc*1000L);

    /* what to show if frequency is 0 (blank channel in eeprom) */
    if (display_freq == 0L) {
        lcd.setCursor(8,0);         // top row
        lcd.print("[BLANK] ");
        return;
    }

    // show freq on display
    sprintf(lcdstr,"%05d.%03d",fkc, fhz);
    lcd.setCursor(8,0);     // top row
    lcd.print(lcdstr);
    showStep();         // display cursor for step size
    return; 
}



/* display current mode */
void updateModeDisplay() {
    lcd.setCursor(0,0);     // top row
    lcd.print(modetext[modeval]);
    return;
}



/* display vfo/memory mode */
void updateVfoMemDisplay(void) {
    lcd.setCursor(0,1);     // bottom row
    if (vfoChannelMode == 0) {
        sprintf(lcdstr,"VF        ");
        lcd.print(lcdstr);
    } else {
        sprintf(lcdstr,"B:CH %d:%02d",bank,channel);
        lcd.print(lcdstr);
    }
    return;
}


/* set rx mode relay when MODE button is pressed */
void setMode(void) {

    /* set the polyphasor relay to LSB/USB */
    if (modeval==0 || modeval==2 || modeval==4) // cw-l, lsb, am
        digitalWrite(SSBSEL, LOW);              // relay OFF
    if (modeval==1 || modeval==3)               // cw-u, usb
        digitalWrite(SSBSEL, HIGH);             // relay ON
        
    return;
}

/* call this after encoder rotation in channel/vfo mode */
void encoderCommands(void) {
    // only update mode-specific items. lcd updates take significant time.
    if (vfoChannelMode) updateVfoMemDisplay();      // show channel/vfo
    if (vfoChannelMode) eeprom_read();              // get frequency/mode
    if (vfoChannelMode) setMode();                  // set relay on poly board
    if (vfoChannelMode) updateModeDisplay();        // show usb/lsb etc
    updateFreqDisplay();                            // show frequency
    showStep();                                     // step size cursor
    update_receive_frequency();                     // update oscillator
    // update port expander for new band if needed
    return;
}

/* encoder is turned CCW - step decrease */
void encoderDown(void) {    // DIR goes low
    detachInterrupt(digitalPinToInterrupt(ENCODERDIR));
    detachInterrupt(digitalPinToInterrupt(ENCODERPULSE));
    interrupts();
    while (1) {     // still turning - wait for condition we want
        if (digitalRead(ENCODERDIR)==LOW && digitalRead(ENCODERPULSE)==LOW) break;
        if (digitalRead(ENCODERDIR)==HIGH && digitalRead(ENCODERPULSE)==HIGH) break;
    }
    
    if (digitalRead(ENCODERDIR)==HIGH && digitalRead(ENCODERPULSE)==HIGH) goto edonedn;
    
    // ready for operation
    if (digitalRead(ENCODERDIR)==LOW && digitalRead(ENCODERPULSE)==LOW) {
        if (vfoChannelMode == 0) {     // vfo mode
            frequency -= STEPSIZE;
            if (frequency < MINFREQ) frequency = MINFREQ;
        }
        if (vfoChannelMode == 1) {     // channel mode
            channel -= 1;
            if (channel < 1) channel = 50;   
        }
        encoderCommands();
    } 

    // show dot when encoder screws up
    lcd.noCursor(); lcd.setCursor(5,0); lcd.print(".");
    // operation done - wait for stability
    while (1) {
       if (digitalRead(ENCODERPULSE)==HIGH && digitalRead(ENCODERDIR)==HIGH) break;
    }
    lcd.noCursor(); lcd.setCursor(5,0); lcd.print(" "); showStep();

edonedn:    
    // re-enable interrupts
    attachInterrupt(digitalPinToInterrupt(ENCODERDIR),encoderDown,FALLING);
    attachInterrupt(digitalPinToInterrupt(ENCODERPULSE),encoderUp,FALLING);
    return;
}


/* encoder is turned CW - step increase */
void encoderUp(void) {  // PULSE goes low
    detachInterrupt(digitalPinToInterrupt(ENCODERPULSE));
    detachInterrupt(digitalPinToInterrupt(ENCODERDIR));
    interrupts();
    while (1) {     // still turning - wait for condition we want
        if (digitalRead(ENCODERPULSE)==LOW && digitalRead(ENCODERDIR)==LOW) break;
        if (digitalRead(ENCODERDIR)==HIGH && digitalRead(ENCODERPULSE)==HIGH) break;
    }
    
    if (digitalRead(ENCODERDIR)==HIGH && digitalRead(ENCODERPULSE)==HIGH) goto edoneup;
    
    // ready for operation
    if (digitalRead(ENCODERPULSE)==LOW && digitalRead(ENCODERDIR)==LOW) {
        if (vfoChannelMode == 0) {     // vfo mode
            frequency += STEPSIZE;
            if (frequency > MAXFREQ) frequency = MAXFREQ;
        }
        if (vfoChannelMode == 1) {     // channel mode
            channel += 1;
            if (channel > 50) channel = 1;  
        }
        encoderCommands();
    } 

    // show dot when encoder screws up
    lcd.noCursor(); lcd.setCursor(5,0); lcd.print(".");
    // operation done - wait for stability
    while (1) {
        if (digitalRead(ENCODERPULSE)==HIGH && digitalRead(ENCODERDIR)==HIGH) break;
    } 

    lcd.noCursor(); lcd.setCursor(5,0); lcd.print(" "); showStep();
    
edoneup:
    // re-enable interrupts
    attachInterrupt(digitalPinToInterrupt(ENCODERPULSE),encoderUp,FALLING);
    attachInterrupt(digitalPinToInterrupt(ENCODERDIR),encoderDown,FALLING);
    return;
}






/* recall a channel - read freq, mode from eeprom */
void eeprom_read(void) {
    unsigned int address;
    
    /* for frequency: calculate the offset from channels and bank, add return size */
    address = ((channel-1) + ((bank-1)*50)) * 4;    // 4 bytes in a long
    EEPROM.get(address, frequency);
    
    /* for mode: same, but start at offset value (offset is long * chan*bank) */
    address = ((channel-1) + ((bank-1)*50)) + 600;  // offset of 600 is 50 chan*3 banks (150*4)
    EEPROM.get(address, modeval); 

    if (frequency < MINFREQ || frequency > MAXFREQ) frequency = 0L;
    if (modeval > MAXMODE) modeval = 0;         // ignore eeprom corruption, show BLANK
    
    return;
}




/* show stepsize */
void showStep(void) {
    if (STEPSIZE == 10L) lcd.setCursor(15,0);
    if (STEPSIZE == 100L) lcd.setCursor(14,0);
    if (STEPSIZE == 1000L) lcd.setCursor(12,0);
    if (STEPSIZE == 10000L) lcd.setCursor(11,0);
    if (STEPSIZE == 100000L) lcd.setCursor(10,0);    
    if (vfoChannelMode)
        lcd.noCursor();     // don't show cursor in MEM mode
    else
        lcd.cursor();       // but only in VF mode
    return;
}


/* Update Receive Frequency - send receive frequency to oscillator */
void update_receive_frequency(void) {
    uint8_t err = 0;
    if (frequency < MINFREQ || frequency > MAXFREQ) return;
    // *4 because of receiver needing 4x frequency, *100 because clock wants millihertz
    err = si5351.set_freq((unsigned long long)frequency*400ULL, SI5351_CLK0);
    if (!err) return;       // no errors
    // setting freq returned an oscillator error
    lcd.clear(); lcd.print("CLOCK ERROR");  // show the error
    while (1) continue;           // and stop everything (this is fatal)
}


/* Initially load the eeprom with frequencies and modes.
 * Note that this is only called once when first setting up
 * or from the menu if you want to restore channels to defaults
*/
 
void load_eeprom(void) {
    unsigned int address;
    unsigned int channel = 0;
    // load frequency first
    for (address = 0; address < 600; address+=4) {   // 150 chan*4 bytes/freq=600
        EEPROM.put(address, defaultFreq[channel++]);
    }

    // now load the modes
    channel = 0;
    for (address = 600; address < 750; address++) { 
        EEPROM.put(address, defaultMode[channel++]);
    }

    return;
}


/* for storeChannel(), use knob to sel bank/chan & show result */
void showStore(void) {
    long tempFreq;
    int address;
    
    // clockwise (increasing)
    if (digitalRead(ENCODERPULSE)==LOW && digitalRead(ENCODERDIR)==HIGH) {
    if (storeChan == 1) {       // in store mode - change bank
            bank++;
            if (bank > 3) bank = 1;
            address = ((channel-1) + ((bank-1)*50)) * 4;
            EEPROM.get(address, tempFreq);
            sprintf(lcdstr,"%d:%02d ",bank,channel);
            lcd.setCursor(0,0); lcd.print(lcdstr);
            lcd.print(tempFreq); lcd.print("   ");
            lcd.setCursor(0,1); lcd.print("Sel=BANK");
            while (digitalRead(ENCODERPULSE)==LOW) continue;
            //interrupts();
            return;
        }
        if (storeChan == 2) {       // in store mode - change channel
            channel++;
            if (channel > 50) channel = 1;
            address = ((channel-1) + ((bank-1)*50)) * 4;
            EEPROM.get(address, tempFreq);
            sprintf(lcdstr,"%d:%02d ",bank,channel);
            lcd.setCursor(0,0); lcd.print(lcdstr);
            lcd.print(tempFreq); lcd.print("   ");
            lcd.setCursor(0,1); lcd.print("Sel=CHAN");
            while (digitalRead(ENCODERPULSE)==LOW) continue;
            //interrupts();
            return;
        }
    }
    // counter-clockwise (decreasing)
    if (digitalRead(ENCODERPULSE)==LOW && digitalRead(ENCODERDIR)==LOW) {
    if (storeChan == 1) {       // in store mode - change bank
            bank--;
            if (bank < 1) bank = 3;
            address = ((channel-1) + ((bank-1)*50)) * 4;
            EEPROM.get(address, tempFreq);
            sprintf(lcdstr,"%d:%02d ",bank,channel);
            lcd.setCursor(0,0); lcd.print(lcdstr);
            lcd.print(tempFreq); lcd.print("   ");
            lcd.setCursor(0,1); lcd.print("Sel=BANK");
            while (digitalRead(ENCODERPULSE)==LOW) continue;
            //interrupts();
            return;
        }
        if (storeChan == 2) {       // in store mode - change channel
            channel--;
            if (channel < 1) channel = 50;
            address = ((channel-1) + ((bank-1)*50)) * 4;
            EEPROM.get(address, tempFreq);
            sprintf(lcdstr,"%d:%02d ",bank,channel);
            lcd.setCursor(0,0); lcd.print(lcdstr);
            lcd.print(tempFreq); lcd.print("   ");
            lcd.setCursor(0,1); lcd.print("Sel=CHAN");
            while (digitalRead(ENCODERPULSE)==LOW) continue;
            //interrupts();
            return;
        }
    }
}


/* store the current freq/mode in user-selected bank:channel */
void storeChannel(void) {
long tempFreq;
int address;

    noInterrupts();
    detachInterrupt(digitalPinToInterrupt(ENCODERPULSE));
    detachInterrupt(digitalPinToInterrupt(ENCODERDIR));
    attachInterrupt(digitalPinToInterrupt(ENCODERPULSE),showStore,FALLING);
    interrupts();
    
    storeChan = 1;      // when 1, knob sets bank for selection
    bank = 1; channel = 1;
    
    // show bank:channel and freq on display
    lcd.clear();
    address = ((channel-1) + ((bank-1)*50)) * 4;
    EEPROM.get(address, tempFreq);
    sprintf(lcdstr,"%d:%02d ",bank,channel);
    lcd.setCursor(0,0); lcd.print(lcdstr);
    lcd.print(tempFreq);
    lcd.setCursor(0,1); lcd.print("Sel=BANK");
    while (digitalRead(VFOMEM)==0) {
        delay(DEBOUNCE);
    }
    delay(DEBOUNCE);
    
    
    
    while (1) {
        if (digitalRead(STEP) == 0) {
            storeChan += 1;
            if (storeChan == 3) storeChan=1;
            if (storeChan==1) {
                lcd.setCursor(0,1); lcd.print("Sel=BANK");
            }
            if (storeChan==2) {
                lcd.setCursor(0,1); lcd.print("Sel=CHAN");
            }
            while (digitalRead(STEP)==0) {
                delay(DEBOUNCE);
            }
            delay(DEBOUNCE);
            continue;
        }
        if (digitalRead(VFOMEM)==0) {   // store freq/mode in selected bank:channel
            address = ((channel-1) + ((bank-1)*50)) * 4;
            EEPROM.put(address,frequency);
            address = ((channel-1) + ((bank-1)*50)) + 600;
            EEPROM.put(address, modeval);
            lcd.clear();
            lcd.print("Chan Saved");
            delay(1000);
            lcd.clear();
            updateVfoMemDisplay();
            updateModeDisplay();
            updateFreqDisplay();
            readDcVoltage();
            showStep();
            
            while (digitalRead(VFOMEM)==0) {
                delay(DEBOUNCE);
            }
            delay(DEBOUNCE);
            noInterrupts();
            detachInterrupt(digitalPinToInterrupt(ENCODERPULSE));
            attachInterrupt(digitalPinToInterrupt(ENCODERPULSE),encoderUp,FALLING);
            attachInterrupt(digitalPinToInterrupt(ENCODERDIR),encoderDown,FALLING);
            interrupts();
            return;
        }
        if (digitalRead(MODERIT)==0) {      // abort storing - just exit
            lcd.clear(); lcd.print("Aborting Save");
            delay(1000);
            lcd.clear();
            updateVfoMemDisplay();
            updateModeDisplay();
            updateFreqDisplay();
            readDcVoltage();
            showStep();
            while (digitalRead(MODERIT)==0) {
                delay(DEBOUNCE);
            }
            delay(DEBOUNCE);
            noInterrupts();
            detachInterrupt(digitalPinToInterrupt(ENCODERPULSE));
            attachInterrupt(digitalPinToInterrupt(ENCODERPULSE),encoderUp,FALLING);
            attachInterrupt(digitalPinToInterrupt(ENCODERDIR),encoderDown,FALLING);
            interrupts();
            return;
        }
    }

    return;
}


/* Read and display DC voltage */
void readDcVoltage(void) {
    int voltage, tens, hunds;
    noInterrupts();     // if tuning happens during this routine, display gets corrupted
    voltage = 10 * analogRead(VOLTMETER);
    voltage /= 37;      // correction value
    // now convert count to decimal voltage
    tens = voltage/10;
    hunds = voltage - (tens*10);
    // and display it
    sprintf(lcdstr,"%02d.%dv",tens,hunds);
    lcd.setCursor(11,1); lcd.print(lcdstr);
    interrupts();           // stops display corruption
    return;
}


/************************************************/
/*** set up and initialize the radio hardware ***/
/************************************************/

void setup() {

    // after power-up, it takes apx 2 seconds to get here. Bootloader?

    /* set up the LCD */
    lcd.begin(16,2);
    
    lcd.clear(); lcd.setCursor(0,0); lcd.print("Kurt Theis");    // I made this!
    lcd.setCursor(0,1); lcd.print("(C)2022 WA6YDQ");

    /* setup buttons, encoder, etc */
    pinMode(VFOMEM, INPUT_PULLUP);
    pinMode(MODERIT, INPUT_PULLUP);
    pinMode(KEY, INPUT_PULLUP);     // cw key
    pinMode(TRRELAY, OUTPUT);
    digitalWrite(TRRELAY, LOW);     // relay OFF 
    pinMode(TXKEY, OUTPUT);
    digitalWrite(TXKEY, LOW);       // unkeyed
    pinMode(SSBSEL, OUTPUT);        // usb/lsb polyphaser relay
    digitalWrite(SSBSEL, LOW);      // off in LSB, AM. On in USB
    pinMode(STEP, INPUT_PULLUP);
    pinMode(ENCODERDIR, INPUT_PULLUP);
    pinMode(ENCODERPULSE, INPUT_PULLUP);

    /* set up I2C for port expander */
    //Wire.begin();
    //Wire.beginTransmission(0x20);   // port adderss
    //Wire.write(0x00);               // select IODIRA
    //Wire.write(0x00);               // port A is all output
    //Wire.endTransmission();

    /* serial setup for debugging */
    //Serial.begin(9600);
    //Serial.print("starting\n");

    /* set up the si5351 osc (this uses 9k of flash, 200 bytes of ram) */ 
    bool osc_found = si5351.init(SI5351_CRYSTAL_LOAD_8PF,REFOSC,0);    // 27mhz ref clock
    if (!osc_found) {
        lcd.clear();
        lcd.print("osc not running");
        while (1);
    }
    
    /* Set a correction factor since the 27 MHz reference osc is not at 27.0000 MHz.
    *  You will need to do this yourself for your setup. corr val is a 32 bit signed int
    *  and is the value that the freq is above/below expected.
    *  Press/hold the VF/CH button while turning on to enter calibration mode.
    *  Press VF/CH to save new cal value, turn radio OFF to not set anything.
    *  Turn TUNE knob to change value in Hz, press STEP to change step size.
    */
    int32_t calOffset;
    EEPROM.get(1000,calOffset);     // calibration in EEPROM(1000) is in hz, mult by 100
    si5351.set_correction(calOffset*100,SI5351_PLL_INPUT_XO);  // mine is 1.721 Khz off
    
    si5351.drive_strength(SI5351_CLK0, SI5351_DRIVE_8MA);
    si5351.output_enable(SI5351_CLK0, 1);       // enable  #1 osc output (receive output)
    si5351.output_enable(SI5351_CLK1, 0);       // disable #2 osc output (transmit output)
    si5351.output_enable(SI5351_CLK2, 0);       // disable #3 osc output (reference output)
    delay(700);        // let osc settle out while showing splash screen

    /* use interrupts when the shaft encoder is turned */
    interrupts();       // encoderUp() and encoderDown() to change freq etc

    
    /* if defined, store defaults to EEPROM (NOTE: This includes the frequency offset) */
    if (LOADEEPROM) {
        lcd.clear();
        lcd.print("Loading EEPROM");
        
        // set default frequencies & modes
        load_eeprom();                  // see radioNano.h
        
        // initialize the osc val value
        int32_t init_cal = 0;
        EEPROM.put(1000,init_cal);      // frequency offset
        
        delay(1000);
    }
    LOADEEPROM = 0;     // previously defined in global variables above

    /* done with setup */
    lcd.clear(); lcd.print("Setup Done");       // debuging
}




/*************************/
/*** Main program loop ***/
/*************************/

void loop() {

    uint8_t err = 0;        // osc tune result. 1 is failure, 0 is OK

    /* test for button presses at startup */
    if (digitalRead(VFOMEM)==0) {
        calibrate_osc();
    }

    if (digitalRead(MODERIT)==0) {
        menu();
    }

    /* allow dc voltage measurements */
    unsigned long mytime_volts;
    mytime_volts = millis();

    /* hold TR relay time on value */
    unsigned long mytime_trrelay;

    /*** Radio Startup ***/
    lcd.clear();
    
    /* start in vfo mode */
    vfoChannelMode = 0;

    /* load freq/mode from last saved in EEPROM */
    EEPROM.get(1004, frequency);
    EEPROM.get(1008, modeval);
    if (frequency < MINFREQ || frequency > MAXFREQ) {   // EEPROM has bad data
        frequency = 7047500L; modeval = 0;      // default for first turn-on
    }
    
    /* show radio status */
    updateDisplay();
    
    /* set receive frequency */
    update_receive_frequency();
    
    /* set usb/lsb relay */
    setMode();

    /* enable rotary encoder normal radio operation */
    attachInterrupt(digitalPinToInterrupt(ENCODERPULSE),encoderUp,FALLING);
    attachInterrupt(digitalPinToInterrupt(ENCODERDIR),encoderDown,FALLING);

    digitalWrite(TRRELAY,0);            // enable receive

    /* Start Main Loop 
     * test cw key & buttons for activity.
     * the shaft encoder uses interrupts
     * so is not tested here, but the
     * step button is.
     */
     
    while (1) {

        // show dc voltage every 60 seconds
        if (millis() > mytime_volts + 60000) {
            readDcVoltage();            // read and display voltage
            mytime_volts = millis();    // reset timer
            showStep();                 // reset cursor
            continue;
        }


        /*----------------- Test Front Panel Buttons ---------- */

        /* test MODERIT button - short press, mode change. long press, rit on/off */
        if (digitalRead(MODERIT)==0) {
            delay(350);         // short delay
            if (digitalRead(MODERIT) == 1) {    // switch modes with short push
                modeval++;
                if (modeval > MAXMODE) modeval = 0;
                setMode();                  // set new mode
                updateModeDisplay();        // show new mode
                updateFreqDisplay();        // account for mode offsets changing
                update_receive_frequency();
                readDcVoltage();
                showStep();                 // keep step cursor at right place
                continue;                   // done with mode change
            }
            // turn on/off RIT with long push
            if (digitalRead(MODERIT) == 0) {    // still pushed
                EEPROM.put(1004,frequency);     // save frequency info
                EEPROM.put(1008,modeval);       // and mode  
                lcd.clear();
                lcd.print("quick save");             // show what happened
                while (digitalRead(MODERIT)==0) {
                    delay(DEBOUNCE);
                }
                lcd.clear();
                delay(DEBOUNCE);
                updateDisplay();
                continue;
            }
        }   // end of MODERIT button

        /*--------------------------------------------------*/

        // test VFO/Channel button
        if (digitalRead(VFOMEM)==0) {
            delay(350);
            if (digitalRead(VFOMEM)==1) {   // short press: vfo/mem mode
                if (vfoChannelMode == 0) {      // currently in vfo mode - switch to channel mode
                    vfoChannelMode = 1;
                    lcd.noCursor();             // turn off the cursor for step size
                    bank = 1;                   // set bank number to beginning
                    channel = 1;                // and channel
                    oldFrequency = frequency;
                    oldModeval = modeval;
                    // now change to channel mode
                    updateVfoMemDisplay();
                    eeprom_read();
                    updateFreqDisplay();
                    setMode();
                    updateModeDisplay();
                    update_receive_frequency();
                    readDcVoltage();
                    delay(DEBOUNCE);
                    continue;                   // and jump back to main
                }
                if (vfoChannelMode == 1) {      // currently in channel mode - increment bank
                    bank += 1;
                    channel = 1;
                    if (bank > 3) {             // bank=3, back to vfo mode
                        bank = 1;
                        vfoChannelMode = 0;
                        lcd.cursor();
                        frequency = oldFrequency;
                        modeval = oldModeval;
                    }
                    if (vfoChannelMode) eeprom_read();
                    updateVfoMemDisplay();
                    updateFreqDisplay();
                    setMode();
                    updateModeDisplay();
                    update_receive_frequency();
                    readDcVoltage();
                    showStep();
                    delay(DEBOUNCE);
                    continue;
                }
                continue;       // nothing else possible, but cont just in case
            }
            // button still pressed - long press
            if (vfoChannelMode == 1) {      // memory mode, so recall current freq/mode
                vfoChannelMode = 0;         // and switch to VF mode
                updateVfoMemDisplay();
                updateFreqDisplay();
                setMode();
                updateModeDisplay();
                update_receive_frequency();
                readDcVoltage();
                showStep();
                // wait until button is released
                while (digitalRead(VFOMEM)==0) {
                    delay(DEBOUNCE);
                }
                delay(DEBOUNCE);
                continue;
            }
            if (vfoChannelMode == 0) {      // vfo mode - store freq/mode in bank:chan
                storeChannel();
                delay(DEBOUNCE);
                continue;
            }
        

            
        }   // end of VFO/Channel button

        /*--------------------------------------------------*/

        // test STEP button (on shaft encoder)
        
        // short press: cycle from 10, 100, 1000 Hz then back to 10Hz.
        // long press: cycle to 10 KHz, 100 KHz, then back to 10 KHz
        
        if (digitalRead(STEP)==0) {     // short/long press
            delay(350);
            if (digitalRead(STEP)==1) {
                if (!vfoChannelMode) {      // don't bother in MEM mode
                    STEPSIZE *= 10L;
                    if (STEPSIZE > 1000L) STEPSIZE = 10L;
                    showStep();
                }
                continue;
            }
            // long press
            if (!vfoChannelMode) {      // don't bother in MEM mode
                if (STEPSIZE < 10000L) {
                    STEPSIZE = 10000L;
                    showStep();
                    while (digitalRead(STEP)==0) continue;
                    delay(DEBOUNCE);
                    continue;
                }
                if (STEPSIZE == 10000L) {
                    STEPSIZE *= 10L;
                    showStep();
                    while (digitalRead(STEP)==0) continue;
                    delay(DEBOUNCE);
                    continue;
                }
                if (STEPSIZE == 100000L) {
                    STEPSIZE = 10000L;
                    showStep();
                    while (digitalRead(STEP)==0) continue;
                    delay(DEBOUNCE);
                    continue;
                }
            }

        }   // end of STEP button


        /* if tr relay active, test for time out and make inactive */
        if (mytime_trrelay + 500 > millis() && RELAY_KEYED) {  // timed out (0.5 seconds)
            digitalWrite(TRRELAY,0);            // turn off relay
            RELAY_KEYED = 0;
        }

        /*----------------- Test key line ---------------------------------*/

        // test TX line
        if (digitalRead(KEY)) continue;
        digitalWrite(TRRELAY,1);        // enable TR relay
        RELAY_KEYED = 1;
        mytime_trrelay = millis();      // timer for TR RELAY
        delay(10);                      // let relay settle (turn this off for burst)
        err = si5351.set_freq((unsigned long long)frequency*100ULL, SI5351_CLK1);
        if (err) {          // clock error
            lcd.clear(); lcd.print("Clock Error");
            while (1) continue;
        }
        si5351.output_enable(SI5351_CLK1, 1);       // enable #2 osc output (transmit output)
        digitalWrite(TXKEY,1);          // key xmtr
        tone(SIDETONE,sidetonefreq);
        while (digitalRead(KEY)==0) continue;       // wait for dekey
        digitalWrite(TXKEY,0);          // turn off xmtr
        si5351.output_enable(SI5351_CLK1, 0);       // turn off tx osc
        noTone(SIDETONE);
        
        continue;
    }   /* done with main loop */
}
