/*
 * PDM (PiDoorMan) Raspberry Pi Access Control System
 * Version 1.0
 * By Ben Kent
 * 11/08/2012
 * Based on an Arduino sketch by Daniel Smith: www.pagemac.com
 * Depends on the wiringPi library by Gordon Henterson: https://projects.drogon.net/raspberry-pi/wiringpi/
 *
 * The Wiegand interface has two data lines, DATA0 and DATA1.  These lines are normall held
 * high at 5V.  When a 0 is sent, DATA0 drops to 0V for a few us.  When a 1 is sent, DATA1 drops
 * to 0V for a few us. There are a few ms between the pulses.
 *   *************
 *   * IMPORTANT * 
 *   *************
 *   The Raspberry Pi GPIO pins are 3.3V, NOT 5V. Please take appropriate precautions to bring the
 *   5V Data 0 and Data 1 voltges down. I used a 330 ohm resistor and 3V3 Zenner diode for each
 *   connection. FAILURE TO DO THIS WILL PROBABLY BLOW UP THE RASPBERRY PI!
 *
 * The wiegand reader should be powered from a separate 12V supply. Connect the green wire (DATA0) 
 * to the Raspberry Pi GPIO 0(SDA) pin, and the white wire (DATA1) to GPIO 1 (SCL).  
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <wiringPi.h>
#include <mysql.h>
#include <time.h>

#define D0_PIN 0
#define D1_PIN 1
#define ALLOWED_PIN 17 						// I'm pretty sure green is pin 17, red pin 21
#define DENIED_PIN 21

#define MAX_BITS 100                 		// max number of bits 
#define WIEGAND_WAIT_TIME 300000      		// time to wait for another wiegand pulse.  

static unsigned char databits[MAX_BITS];    // stores all of the data bits
static unsigned char bitCount;              // number of bits currently captured
static unsigned int flagDone;               // goes low when data is currently being captured
static unsigned int wiegand_counter;        // countdown until we assume there are no more bits

static unsigned long facilityCode=0;        // decoded facility code
static unsigned long cardCode=0;            // decoded card code

FILE *fr;									// config file
static char locationID[2];					// id of location
static char mysqlIP[16];					// ip of the mysql server
static char mysqlUser[16];					// username of the mysql server
static char mysqlPass[16];					// password of the mysql server

PI_THREAD (waitForData0)
{
	(void)piHiPri (10) ;	// Set this thread to be high priority
	  
	for (;;)
	{
		if (waitForInterrupt (D0_PIN, -1) > 0)	// Got it
		{
			//printf ("0") ; fflush (stdout) ;
			bitCount++;
			flagDone = 0;
			wiegand_counter = WIEGAND_WAIT_TIME;  
		}
	}
}

PI_THREAD (waitForData1)
{
	(void)piHiPri (10) ;	// Set this thread to be high priority
	  
	for (;;)
	{
		if (waitForInterrupt (D1_PIN, -1) > 0)	// Got it
		{
			//printf ("1") ; fflush (stdout) ;
			databits[bitCount] = 1;
			bitCount++;
			flagDone = 0;
			wiegand_counter = WIEGAND_WAIT_TIME;  
		}
	}
}

void setup(void)
{
	system ("gpio edge 0 falling") ;
	system ("gpio edge 1 falling") ;

	system ("gpio export 17 out") ;
	system ("gpio export 21 out") ;
	  
	// Setup wiringPi

	wiringPiSetupSys () ;

	//Because my wiring is currently wrong my LEDs are on when initilaised, so turn them off
	digitalWrite (ALLOWED_PIN, 1); // these should be 0 to turn off
	digitalWrite (DENIED_PIN, 1); // in fact if 0 was off, no need to turn off at startup...

	// Fire off our interrupt handler

	piThreadCreate (waitForData0) ;
	piThreadCreate (waitForData1) ;  

	wiegand_counter = WIEGAND_WAIT_TIME;
}

void getConfig()
{
	// path should reallu be /usr/local/bin - I'm happy to run in my home directory for now
	fr = fopen("/home/pi/CPrograms/pdmconf.txt","rt");

    if(fr == NULL){ printf("File not found");}

    fscanf(fr, "location: %[^\n] mysqlip: %[^\n] mysqluser: %[^\n] mysqlpass: %[^\n]",
    	locationID, mysqlIP, mysqlUser, mysqlPass);
		
	//printf("\nLocation=%s ip=%s user=%s pass=%s",locationID, mysqlIP, mysqlUser, mysqlPass);
	//fflush (stdout);
}

void printBits()
{
    // Prints out the results
    printf ("\nRead %d bits\n", bitCount) ; fflush (stdout) ;
    printf ("Facility Code: %d\n", facilityCode) ; fflush (stdout) ;
    printf ("TagID: %d\n", cardCode) ; fflush (stdout) ; 
    return;
}

void writeLog()
{
    // Writes the log to the MYSQL database
    MYSQL *con = mysql_init(NULL);

    if (con == NULL) 
    {
        fprintf(stderr, "%s\n", mysql_error(con));
	    return;
    }

    if (mysql_real_connect(con, mysqlIP, mysqlUser, mysqlPass, 
        "pidoorman", 0, NULL, 0) == NULL) 
    {
        fprintf(stderr, "%s\n", mysql_error(con));
        mysql_close(con);
	    return;
    } 

    char sqlselect[80];
   
    snprintf(sqlselect, sizeof sqlselect, "%s%d%s%d%s", "select id from pdm_cards where faccode='", facilityCode, "' and cardno='", cardCode, "'");
   
    if (mysql_query(con, sqlselect)) 
    {
        fprintf(stderr, "%s\n", mysql_error(con));
        mysql_close(con);
    }
  
    MYSQL_RES *result = mysql_store_result(con);
  
    if (result == NULL) 
    {
        //finish_with_error(con);
	    // An error has occur
	    fprintf(stderr, "%s\n", mysql_error(con));
	    mysql_close(con); 
    }
	
	char accresult;
    int num_rows = mysql_num_rows(result);
  
    if (num_rows > 0)
    {
	    // access allowed
	    digitalWrite (ALLOWED_PIN, 0); //turn LED (lock) on (0 and 1 back to front here...)
		accresult = 'A'; // allowed = A, denied = D. Other chars like S for schedule error etc.
    }
    else
    {
        // access denied
	    digitalWrite (DENIED_PIN, 0);
		accresult = 'D';
    }
   
    time_t rawtime;
    struct tm *info;

    time( &rawtime );

    info = localtime( &rawtime );

    char timestamp[80];
    char sqlinsert[80];

    // work out the current timestamp in the format YYYY-mm-dd hh:mm:ss
    // remember that the rpi has no hwc, so the time is utc if connected to the internet, random if not.
   
    strftime(timestamp,80,"%Y-%m-%d %X", info);
    printf("Timestamp: %s\n", timestamp );

    snprintf(sqlinsert, sizeof sqlinsert, "%s%d%s%d%s%s%s%c%s%s%s", "insert into pdm_logs values(null,'", facilityCode, "','", cardCode, "','", timestamp, "','", accresult, "','", locationID, "')");

    if (mysql_query(con, sqlinsert)) 
    {
        fprintf(stderr, "%s\n", mysql_error(con));
        mysql_close(con);
    }
 
    mysql_free_result(result); 
    mysql_close(con);

    delay(1000);

    digitalWrite (ALLOWED_PIN, 1); //turn LED (lock) off (0 and 1 back to front here...)
    digitalWrite (DENIED_PIN, 1); // turn both off
    return;
}

int main(void)
{
	setup();
	getConfig();

	unsigned char i;

	//char useSiteCode = '\0'; // determines whether or no to use 26 bit site code.
	char useSiteCode = 'Y';

	int validInput;

	printf ("\nPidoorman v1.0\nWritten for Raspberry Pi\nBy Ben Kent\n30/04/2013\n\n") ; fflush (stdout) ;

	validInput = 0;
	/*
	while( validInput == 0 ) {
		printf("Use standard site code for 26bit numbers (Y/N)?\n");
		scanf("  %c", &useSiteCode );
		useSiteCode = toupper( useSiteCode );
		if((useSiteCode == 'Y') || (useSiteCode == 'N'))
		validInput = 1;
		else  printf("\nError: Invalid choice\n");
	}
	*/
 
    //printf("\n%c was your choice.\n", useSiteCode ); fflush (stdout) ;
    printf("\nReady.\nPresent card:\n"); fflush (stdout) ;
 
 
    for (;;)
    { 
			// This waits to make sure that there have been no more data pulses before processing data
			if (!flagDone) {
				if (--wiegand_counter == 0)
					flagDone = 1;	
		}
	  
		// if we have bits and the wiegand counter reached 0
		if (bitCount > 0 && flagDone) {
			//unsigned char i;
			
			//Full wiegand 26 bit
			if (bitCount == 26 & useSiteCode == 'N')
			{
			//unsigned char i;

			  // card code = bits 2 to 25
				for (i=1; i<25; i++)
				{
					cardCode <<=1;
					cardCode |= databits[i];
				}
			  
				printBits();
				writeLog();
			}
			else if (bitCount == 26 & useSiteCode == 'Y')
			{
				// standard 26 bit format with site code
				// facility code = bits 2 to 9
				for (i=1; i<9; i++)
				{
					facilityCode <<=1;
					facilityCode |= databits[i];
				}
			  
				// card code = bits 10 to 23
				for (i=9; i<25; i++)
				{
					cardCode <<=1;
					cardCode |= databits[i];
				}
			  
				printBits();
				writeLog(); 
			}
			else if (bitCount == 35)
			{
				// 35 bit HID Corporate 1000 format
				// facility code = bits 2 to 14
				for (i=2; i<14; i++)
				{
					facilityCode <<=1;
					facilityCode |= databits[i];
				}
			  
				// card code = bits 15 to 34
				for (i=14; i<34; i++)
				{
					cardCode <<=1;
					cardCode |= databits[i];
				}
			  
				printBits(); 
				writeLog();    
			}
			else if (bitCount == 37)
			{
				//HID Proprietary 37 Bit Format with Facility Code: H10304
				// facility code = bits 2 to 17
				for (i=2; i<17; i++)
				{
					facilityCode <<=1;
					facilityCode |= databits[i];
				}
			  
				// card code = bits 18 to 36
				for (i=18; i<36; i++)
				{
					cardCode <<=1;
					cardCode |= databits[i];
				}
				  
				printBits(); 
				writeLog();    
			}
			else {
				// Other formats to be added later.
				printf ("Unknown format\n") ; fflush (stdout) ; 
				//printf ("Wiegand Counter = %d\n", wiegand_counter) ; fflush (stdout) ;
				//printf ("Flag done = %d\n", flagDone) ; fflush (stdout) ;
			}
			
			// cleanup and get ready for the next card
			bitCount = 0;
			facilityCode = 0;
			cardCode = 0;
			for (i=0; i<MAX_BITS; i++) 
			{
				databits[i] = 0;
			}
		}
	}
	return 0 ;
}