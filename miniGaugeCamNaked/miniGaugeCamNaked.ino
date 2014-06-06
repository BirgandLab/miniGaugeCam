/* mini web cam logger */
/*more details to come */
//to do  
//lcd display is not working with this there seems to be an issue with communications, perhaps i2c?
//done // unsinged long integers need to be tested for file counter it needs to
//done //count up to 99,0000 and ints cannot do that                           
//done //problem is that int cannot hold value greater than 32k
//add power on and off for camera during photo cycle
// add quick filefinder algorithm to determine last file efficiently at startup
//use unix clock to streamline time keeping
//implement low-power code to improve efficiency
//determine power consumption
//write R code to rename and archive files--could do in any language. R seems lingua franca
//2-18-2013 added code for battery monitor--V+ up to 14.5ish Volts
//requires hardware addition of voltage divider 10k=R1 .808K=R2.


//Revision history
//7/25/2013 kad commented out fast file finding algorithm. It does not work if there are no files on a disk.
//              updated version to 0.54
//7/30/2013 KAD added EEprom read to idenfity Arduino board. This may help with identifying files  or records
//              updated SD write function to include this information
//              updated version to 0.55
//TODO revise fast find algorithm for success on empty cards
//



#include <Wire.h>                  //i2c library for talking with RealTime Clock
#include <Adafruit_VC0706.h>       //CVCO706 web cam library
#include <SD.h>                    //SD card library
#include <Chronodot.h>             //RealTime Clock library
#include <SoftwareSerial.h>        //To talk serial over digital channels used for camera
//#include <LowPower.h>              //Enable power saving sleep or idle mode
#define chipSelect 10              //enable writing to SD card. SS is 10 for adafruit card
#include <EEPROM.h>              //library to write to EEPROM to keep cumulative flow data if power is lost etc
#include "EEPROMAnything.h"      //a data stucture to facilitate writing to EEPROM
//#include <JeeLib.h>

//declare objects:
Chronodot RTC;                                             //DS3231 or similar clock for keeping time, getting temperature, setting alarms
SoftwareSerial cameraconnection = SoftwareSerial(2, 3);    //this is the wire pair used to talk to the camera
Adafruit_VC0706 cam = Adafruit_VC0706(&cameraconnection);  //the camera
 
 
//ISR(WDT_vect) { Sleepy::watchdogEvent(); } 
/**************************************************************************************************/
/**************************************************************************************************/
/**************************************************************************************************/
/**        Set Parameters for Data Logger  */
/**/         int sampleInterval;           //The minutes between samples. It is now determined by 
/**/                                       //reading the jumpers attached to DIP switch
/**/                                       // and can have a vale of 1, 5, 10, 15, 20, 30, 60 or 360
/**/                                           // 123 SRpin# # (in code)
/**/                                           // 123 dip#     (on board_
/**/                                           // 000  1 minute interval  60 rapid flashes
/**/                                           // 100  5 minute interval  12 flashes
/**/                                           // 010  10 minute interval 6 flashes
/**/                                           // 110  15 minute interval 4 flashes
/**/                                           // 001  20 minute interval 3 flashes
/**/                                           // 101  30 minute interval 2 flashes
/**/                                           // 011  1 hour interval    1 flash
/**/                                           // 111  6 hour interval    3 slow flashes
/**/             char ArduinoID[5]="0000";        //read this value from EEPROM for use in identification of equipment
/**/       const char siteID[4]="SWL";     //where logger is located 3 digit identifier
/**/       const float GCVersion=0.65;     //version of software to be printed in log. lets user know whether curren or not from log
/**/       const uint8_t imageSize=VC0706_640x480; //define image size (biggest is the only reasonable option
/**/                                       //VC0706_640x480;// biggest
/**/                                       //VC0706_320x240;// medium
/**/                                       //VC0706_160x120;// small        
/**/       long int LastFile=0;            //keep last file name number in memory so we don't have to
/**/                                       //search for it each time we write a file
/**/       int powerCycle=1;               //use to keep track of when power cycles are happening--has power been lost in the field?
/**/       int error;                      //keep track of how many problems come up in startup, and flash them.
 /**/                                          //no card
/**/                                           //no camera
/**/                                           //low battery?
/**/
/**//*                   INPUT PINS                                                             */
/**/        const int analogInPin = A3;   // Analog input pin for batterty voltage
/**/        const int SRpin3=5;          //Digital input of DIP switch position to determine sample rate
/**/        const int SRpin2=6;          //
/**/        const int SRpin1=7;          //
/**/
/**//*                   OUTPUT PINS                                                             */
/**/        const int cameraPower=4;      //digital output pin to switch power for camera
/**/        const int illuminatorRing=9;
/**/        const int blinkOutput=8 ; //use output on sampe pin for all visual output
/**/
/**/ /*                                                                                          */
/**************************************************************************************************/
/**************************************************************************************************/
/**************************************************************************************************/




void setup() { //Serial.begin(9600);
  
        //Start essential services
                Wire.begin();                        //Turn on i2c bus for communicating with RTC
        	RTC.begin();                         //Turn the RT clock interface on
        	
        //Initialize Arduino data and control outputs
                pinMode(blinkOutput,OUTPUT);         //Use this pin to send visual feedback to LED by SDCard
                                                     //Used for errors, sample rate, and countdown
                pinMode(10, OUTPUT);                 //Enable SD card SS
                pinMode(cameraPower,OUTPUT);         //Enable IO control of camera FET
                pinMode(illuminatorRing,OUTPUT);     //Enable IO control of illuminator ring FET
                
         //Initialize Arduino data inputs
                pinMode(SRpin3,INPUT);	             //3 pins are read to determine sample interval	  
                pinMode(SRpin2,INPUT);
        	pinMode(SRpin1,INPUT);		  
        	pinMode(analogInPin,INPUT);	     //Analog pin for reading battery voltage via voltage divider
                                                     //R1=10kOhm R2=768Ohm
        	analogReference(INTERNAL);           //1.1v internal reference for voltage
        	
        //Check attached hardware
              // Check SD card status
              digitalWrite(cameraPower,HIGH);
              delay(3000);
              if (!SD.begin(chipSelect)) {
        	        error++;
        //Serial.println("card error");
                      //  return;
                        }
               //Check Camera status
               //digitalWrite(cameraPower,HIGH);        //Turn on Camera Power
               //delay(1000);                           //Wait for it to warm up
               if (!cam.begin()) {                    //If the camera doesn't start
                        error++;       
                     //Serial.println("camera error");               //there is a problem, add it to error count
                      //  return;
                        }
               digitalWrite(cameraPower,LOW);         //Turn Camera Power off 
               
        //Read sample interval from pins
        	sampleInterval = ReadSampleIntervalDIP();
        //Read Arduino ID# from EEPROM
                EEPROM_readAnything(0,ArduinoID);
        //Read SD Card for last image filename
        //      getLastFile(); //may cause p;roblems if there are no files present?
                }
//end setup

 void loop() {
//If there are errors set the output light to blinking and go no further
      while(error>=1){
             BlinkOutput(1,100,500); 
             }
//Otherwise enter sample cycle
     DateTime now = RTC.now();         //start clock object "now"
     long int timeToNextSample = TimeToNextSample(sampleInterval,now.unixtime()); //Calculate time to next sample
      
if (timeToNextSample>6){              //If it is going to be a while take a long nap
     BlinkOutput(1,100,100);               //Give a little feedback
     //Sleepy::loseSomeTime(5000);
     delay(5000);
    }
      
else if (timeToNextSample>3){          //If is going to be a short while, take a short nap
     BlinkOutput(1,100,100);
     //Sleepy::loseSomeTime(1000);
     delay(1000);
      }
      
else if(timeToNextSample<3){//If the time to the next picture is very short, give some visual feedback, and take a picture
      long int countdown=millis();
      while(millis()-countdown<timeToNextSample*1000){//Delay until time has arrived
         if (millis()%200==0){                    //every 100ms make a flash
                  BlinkOutput(1,10,10);
                  }//end if(millis()...            
                  }//end while(millis()...)
         //Take the picture
         //Serial.println("take a picture");
         digitalWrite(cameraPower,HIGH);           //Turn power to camera on
         //Sleepy::loseSomeTime(1000); 
          delay(5000);
         //Wait for camera to do any adjusting
        //cam.setImageSize(imageSize);             //Set the picture size (defined above)
         digitalWrite(illuminatorRing,HIGH);       //Turn on the lights
         //Sleepy::loseSomeTime(1000);          
         delay(1000);         //Delay to allow camera to adjust image to illumination
         snap();  //Call subroutine to record the picture to SD Card, and write log file
                                  
         powerCycle=0; //If power has not been reset, it will stay at 0. When reset, the first picture will have a value of 1 in the logfile for PS.
         digitalWrite(cameraPower,LOW);
          }//end time to next sample <5s

} //end void loop


//Function to calculate time to next sample:
//It takes the variables Sample Rate (SR) and unixtime (time in seconds since 00:00:01 01/01/1970
//It returns a long integer with the seconds to next sample

long int TimeToNextSample(int SR, long int unixTime) {
                long int timetoNextSample=(SR*60)-unixTime%(SR*60); //Uses modulo operator (%) to calculate remainder of unixTime divided by sample rate in seconds (SR*60)                  
                                                                    //Subtracting this from the Sample rate in seconds give the number of seconds to wait 
                return timetoNextSample;  
             }


//Function to read the sample rate from the DIP switches on the control board

int ReadSampleIntervalDIP() {
      int sampleFrequency=15;//we set a default value here, but probably don't need to. If the pins aren't working...
  
      int logic[3]={digitalRead(SRpin3),digitalRead(SRpin2),digitalRead(SRpin1)}; //Read the pins into a 3 item array
 
       //Use simple conditional test to see what the pins are set to, and assign a sampleFrequency
               if ((logic[0]==0) && (logic[1] == 0) && (logic[2]==0)){
                  sampleFrequency=1; //at 1 minute intervals
                  }
                else if ((logic[0]==0) && (logic[1] == 0) && (logic[2]==1)){
                  sampleFrequency=5; //at 5 minute intervals
                  }
                else if ((logic[0]==0) && (logic[1] == 1) && (logic[2]==0)){
                  sampleFrequency=10; //at 10 minute intervals
                  }
                
               else if ((logic[0]==0) && (logic[1] == 1) && (logic[2]==1)){
                  sampleFrequency=15; //at 15 minute intervals
                  }
               else if ((logic[0]==1) && (logic[1] == 0) && (logic[2]==0)){
                  sampleFrequency=20; //at 20 minute intervals
                  }  
                
               else if ((logic[0]==1) && (logic[1] == 0) && (logic[2]==1)){
                  sampleFrequency=30; //at 30 minute intervals
                  } 
               else if ((logic[0]==1) && (logic[1] == 1) && (logic[2]==0)){
                  sampleFrequency=60; //at 60 minute intervals
                  }
               else if ((logic[0]==1) && (logic[1] == 1) && (logic[2]==1)){
                  sampleFrequency=360; //at 6 hour intervals
                   }
  
        int delayTime=4000/(60/sampleFrequency*3);//The time to wait between flashes
        if (sampleFrequency<100){                 //If sample Frequency is not 6 hours
                  BlinkOutput(60/sampleFrequency,delayTime*2,delayTime);  //Blink like this
            }
        else{                                      //If sample frequency is 6 hours
                  BlinkOutput(3,1000,100);         //Blink like this (3 times 1000ms on 100ms off)
            }
                 //Serial.println(sampleFrequency);                              
        return sampleFrequency;                //return the value 
}//end ReadSampleIntervalDIP()



//Function that calls the camera to take a picture, and store it on the SD Card
void snap(){
  //Get things ready
      DateTime now = RTC.now();              //Get the time for picture creation
      SD.begin(chipSelect);
      cam.setImageSize(imageSize);           //Set the picture size (defined above)
  //Take the picture
       if (!cam.takePicture()){              //If the picture doesn't work
               error++;                      //Add it to the error pile
               }
       digitalWrite(illuminatorRing,LOW);    //Turn the lights back off to save energy
  
  //Assemble a name for the file
     char filename[14];                         //build a file name
     sprintf(filename,"%s00000.JPG",siteID);    //use defined siteID in file names are limited to 8 char + 3 char extention
 
      for (long int i = LastFile; i < 99999; i++) {   // create new filename w/ 5 digits 00000-99999
            //calculate ten thousands                 //Relies on the LastFile determined in the Setup routine.
            long int v =i/10000;                      //the way it is written, it will tolerate there being random files with identical names, and skip over them.
            filename[3] = '0' + v;                  
            //calculate thousands                    
            long int w= (i-v*10000)/1000;
            filename[4] = '0' + w; 
            long int x = (i-((v*10000)+(w*1000)))/100;          // Calculates hundreds position
            filename[5] = '0' + x;         
            long int y = (i-((v*10000)+(w*1000)+(x*100)))/10;   // Calculates tens position
            filename[6] = '0' + y ;          
            long int z = i%10;                                  //  Calculate ones position
            filename[7] = '0' + z;         
            if (! SD.exists(filename)) {                        // If the filename hasn't been used yet, go for it
                  LastFile=i;                                   // Update   counter
                  break;
                }
            }
//Open a file, and determine the size of the file to store
      File imgFile = SD.open(filename, O_CREAT | O_WRITE);        // Open the file for writing
      uint16_t jpglen = cam.frameLength();                 // Get the size of the image (frame) taken  

//Collect data from camera save it to the open file buffer
    while (jpglen > 0) {                                    // Read all the data up to # bytes!
           uint8_t *buffer;                                 // read 32 bytes at a time;   
           uint8_t bytesToRead = min(32, jpglen);           // change 32 to 64 for a speedup but may not work with all setups!
           buffer = cam.readPicture(bytesToRead);
           imgFile.write(buffer, bytesToRead);

           jpglen -= bytesToRead;
            }
     imgFile.flush();
     imgFile.close();                                        //Close out the file, and release file handle

//Prepare an entry for the Data.txt file that keeps activity logs
     File dataFile=SD.open("imageLog.txt", FILE_WRITE);          //Open the log file
     if (dataFile){                                          //If it is open, write information to it
          double analogIn=analogRead(analogInPin);           //Read the analog input from voltage divider to determine battery voltage
          float voltage = (((analogIn)/1023)*14.1*1.1);      //Calculate battery voltage
          char conversion[10];                               //Make a container to convert the number to a string
          dtostrf(voltage,2,2,conversion);                   //Write it as a string
         char dataString[27];                                //Make a container to  hold the date time data from the clock
                                                             //Write datetime to container
         int a = sprintf(dataString,"%d\t%02d\t%02d\t%02d\t%02d\t%02d\t",now.year(),now.month(), now.day(),now.hour(),now.minute(),now.second());                         
                 dataFile.print(dataString);                 //Write data to file, append tab character, keep going
                 dataFile.print("\t");      
                 dataFile.print(filename);
                 dataFile.print("\t");
                 dataFile.print(ArduinoID);
                 dataFile.print("\t");
                 dataFile.print(voltage);
                 dataFile.print("\t");
                 dataFile.print(now.tempF()); 
                 dataFile.print("\t");
                 dataFile.print(GCVersion);
                 dataFile.print("\t");
                 dataFile.print(sampleInterval);
                 dataFile.print("\t");
                 dataFile.print(powerCycle);
                 dataFile.print("\n");                        //Add a newline character to the end
         
         dataFile.close();
         }  //end if(dataFile){...

      else {   // if the file isn't open, pop up an error:
            error++;
             } 
 
      cam.resumeVideo();                                     //Tell the camera that it can take more image frames

}//end snap()...

//Function to display status via LED blinks
//Requires 3 arguments, that are pretty self explanatory
void BlinkOutput(int numberOfFlashes, int durationOn, int durationOff){
        for (int i=0; i<numberOfFlashes;i++){  
                digitalWrite(blinkOutput,HIGH);
                delay(durationOn);
                digitalWrite(blinkOutput,LOW);
                delay(durationOff);
        }
}


//Function to figure out the last file on the SD Card
//Well, the last file with the format we care about
//Check for the highest integer in each of the number places in the filename. 
//Use them to recreate the highest image number name.
void getLastFile(){
     char filename[14];                         //build a file name
     sprintf(filename,"%s00000.JPG",siteID);
     int i,j,k,l,m;
        for ( i=9; i>=0; i--){
            filename[3]='0'+ i;
            if (SD.exists(filename)){
              for (j=9; j>=0; j--){
                  filename[4]='0'+j;
                  if (SD.exists(filename)) {
                      for ( k=9; k>=0;k--){
                          filename[5]='0'+ k;
                           if (SD.exists(filename)){
                                for ( l=9; l>=0; l--){
                                 filename[6]='0'+l;
                                  if (SD.exists(filename)){
                                       for (m=9; m>=0; m--){
                                           filename[7]='0'+m;
                                            if (SD.exists(filename)){
                                               break;
                                            }
                                        }  
                                  break; 
                                  }
                                }
                           break;    
                           }
                      }
                 break;   
                 }
              }
           break; 
           }
        }
LastFile=i*10000*j*1000+k*100+l*10+m; //This is the long integer that represents the last file found on the disk.
}
