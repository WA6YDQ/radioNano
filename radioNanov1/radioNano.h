
/* radioNano.h */

/* frequencies and modes for eeprom storage */
/* (const should save these to flash, write to eeprom later) */
const long defaultFreq[150] = {		// banks 1-3, 50 channels per bank
    
    // bank 01 - amateur radio channels
    1825000L,   // chan 01, bank 01
    1925000L,
	3525000L,	// chan 03, bank 01
	3800000L,
	5330500L,   // chan 05, bank 01 (60M chan 1)
	5346500L,   // (60M chan 2)
	5366500L,	// chan 07, bank 01 (60M chan 3)
	5371500L,   // (60M chan 4)
	5403500L,   // chan 09, bank 01 (60M chan 5)
	7035000L,
	7240000L,
	10110000L,	// chan 12, bank 01
	14035000L,
	14275000L,  // chan 14, bank 01
	18085000L,
	18125000L,
	21040000L,	// chan 17, bank 01
	21300000L,
	24900000L,
	24950000L,  // chan 20, bank 01
	28035000L,
	28350000L,	// chan 22, bank 01
    27125000L,  // chan 23, bank 01
    27385000L,
    27500000L,  // chan 25, bank 01
    0L,
    0L,         // chan 27, bank 01
    0L,
    0L,
    0L,         // chan 30, bank 01
    0L,
    0L,
    0L,
    0L,
    0L,         // chan 35, bank 01
    0L,
    0L,
    0L,
    0L,
    0L,         // chan 40, bank 01
    0L,
    0L,
    0L,
    0L,
    0L,         // chan 45, bank 01
    0L,
    0L,
    0L,
    0L,
    0L,         // chan 50, bank 01
    /* this ends BANK 01 */


    // bank 02, utility channels
    5000000L,   // WWV 5 mc  ch 01, bank 02
    10000000L,  // WWV 10 mc ch 02
    15000000L,  // WWV 15 mc ch 03
     3330000L,  // CHU 3 mc  ch 04
     7850000L,  // CHU 7 mc  ch 05, bank 02
    14670000L,  // CHU 14 mc ch 06
    
     3485000L,  // volmet US, Can ch 07
     6604000L,  // volmet ""  ""  ch 08
    10051000L,  // volmet "" ""   ch 09
    13270000L,  // volmet "" ""   ch 10, bank 02
     6754000L,  // volmet Trenton ch 11
    15034000L,  // volmet   ""    ch 12
     3413000L,  // volmet Europe  ch 13
     5505000L,  // volmet   ""    ch 14
     8957000L,  // volmet   ""    ch 15, bank 02
    13270000L,  // volmet   ""    ch 16 (also canada volmet)
     5450000L,  // volmet Europe  ch 17
    11253000L,  // volmet   ""    ch 18
     4742000L,  // volmet Europe  ch 19
    11247000L,  // volmet   ""    ch 20, bank 02
     6679000L,  // volmet Oceania ch 21
     8828000L,  // volmet   ""    ch 22
    13282000L,  // volmet   ""    ch 23
     6676000L,  // volmet SE Asia ch 24
    11387000L,  // volmet   ""    ch 25, bank 02
     3458000L,  // volmet   ""    ch 26
     5673000L,  // volmet   ""    ch 27
     8849000L,  // volmet   ""    ch 28
    13285000L,  // volmet   ""    ch 29
 
     4426000L,  // USCG Weather   ch 30, bank 02
     6501000L,  //       ""       ch 31
     8764000L,  //       ""       ch 32 (wefax)
    13089000L,  //       ""       ch 33
     4316000L,  //       ""       ch 34
     8502000L,  //       ""       ch 35, bank 02
    12788000L,  //       ""       ch 36 (wefax)
     4125000L,  // USCG Distress  ch 37
     6215000L,  //       ""       ch 38
     8291000L,  //       ""       ch 39
    12290000L,  //       ""       ch 40, bank 02
 
     8992000L,  // USAF HFGCS     ch 41
    11175000L,  // USAF   ""      ch 42
     6739000L,  // USAF   ""      ch 43
      
     8152000L,  //  Marine chat   ch 44
    11330000L,  //  ATC carib B USB    ch 45, bank 02
    11396000L,  //  ATC carib A USB    ch 46, bank 02
    11309000L,  //  ATC N Atlantic E USB ch 47, bank 02
    13306000L,  //  ATC N Atlantic A USB ch 48, bank 02
    13927000L,  //  MARS HF USB
    11232000L,  //  Canadian HF USB ch 50, bank 02
     /* this ends bank 02 */


     // bank 03, broadcast channels
     6000000L,         // chan 01, bank 03
     0L,
     0L,
     0L,
     0L,                
     0L,            // chan 6
     0L,
     0L,
     0L,
     0L,
     0L,            // chan 11
     0L,
     0L,
     0L,
     0L,
     0L,            // ch 16
     0L,
     0L,
     0L,
     0L,
     0L,            // ch 21
     0L,
     0L,
     0L,
     0L,
     0L,0L,0L,0L,0L,    // 26-30
     0L,0L,0L,0L,0L,    // 31-35
     0L,0L,0L,0L,0L,    // 36-40
     0L,0L,0L,0L,0L,    // 41-45
     0L,0L,0L,0L,0L     // 46-50  
};


// mode 0=cw-l, 1=cw-u, 2=lsb, 3=usb, 4=am 
const unsigned char defaultMode[150] = {    // banks 1-3, 50 channels/bank
    // bank 1
    0,0,0,2,3,      // 1-5
    3,3,3,3,0,      // 6-10
    2,0,0,3,0,      // 11-15
    3,0,3,0,3,      // 16-20
    0,3,4,3,3,      // 21-25
    3,3,3,3,3,      // 26-30
    3,3,3,3,3,      // 31-35
    3,3,3,3,3,      // 36-40
    3,3,3,3,3,      // 41-45
    3,3,3,3,3,      // 46-50  

    // bank 2
    4,4,4,3,3,      // 1-5
    3,3,3,3,3,      // 6-10
    3,3,3,3,3,      // 11-15
    3,3,3,3,3,      // 16-20
    3,3,3,3,3,      // 21-25
    3,3,3,3,3,      // 26-30
    3,3,3,3,3,      // 31-35
    3,3,3,3,3,      // 36-40
    3,3,3,3,3,      // 41-45
    3,3,3,3,3,      // 46-50

    // bank 3
    4,4,4,4,4,      // 1-5
    4,4,4,4,4,      // 6-10
    4,4,4,4,4,      // 11-15
    4,4,4,4,4,      // 16-20
    4,4,4,4,4,      // 21-25
    4,4,4,4,4,      // 26-30
    4,4,4,4,4,      // 31-35
    4,4,4,4,4,      // 36-40
    4,4,4,4,4,      // 41-45
    4,4,4,4,4       // 46-50
};
