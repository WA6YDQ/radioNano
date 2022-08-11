/* menu.h - this contains the menu sub-system
 * and allows setup of the radioNano controller
 * when VFOMEM is pressed and held when the radio 
 * is powered up.
*/

#define MODERIT 9
#define DEBOUNCE 80
extern LiquidCrystal lcd;

unsigned char menuitems[10][16] = {"VFO Output","Set Default","Set Tone Freq","Beacon CW","Set VM",\
                                  "menu 6","menu 7","menu 8","menu 9","menu 10"};
unsigned int mewnuitem = 0;     // 0-9

void menu(void) {

    lcd.clear(); lcd.print("MENU");
    while (digitalRead(MODERIT)==0) {
        delay(DEBOUNCE);
    }
    delay(DEBOUNCE);

    
    return;
}
