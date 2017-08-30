

/*https://wiki.thingscalderdale.com/LoRaWAN_Workshop
 * 
 *  This sketch is based upon the SmarteEveryting Lion RN2483 Library - sendDataOTA_console
    This sketch also uses the code by Andrew Lindsay created for the things calderdale boost workshop
    created 25 Feb 2017
    by Seve (seve@ioteam.it)

    This example is designed as a periodic upload demonstrator and a compelling lora example https://twitter.com/ABOPThings
    The timing has been re-worked to use a tight timing loop to better control the time between transmit requests and other items.
    This code uses a button connected to Pin 2 (normally open) and will then upload the number of button pushes, Leds on pins 3,5,6 will then show the status.
    Pin 6 is the transmit pin and will be active during transmit
    Pin 5 is the button pin and will go active every time the button is pushed
    Pin 3 is the message waiting indicator and will be active if there is data to send in the next TX window


    Known issues:
    Some formatting issues for 100's of button pushes per minute
    The Button Led is not perfect and has some issues
    This example polls the button an interrupt driven method would be better

    
 */

#include <Arduino.h>
#include <rn2483.h>
#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7735.h> // Hardware-specific library
#include <SPI.h>


/* Create an rtc object */

/* * INPUT DATA (for OTA)
 * 
 *  1) device EUI, 
 *  2) application EUI 
 *  3) and application key 
 *  
 *  then OTAA procedure can start.
 */
#define APP_EUI "xxxxxxxxxxxxxxxx" //"Place ThingsNetwork APP_EUI here"
#define APP_KEY "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx" //"Place ThingsNetwork APP_KEY here"

//****************************BAT CODE***************************////

const int Bats = 2;       // Bat Pulses - Pin 2
const int LedA = 5;       // Green LED - Pin 5
const int LedB = 6;       // Yellow LED - Pin 6
const int LedC = 7;       // Red LED - Pin 7
const int PBF1 = 8;       // F1 PButton - Pin 8
const int PBF2 = 9;       // F2 PButton - Pin 9
unsigned long BatPulse = 0;        // holds the value of PulseIn
int Freq = 0;                      // calculated frequency
unsigned long BatsDetected =0; // to store number of bats
//*****************************************************************///

char c; 
char buff[100]  = {};
int  len        = 0;
unsigned char i = 0;

bool Go_Tx= false;
bool Confirmtx=false;
unsigned char buff_size = 100;
int Serialdebuglevel=1;

    unsigned int adc_volts =0;
    static unsigned long millisecondtimer =0;  
    static unsigned long seconds = 0;
    static unsigned long seconds2 = 0;
    static unsigned int low = 0;
    static unsigned int mid = 0;
    static unsigned int high = 0;
    static unsigned long Tx_delay = 900; // time in seconds
    static unsigned long Confirm_Tx_count = 0; // how often to send a confirmed TX 0 = never // TX counts will reset the bat counters.

void setup() {
    errE err;
    loraDbg = false;         // Set to 'true' to enable RN2483 TX/RX traces
    bool storeConfig = true; // Set to 'false' if persistent config already in place    
    
    //analogReference(INTERNAL2V56);
    delay(500);   
    Serial.begin(115200);

    lora.begin();
    // Waiting for the USB serial connection
//    while (!Serial) {
//        ;
//    }
    Serial.print("LoRa Reset");
    lora.macResetCmd();

    delay(1000);
    Serial.print("FW Version :");
    Serial.println(lora.sysGetVersion());

    /* NOTICE: Current Keys configuration can be skipped if already stored
     *          with the store config Example
     */
    if (storeConfig) {
         // Write HwEUI
        Serial.println("Writing Hw EUI in DEV EUI ...");
        lora.macSetDevEUICmd(lora.sysGetHwEUI());

        if (err != RN_OK) {
            Serial.println("\nFailed writing Dev EUI");
        }
        
        Serial.println("Writing APP EUI ...");
        err = lora.macSetAppEUICmd(APP_EUI);
        if (err != RN_OK) {
            Serial.println("\nFailed writing APP EUI");
        }
        
        Serial.println("Writing Application Key ...");
        lora.macSetAppKeyCmd(APP_KEY);
        if (err != RN_OK) {
            Serial.println("\nFailed writing raw APP Key"); 
        }
        
        Serial.println("Writing Device Address ...");
        err = lora.macSetDevAddrCmd(lora.sendRawCmdAndAnswer("mac get devaddr"));
        if (err != RN_OK) {
            Serial.println("\nFailed writing Dev Address");
        }
        
        Serial.println("Setting ADR ON ...");
        err = lora.macSetAdrOn();
        if (err != RN_OK) {
            Serial.println("\nFailed setting ADR");
        }
    }
    /* NOTICE End: Key Configuration */
    
    Serial.println("Setting Automatic Reply ON ...");
    err = lora.macSetArOn();
    if (err != RN_OK) {
        Serial.println("\nFailed setting automatic reply");
    }
    
 /* KW Removing this as it is bad practice if we dont need to provide too much power.  
  * Serial.println("Setting Trasmission Power to Max ...");
    lora.macPause();
    err = lora.radioSetPwr(14);
    if (err != RN_OK) {
        Serial.println("\nFailed Setting the power to max power");
    }
    lora.macResume();
*/
    delay(5000);
    ledYellowTwoLight(LOW); // turn the LED off 
    //lora.macJoinCmd(OTAA); // Start a join event before we enter the loop (so we may have joined when we get our first TX window)


  //*********************Bat code*****************************************************//
   pinMode ( Bats, INPUT );         // Div 32 pulse input
  
  pinMode ( LedA, OUTPUT );
  pinMode ( LedB, OUTPUT );
  pinMode ( LedC, OUTPUT );
  pinMode ( PBF1, INPUT );
  pinMode ( PBF2, INPUT );
  pinMode (A0,INPUT);
  
  digitalWrite ( LedA, LOW );  // initialize the
  digitalWrite ( LedB, LOW );  // LEDS
  digitalWrite ( LedC, LOW );  // All are off
  //************************************************************************************//

    
}


void loop() {
  

    static int tx_cnt = 0;
    static bool joined = false;
    static uint32_t state;
    int seconds =0;
    int timeatbp;
    int val =0;
    LoRa_management();
  

    bat_detection_loop();
    checktime();
    Time_to_tx();
    
    if (Go_Tx) 
    { 
        String LoRaString;
        val = analogRead(A0);    // read the input pin
        Serial.println(val);             // debug value
        Serial.println("\nTry to TX:\n");
        LoRaString = val;
        LoRaString = LoRaString + ",";
        LoRaString = LoRaString + BatsDetected;
        LoRaString = LoRaString + ",";
        LoRaString = LoRaString + low;
        LoRaString = LoRaString + ",";
        LoRaString = LoRaString + mid;
        LoRaString = LoRaString + ",";
        LoRaString = LoRaString + high;
        
        Serial.println(LoRaString);
        LoRa_TX(LoRaString);  
        Go_Tx=false;
    }

    
}

void bat_detection_loop()
{

    if ( ( digitalRead ( PBF2 ) == LOW )) {   // Test Button / Red LED
        digitalWrite ( LedC, HIGH );          // Turn on Red LED
        digitalWrite ( LedA, LOW );           // Turn off Green LED
    }

    if ( ( digitalRead ( PBF1 ) == LOW )) {   // Test Button / Green LED
      digitalWrite ( LedA, HIGH );            // Turn on Green
      digitalWrite ( LedC, LOW );             // Turn off Red LED
    }

    BatPulse = pulseIn( Bats, HIGH );         // Get a bat pulse
    if(Serialdebuglevel==0)
    {
        Serial.print ( "Pulse: ");                // Show the Raw pulse width
        Serial.print  ( BatPulse );               
        Serial.print (" uSeconds " );
    }
    if ( BatPulse < 1000 ) 
    {                  // Must be > 16kHz
      if ( BatPulse > 200 ) 
      {                 // Must be < 64kHz

        Freq = ( 16000 / BatPulse );          // Calculate Frequency
        if(Serialdebuglevel ==0)
        {
          Serial.print ( " : Frequency is " );  // Show frequency
          Serial.print ( Freq );
          Serial.print ( " kHz " );
        }

        if(Freq <20)
        {
            low++;
        }
        else
        {
            if(Freq <40)
            {
                mid++;
            }
            else
            {
                if(Freq <60)
                {
                    high++;
                }
            }
        }
        
        digitalWrite ( LedB, HIGH );          // Turn on Yellow LED
        BatsDetected++;
        if(Serialdebuglevel <=1)
        {
            Serial.print ("\nBats count");
            Serial.println (BatsDetected);
            Serial.println (low);
            Serial.println (mid);
            Serial.println (high);
        }
        }
    }
    else
    {
      if(Serialdebuglevel==0)
      {  
        Serial.print ("Glitch");
      }
    }
    digitalWrite ( LedB, LOW );               // Turn Yellow LED off
    if(Serialdebuglevel==0)
    {
      Serial.println();                         // finish line
    }
}
void LoRa_management()
{
        // Loop though serial to the RN2483
    if (Serial.available()) {
        c = Serial.read();    
        Serial.write(c); 
        buff[i++] = c;
        if (c == '\n') {
            Serial.print(lora.sendRawCmdAndAnswer(buff));
            i = 0;
            memset(buff, 0, sizeof(buff));
        }
    }
    if (lora.available()) 
    {

        //Unexpected data received from Lora Module;
        //Reset Data if command 1 is sent from network (Yes a magic number these are bad)
        //if(Serialdebuglevel <=1)
        //{
        //      Serial.print("\nRx> ");
        //      
        //      Serial.print(lora.read());
        //}
        if(lora.read()=="mac_rx 1 31")
        {
             Serial.println("Network Clear detected");
             BatsDetected =0; //clear bat count
        }

    }

}
void Time_to_tx()
{
     
    // Serial.println("/nTime left until TX:");
    // Serial.println(Tx_delay-seconds);
     if( Tx_delay == seconds)
     {
         seconds =0;
         Go_Tx=true;
         if (Confirm_Tx_count ==0) // do nothing
         {}
         else
         {
           if(seconds2 == Confirm_Tx_count)  //time to confirm the TX count
           {
                Confirmtx = true;
                seconds2=0;
           }
         }
     }
}

void LoRa_TX(String s)
{
        int state = lora.macGetStatus();
       //Serial.println(state, HEX);
        static bool joined = false;
        static long tx_cnt=0;
        String returned;
       // Check If network is still joined
       if (MAC_JOINED(state)) 
       {
           if (!joined) 
           {
               Serial.println("\nOTA Network JOINED! ");

               joined = true;
           }
               tx_cnt++;

               if(Confirmtx==true)
               {
                   Serial.println("Sending Confirmed String and clearing count...");
                   
                   returned = lora.macTxCmd(String(s), 1, TX_ACK);  // Confirmed tx check received ok so we can clear the count
                   if(Serialdebuglevel <=1)
                   {
                       Serial.println(returned);
                   }
                   if(returned == "RN_OK")
                   {
                       
                        BatsDetected =0;  // clear bat count
                        low=0;
                        mid=0;
                        high=0;
                   }
                   Confirmtx=false;
               }
               else
               {
                   Serial.println("Sending UnConfirmed String ...");
                   lora.macTxCmd(String(s), 1);   // Unconfirmed tx buffer if required
                   BatsDetected =0;  // clear bat count comment for confirmed tx
                   low=0;
                   mid=0;
                   high=0;
               }


                
               
       } 
       else 
       {   
               joined=false;// Network is not joined so try to join
               Serial.println("\nNot Joined Tying to Join");
               lora.macJoinCmd(OTAA); // wait a while after joining
               delay(4000);
       }  
}

void checktime()
{
    
    if ((millisecondtimer + 1000) <= millis()) // check to see if a second has elapsed if it has update the display and seconds counter
    {
         seconds++;
         seconds2++;
        // Serial.println(loop_cnt);
         millisecondtimer=millis(); // store current time
    }
    
    
}


