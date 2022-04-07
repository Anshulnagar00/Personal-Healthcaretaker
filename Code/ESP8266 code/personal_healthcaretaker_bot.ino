/********************************* Personal health-caretaker bot ESP8266 Source code ******************************/
/* Contributors :- Anshul Nagar, Himanshi Agrawal, Sazal Jain, Somya Jain, Soumya Patel                           */
/* 4th year Major project EC department SGSITS Indore batch 2018-22                                               */
/******************************************************************************************************************/

// Import required libraries

#include <SPI.h>
#include <Wire.h>
#include <GyverOLED.h>
#include <ESP8266WiFiMulti.h> 
#include <ESP8266mDNS.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <Firebase_ESP_Client.h>  // Use version 2.5.5 if facing issues with newer version
#include <ESP8266HTTPClient.h>
#include <DFRobot_MLX90614.h>
#include "MAX30100_PulseOximeter.h"
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>
#include "ESP8266WiFi.h"
#include <EEPROM.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>

ESP8266WebServer server(80); // Initialize Server at port 80

#define MLX90614_IIC_ADDR 0x5A // Setting up the temperature sensor
DFRobot_MLX90614_IIC mlx(MLX90614_IIC_ADDR, &Wire); // Create an instance of the DFRobot_MLX90614_IIC class, called 'mlx'

const uint8_t clock_bm[] = {0x02, 0x39, 0x44, 0x54, 0x44, 0x39, 0x02};

ESP8266WiFiMulti wifiMulti;     // Create an instance of the ESP8266WiFiMulti class, called 'wifiMulti'

const long utcOffsetInSeconds = 19800;  // Setting up the timezone shift (19800 For Indian Standard Time)  

char daysOfTheWeek[8][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"};

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);

GyverOLED<SSH1106_128x64> oled;


#define API_KEY "AIzaSyCF0Rg66qdvLZ4tqxiwiLgf1gDp994TIZk"  // Define the Web API Key of firebase project
#define DATABASE_URL "https://personal-healthcaretaker-default-rtdb.firebaseio.com/"  // Define the Database url of firebase project

//SuperUser Account access [KEEP BELOW DETAILS HIDDEN AND PRIVATE]
#define USER_EMAIL "{{ put your firebase project admin account email here }}"
#define USER_PASSWORD "{{ put your firebase project admin account password here }}"

// Define the GPIO pins Connections 
#define SNOOZE_BTN 14
#define NAV_BTN1 13
#define NAV_BTN2 12
#define BUZZER 0
#define ANALOG_IP A0

// Define the device default path must be different for each device
#define DEVICE_ID "Devices/A00010222"

// Create an instance of the PulseOximeter class, called 'pox'
PulseOximeter pox;

// Create instances of the various Firebase class variables
FirebaseData fbdo,fbdor,check_st;  // Stores the data recieved from firebase 
FirebaseJson json; // json variable instance for handling JSON data
FirebaseJsonData jsonObj; // Firebase json variable instance for handling recieved JSON data
FirebaseAuth auth; // FirebaseAuth class instance for handling authentication data
FirebaseConfig config; // FirebaseConfig class instance for handling configurations data

// Function Defination of updateAndDisplayAtime function
void updateAndDisplayAtime(bool);

// Define Global Variables 
uint32_t tsLastReport = 0; // Time counter variable for taking Pulse oximeter reading
int snz_btn_press_count = 0; // Holds the count of number of times medicine taken button is pressed resets when it increment 
bool alarm_st = false; // True if medicine time alarm is ringing 
int med_time[3]; // Holds the value of next medicine alarm time value index 0 = hrs index 1 =  min & index 2 = day
int nav_opt = 0; // Contains the current option no. selected  
bool onlaunch_st = true; // True in the begining when device is turned on used in updateAndDisplayAtime function as parameter
String Ssid =""; // Holds the SSID of wifi connection 
String Admin_email=""; // Holds the email id of device admin

void configureConnections(){ 
  /*
  Input : none
  Output : none
  Description :
  Reads the wifi configurations SSID and Password stored in device's EEPROM memory and sets it to connect the particular network  
  */
  String esid;
  for (int i = 0; i < 32; ++i)
  {
    esid += char(EEPROM.read(i)); // read data from EEPROM memory one char at a time
  }
  Serial.println();
  Serial.print("SSID: ");
  Serial.println(esid);
  Serial.println("Reading EEPROM pass");
  Ssid = esid.c_str(); // convert it to string and store it to global variable Ssid
  String epass = "";
  for (int i = 32; i < 96; ++i)
  {
    epass += char(EEPROM.read(i)); // read data from EEPROM memory one char at a time
  }
  Serial.print("PASS: ");
  Serial.println(epass);
  wifiMulti.addAP(esid.c_str(), epass.c_str()); // set the wifi configuration 
}

bool setAdmin(){
  /*
  Input : none
  Output : [bool] returns true if the email is set successfully as device admin else false
  Description :
  Reads the user email from EEPROM memory converts it to a key and check for the email registered if 
  it is a registered email then sets it as device admin email and returns true else it fails to set 
  the user email as device admin and returns false
  */
  String euid = "";
  for (int i = 96; i < 200; ++i)
  {
    euid += char(EEPROM.read(i));   // Read data from EEPROM memory
  }
  Admin_email = euid.c_str();
  Serial.print("DEVICE ADMIN: ");
  Serial.println(euid);
  euid = euid.c_str();
  euid.toLowerCase();
  euid.replace(".","-{DOT}-");
  euid.replace("@","-{AT}-");
  if (Firebase.RTDB.getString(&fbdor,String("Email_IDs/")+euid)){
     Serial.println(euid);
     Serial.println("[DEBUG] "+String(fbdor.to<String>())); 
     if(fbdor.to<String>() != "null"){  // Checks if the given email is registered in app or not
        if (Firebase.RTDB.setString(&fbdo,String(DEVICE_ID)+String("/Users/Admin"),fbdor.to<String>())){  // Sets the device admin email 
           // If Success Display the status
           oled.clear();
           oled.setCursor(0,0);
           oled.setScale(1);
           oled.println("Device Owner(ADMIN) set to :");
           oled.println(Admin_email);
           oled.update();
           delay(3000); 
           return true; 
        } 
     }
  }
  return false;
}
void setup()   {
  EEPROM.begin(512); // Setup EEPROM of size 512 bytes
  WiFi.mode(WIFI_STA); // Setup Wifi 
  WiFi.disconnect(); // Forgot all the previous connections to device wifi (Stored in EEPROM by WiFi library)
  oled.init(); // Set up OLED
  oled.clear();   
  oled.update();  
  oled.home();
  oled.autoPrintln(true); 
  // Automatically adds new line character in oled print statements when line character limit is reached
  Serial.begin(9600); // Begin serial comm mainly used for debugging purpose
  delay(10);
  Serial.println('\n');
  // Defining GPIO's mode of operation
  pinMode(SNOOZE_BTN, INPUT);
  pinMode(NAV_BTN1, INPUT);
  pinMode(NAV_BTN2, INPUT);
  pinMode(BUZZER, OUTPUT);
  oled.setScale(2);
  oled.setCursor(5, 1);
  oled.println("Hello !");
  delay(1000);
  oled.setScale(1);
  oled.invertText(true);// 'inverted' text
  oled.println("I'm a Caretaker Bot  at your service"); 
  oled.update();
  delay(2000);
  oled.clear();
  oled.home();
  oled.setScale(2);
  oled.invertText(false);

  wifiMulti.addAP("reset", "reset@123");  // Add permanent wifi configuration to set other wifi connection and device admin
  configureConnections(); // Configuring the wifi connections
  
  oled.print("Connecting"); 
  Serial.println("Connecting ...");
  oled.update();
  int i = 0;
  while (wifiMulti.run() != WL_CONNECTED) { // Wait for the Wi-Fi to connect: scan for Wi-Fi networks, and connect to the strongest of the networks above
    oled.print(".");
    oled.update(); 
    delay(250);
    Serial.print('.');
  }

  launchWeb(); // Sets up and starts the Local ESP8266 web server
  
  server.handleClient(); // Handles incoming requests to Local ESP8266 web server
  Serial.println('\n');
  oled.clear();
  oled.setCursor(0, 0);
  oled.setScale(1);
  Serial.print("Connected to ");
  oled.print("Connected to "); 
  Serial.println(WiFi.SSID());
  oled.print(WiFi.SSID()); // Tell us what network we're connected to
  Serial.print("IP address:\t");
  oled.print("\nIP address:\t");  
  Serial.println(WiFi.localIP());   
  oled.println(WiFi.localIP());  // Display the IP address of the ESP8266 web server
  oled.update();
  timeClient.begin();
  timeClient.update();
  delay(1000);
  if (MDNS.begin("esp8266")) {   // Start the mDNS responder for esp8266.local
    Serial.println("mDNS responder started");
  } else {
    Serial.println("Error setting up MDNS responder!");
  }

  //Set the size of WiFi rx/tx buffers in the case where we want to work with large data.
  fbdo.setBSSLBufferSize(1024, 1024);
  fbdor.setBSSLBufferSize(1024, 1024);

  //Set the size of HTTP response buffers in the case where we want to work with large data.
  fbdo.setResponseSize(1024);
  fbdor.setResponseSize(1024);
  
  //optional, set the decimal places for float and double data to be stored in database
  Firebase.setFloatDigits(2);
  Firebase.setDoubleDigits(6);
  
  Serial.printf("Firebase Client v%s\n\n", FIREBASE_CLIENT_VERSION);
  // Setting up firebase configurations 
  config.api_key = API_KEY;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  config.database_url = DATABASE_URL;
  config.token_status_callback = tokenStatusCallback;
  Firebase.begin(&config, &auth);  // Begin the firebase service using firebase project configurations and firebase superuser(admin) account authentication   
  Firebase.reconnectWiFi(true);
  check_st.setBSSLBufferSize(64,64); // set the size of HTTP response buffers for stream variable
  updateAndDisplayAtime(true); //sets and displays next medicine time
  String path = String(DEVICE_ID)+String("/Data"),altd ="";  
  json.clear().add("Stream_Update"," 0");
  if (Firebase.RTDB.updateNode(&fbdo,String(DEVICE_ID)+String("/Data/Metadata"),&json)){  
    // set up data stream at path "Devices/{{ Device ID }}/Data/Metadata/Stream_Update"
    delay(1000); 
    if (!Firebase.RTDB.beginStream(&check_st,path+String("/Metadata/Stream_Update")))  // begin the stream 
      Serial.printf("stream begin error, %s\n\n", check_st.errorReason().c_str());
    Firebase.RTDB.setStreamCallback(&check_st, stream_callback_handler, streamTimeoutCallback);  
    // Define stream callback means the function we want to run when data is change at the stream path in firebase database 
  }
  else{ // If failed then display error message
      oled.clear();
      oled.setCursor(0, 0);
      oled.setScale(1);
      Serial.print("Unable to connect to firebase ");
      Serial.print(fbdo.errorReason().c_str());
      oled.print("Unable to connect to firebase ");
      oled.print(fbdo.errorReason().c_str());
      oled.update();
      delay(3000); 
  }
  if(setAdmin()){}  // Sets device admin email id from EEPROM memory
  else{ // If failed then display error message
    oled.clear();
    oled.setCursor(0, 0);
    oled.setScale(1);
    oled.print("[ERROR] Please set a registered email id : ");
    oled.update();
    delay(3000);
  }
}


 
void launchWeb()
{
  /*
  Input : none
  Output : none
  Description :
  Starts the local web server 
  */
  Serial.println("");
  if (WiFi.status() == WL_CONNECTED)
    Serial.println("WiFi connected");
  Serial.print("Connected to "); 
  Serial.println(WiFi.SSID());
  Serial.print("Local IP: ");
  Serial.println(WiFi.localIP());
  Serial.print("SoftAP IP: ");
  Serial.println(WiFi.softAPIP());
  createWebServer();
  // Start the server
  server.begin();
  Serial.println("Server started");
}
 

 
void createWebServer()
{
  /*
  Input : none
  Output : none
  Description :
  Creates the local web server 
  */
 {
    server.on("/", []() {  // if '/' request is made
      String content = "<h1>Device ID : ";
      content += String(DEVICE_ID).substring(8);
      content += "</h1><h2>Settings</h2><p></p><h4>Wifi Settings</h4>";
      content += "</p><form method='get' action='setssidpass'><label>SSID: </label><input name='ssid' value='";
      content += Ssid;
      content += "' length=32><p></p><label>Password: </label><input name='pass' type='password' length=64><input value='Set' type='submit'></form>";
      content += "<p></p><h4>User Settings</h4></p><form method='get' action='setau'><label>Admin email id : </label><input name='auid' value='";
      content += Admin_email;
      content += "' length=104><input value='Set' type='submit'></form>";
      content += "</html>";
      server.send(200, "text/html", content);   // Send the Device Settings HTML page 
      Serial.print("[DEBUG] -- ");
      Serial.print(Ssid);
      Serial.print(Admin_email);
      
    });
    
    server.on("/setssidpass", []() { // if '/setssidpass' get request is made
      int statusCode = 404;
      String content = "";
      String qsid = server.arg("ssid");
      String qpass = server.arg("pass");
      // get the input SSID and password data from get request
      if (qsid.length() > 0 && qpass.length() > 0) { // if valid SSID and password 
        Serial.println("clearing eeprom");
        for (int i = 0; i < 96; ++i) {
          EEPROM.write(i, 0);
        } // Clearing the previously stored EEPROM data 
        Serial.println(qsid);
        Serial.println("");
        Serial.println(qpass);
        Serial.println("");
 
        Serial.println("writing eeprom ssid:");
        for (int i = 0; i < qsid.length(); ++i)
        {
          EEPROM.write(i, qsid[i]);
          Serial.print("Wrote: ");
          Serial.println(qsid[i]);
        } // Writting new SSID to EEPROM
        Serial.println("writing eeprom pass:");
        for (int i = 0; i < qpass.length(); ++i)
        {
          EEPROM.write(32 + i, qpass[i]);
          Serial.print("Wrote: ");
          Serial.println(qpass[i]);
        } // Writting new Password to EEPROM
        EEPROM.commit();  // Commit the changes to EEPROM
 
        content = "{\"Success\":\"saved to eeprom... reset to boot into new wifi\"}";
        statusCode = 200;
        server.send(statusCode, "application/json", content); // Send the success status JSON response
      } else { // Send the Error status JSON response
        content = "{\"Error\":\"404 not found\"}";
        statusCode = 404;
        Serial.println("Sending 404");
      }
      server.sendHeader("Access-Control-Allow-Origin", "*");
      server.send(statusCode, "application/json", content); 
 
    });

    server.on("/setau", []() {  // if '/setau' get request is made
      int statusCode = 404;
      String content = "";
      String auid = server.arg("auid");
      // get the input device admin email data from get request
      if (auid.length() > 0) {
        Serial.println("clearing eeprom");
        for (int i = 96; i < 200; ++i) {
          EEPROM.write(i, 0);
        } // Clearing the previously stored EEPROM data 
        Serial.println(auid);
        Serial.println("");
   
        Serial.println("writing eeprom ssid:");
        for (int i = 0; i < auid.length(); ++i)
        {
          EEPROM.write(i+96, auid[i]);
          Serial.print("Wrote: ");
          Serial.println(auid[i]);
        } // Writting new device admin email to EEPROM
        EEPROM.commit();  // Commit the changes to EEPROM
 
        content = "{\"Success\":\"saved to eeprom... reset to boot into new admin user config\"}";
        statusCode = 200;
        server.send(statusCode, "application/json", content); // Send the success status JSON response
      } else { // Send the error status JSON response
        content = "{\"Error\":\"404 not found\"}";
        statusCode = 404;
        Serial.println("Sending 404");
      }
      server.sendHeader("Access-Control-Allow-Origin", "*");
      server.send(statusCode, "application/json", content);
 
    });
  } 
}



void loop() {
  timeClient.update();
  server.handleClient(); // Handles incoming requests to Local ESP8266 web server
  if (wifiMulti.run() != WL_CONNECTED) { // Wait for the Wi-Fi to connect: scan for Wi-Fi networks, and connect to the strongest of the networks above
    oled.clear();
    oled.setCursor(1,0);
    oled.setScale(1);
    oled.println("Disconnected from wifi network ");
    oled.setScale(2);
    oled.print("Reconnecting");
    oled.update(); 
    delay(250);
    oled.print(".");
    oled.update();
    wifiMulti.run();
    delay(250);
    oled.print(".");
    oled.update();
    wifiMulti.run();
    delay(250);
    oled.print(".");
    oled.update();
    wifiMulti.run();
    Serial.print('.');
    return;
  }
  int pot_val = analogRead(ANALOG_IP);  // Read the navigaion knob potentiometer value 
  if( !alarm_st){ // check if alarm is not active
  oled.clear();
  oled.setCursor(1,0);
  oled.setScale(2);
  oled.print(timeClient.getFormattedTime());  // Display the current time 
  oled.setScale(1);
  oled.println(" "+String(daysOfTheWeek[timeClient.getDay()]).substring(0,3)); // Display the current day 
  oled.setScale(1);
  oled.println("");
  unsigned long epochTime = timeClient.getEpochTime();
  struct tm *ptm = gmtime ((time_t *)&epochTime); 
  String currentDate = String(ptm->tm_mday)+"-"+ String(ptm->tm_mon+1)+"-"+String(ptm->tm_year+1900);
  
  oled.println(currentDate);  // Display the current date
  oled.println("");
  oled.println("   "+ getTimeStr(med_time[0],med_time[1],0)+" "+daysOfTheWeek[med_time[2]]);
  oled.println("");
  oled.invertText(true); // Displaying the menu 
  if (pot_val < 60){  // If temperature reading option is selected
    nav_opt = 0;  // Set the 'nav_opt' value for temperature reading option
    oled.print("|<------------>     |");
    oled.println("`[ T. ]- P.O. - A.T. ");
  }
  else if (pot_val < 180){  // If pulse oximetry reading option is selected
    nav_opt = 1;  // Set the 'nav_opt' value for pulse oximetry reading option
    oled.print("|<------------>     |");
    oled.println("`T. -[ P.O. ] - A.T. ");
  }
  else if (pot_val < 220){  // If display alarm times option is selected
    nav_opt = 2;  // Set the 'nav_opt' value for display alarm times option
    oled.print("|   <------------>  |");
    oled.println("`P.O. -[ A.T. ] - M. ");
  }
  else if (pot_val < 290){  // If show last message/note option is selected
    nav_opt = 3;  // Set the 'nav_opt' value for show last message/note option
    oled.print("|     <------------>|");
    oled.println("` A.T. -[ M. ] - R.  ");
  }
  else if (pot_val < 400){  // If show last readings option is selected
    nav_opt = 4;  // Set the 'nav_opt' value for show last readings option
    oled.print("|     <------------>|");
    oled.println("` A.T. - M. -[ R. ]  ");
  }
  oled.println("");
  oled.invertText(false);
  oled.drawBitmap(2, 32, clock_bm,7, 7);
  oled.update();
  delay(25);
  oled.clear();
  
  }
  String path = String(DEVICE_ID)+String("/Data"),altd ="";

  if (timeClient.getHours() == med_time[0] && timeClient.getMinutes() == med_time[1] && timeClient.getDay() == med_time[2] && (!alarm_st)){ 
      alarm_st = true;  // If its an alarm activation time then activate the alarm and set alarm state 'alarm_st' to true
  }
  
  int N1_btnst = digitalRead(NAV_BTN1);  // read button input
  int N2_btnst = digitalRead(NAV_BTN2);  // read button input
  if (alarm_st){  // If alarm is active
      oled.clear();
      oled.setCursor(1,0);
      oled.setScale(2);
      oled.println("Take");
      oled.print("Medicines !");
      oled.update();  // Display take medicine message
      String path = String(DEVICE_ID)+String("/Data");
      int btnst = digitalRead(SNOOZE_BTN);
      if (btnst == HIGH) {  // Check for medicine despenser button press 
        // If button is pressed then increament the 'snz_btn_press_count' variable
        Serial.println("SN _BTN PRESSED");
        snz_btn_press_count+=1;
        delay(25);
      }
      btnst = digitalRead(NAV_BTN1); // Check for navigation button press 
      if (btnst == HIGH) {  // If navigation button is pressed
        Serial.println("NAV_BTN1 PRESSED");
        alarm_st = false;   // Set the alarm activation state to false means stop the alarm
        unsigned long epochTime = timeClient.getEpochTime();  // get current time
        struct tm *ptm = gmtime ((time_t *)&epochTime); 
        String currentDate = String(ptm->tm_mday)+"-"+ String(ptm->tm_mon+1)+"-"+String(ptm->tm_year+1900);
        json.clear().add("Status","Medicine skipped");  // add medicine skipped status to json data payload 
        json.add("Time",timeClient.getFormattedTime());  // add time to json data payload 
        json.add("Date",currentDate);  // add date to json data payload 
        digitalWrite(BUZZER,0);  // Disable the alarm buzzer
        oled.clear();
        oled.setCursor(1,0);
        oled.setScale(2);
        oled.println("Medicine");
        oled.println("skipped");
        oled.setScale(1);
        oled.println();
        oled.update();  // Display message the medicine was not taken or skipped 
        if(Firebase.RTDB.getInt(&fbdor,path + "/Metadata/Medicine_logs_CIV")){
            int data_index_v = fbdor.to<int>();  // Get the count of total medicine logs enteries stored in firebase database
            if (Firebase.RTDB.setJSON(&fbdo, path + "/Medicine_logs/"+data_index_v,&json)){ 
              // Upload the medicine skipped status json payload data to cloud database
              data_index_v+=1; // Increment the logs count
              if(Firebase.RTDB.setInt(&fbdor,path + "/Metadata/Medicine_logs_CIV",data_index_v)){ // Update the logs count to database 
                Serial.println("SUCCESSFULLY UPDATED");
              }
              else{
                Serial.println("FAILED");
              }
            }
            else
            {
              Serial.println("FAILED");
              Serial.println("REASON: " + fbdo.errorReason());
              Serial.println("------------------------------------");
              Serial.println();
            }
          }
          else
          {
            Serial.println("FAILED");
            Serial.println("REASON: " + fbdo.errorReason());
            Serial.println("------------------------------------");
            Serial.println();
          }         
          
          updateAndDisplayAtime(false);  //sets and display the next medicine time
      }
      
      else if (snz_btn_press_count > 0){  // Check if medicine despenser button is pressed or not 
          // If pressed
          snz_btn_press_count = 0; // reset the button press count
          alarm_st = false;  // Set the alarm activation state to false means stop the alarm
          unsigned long epochTime = timeClient.getEpochTime();  // get current time
          struct tm *ptm = gmtime ((time_t *)&epochTime); 
          String currentDate = String(ptm->tm_mday)+"-"+ String(ptm->tm_mon+1)+"-"+String(ptm->tm_year+1900);
          json.clear().add("Status","Medicine taken");  // add medicine taken status to json data payload 
          json.add("Time",timeClient.getFormattedTime());  // add time to json data payload 
          json.add("Date",currentDate);  // add date to json data payload 
          digitalWrite(BUZZER,0);  // Disable the alarm buzzer
          oled.clear();
          oled.setCursor(1,0);
          oled.setScale(2);
          oled.println("Medicine");
          oled.print("Taken");
          oled.setScale(1);
          oled.println();
          oled.update();  // Display medicine taken message 
          if(Firebase.RTDB.getInt(&fbdor,path + "/Metadata/Medicine_logs_CIV")){
            int data_index_v = fbdor.to<int>();  // Get the count of total medicine logs enteries stored in firebase database
            if (Firebase.RTDB.setJSON(&fbdo, path + "/Medicine_logs/"+data_index_v,&json)){  
              // Upload the medicine taken status json payload data to cloud database
              data_index_v+=1;  // Increment the logs count
              if(Firebase.RTDB.setInt(&fbdor,path + "/Metadata/Medicine_logs_CIV",data_index_v)){  // Update the logs count to database 
                Serial.println("SUCCESSFULLY UPDATED");
              }
              else{
                Serial.println("FAILED");
              }
            }
            else
            {
              Serial.println("FAILED");
              Serial.println("REASON: " + fbdo.errorReason());
              Serial.println("------------------------------------");
              Serial.println();
            }
          }
          else
          {
            Serial.println("FAILED");
            Serial.println("REASON: " + fbdo.errorReason());
            Serial.println("------------------------------------");
            Serial.println();
          }         
  
          updateAndDisplayAtime(false);  //sets and displays the next medicine time
      }
      else{ // If neither button is pressed
        digitalWrite(BUZZER,1);  // continue the alarm to sound keep buzzer active
      }
  }  
  else if (N1_btnst == HIGH) {  // If navigation select button is pressed when alarm is not active 
     Serial.println("NAV_BTN1 PRESSED");
     if (nav_opt == 0){  // If selected option is T. (Temperature reading)
        int cntv = 0; // counter variable for loop
        float temp_r_F = 0,temp_r_C = 0;  // storing temperature value in Fahrenhiet and in Celcius
        int reading_thresh = 0; // Not major shift in consequentive temperature readings

        // Setting temperature sensor parameters 
        mlx.setEmissivityCorrectionCoefficient(1.0);
        mlx.setIICAddress(0x5A);
        mlx.setMeasuredParameters(mlx.eIIR100, mlx.eFIR1024);  
        mlx.enterSleepMode();
        delay(50);
        mlx.enterSleepMode(false);  // Activating the temperature sensor 
        delay(200);
          
        while(cntv < 20){
          if (mlx.begin()){  // If temperature sensor is not active 
            oled.clear();
            oled.setCursor(1,0);
            oled.setScale(1);
            oled.println("[ERROR] Temperature Sensor Not Found");
            oled.update(); // Display error message
            delay(2000);
            break;
            Serial.println("Error connecting to MLX sensor. Check wiring.");
          }
          float temp_inst_C = round2(mlx.getObjectTempCelsius());  // Get object temperature readings in celsius
          float temp_inst_F = round2(temp_inst_C*9/5 + 32); 
          oled.clear();
          oled.setCursor(1,0);
          oled.setScale(1);
          oled.println("Taking Readings ...");
          oled.println("Temperature :");
          oled.setScale(2);
          oled.println(String(temp_inst_F)+"*F");
          oled.setScale(1);
          oled.println("Object Temp: "+String(temp_inst_C)+"*C");
          oled.update();
          // Increment the 'reading_thresh' variable if int value of current and previous temp reading i same 
          if (((int)temp_r_F == (int)temp_inst_F) && reading_thresh < 5) { reading_thresh += 1; }
          // Break the loop if last 5 consequentive temperature readings are almost same  
          else if (reading_thresh == 5) { temp_r_F = temp_inst_F; temp_r_C = temp_inst_C; break;}
          // reset 'reading_thresh' if change in temperature occurs
          else { reading_thresh = 0; temp_r_F = temp_inst_F; temp_r_C = temp_inst_C; }
          cntv+=1;  // Increment the loop counter variable 
          delay(100);  // wait for 0.1 sec for next reading 
        }

        bool store_to_cloud = false;  // holds bool option value to send reading to database or not
        cntv = 0;  // reset loop counter variable
        while(cntv < 15 && !mlx.begin()){  // run loop for 15 times if temperature sensor is active
          oled.clear();
          oled.setCursor(1,0);
          oled.setScale(1);
          oled.println("Temperature :");
          oled.setScale(2);
          oled.println(String(temp_r_F)+"*F");
          oled.setScale(1);
          oled.println("In Celcius : "+String(temp_r_C)+"*C"); // Display final temperature readings
          oled.setScale(1); 
          oled.println("Send to Cloud");  // Display send temperature readings to cloud option
          pot_val = analogRead(ANALOG_IP);  // read the navigation knob potentiometer value
          if (pot_val > 200){  // if Yes option is selected
            oled.setScale(1);
            oled.print("No  ");
            oled.setScale(2);
            oled.println("Yes");
            oled.update();
            store_to_cloud = true; // set 'store_to_cloud' to true to send data to cloud
          }
          else{  // if No option is selected
            oled.setScale(2);
            oled.print("No");
            oled.setScale(1);
            oled.println("  Yes");
            oled.update();
            store_to_cloud = false;  // set 'store_to_cloud' to false to not send data
          }
          cntv+=1;  // increment loop counter 
          delay(200); // wait for 0.2 seconds 
        }
        mlx.enterSleepMode(); // put the temperature sensor on sleep mode  
        delay(50);
        if(store_to_cloud){  // If want to send temperature readings data to cloud 
          unsigned long epochTime = timeClient.getEpochTime();
          struct tm *ptm = gmtime ((time_t *)&epochTime); 
          String currentDate = String(ptm->tm_mday)+"-"+ String(ptm->tm_mon+1)+"-"+String(ptm->tm_year+1900);
          json.clear().add("Time",timeClient.getFormattedTime());  // add time of reading to json data payload 
          json.add("Date",currentDate);  // add date of reading to json data payload 
          json.add("Temperature",temp_r_F);  // Add temperature reading in Fahrenhiet to json data payload 
          if(Firebase.RTDB.getInt(&fbdor,path + "/Metadata/Temperature_logs_CIV")){
            // Get the count of total temperature reading logs enteries stored in firebase database
            int data_index_v = fbdor.to<int>();
            if (Firebase.RTDB.setJSON(&fbdo, path + "/Temperature_logs/"+data_index_v,&json)){
              // Send the temperature log json data payload to firebase database 
              data_index_v+=1; // Increament the temperature logs count
              if(Firebase.RTDB.setInt(&fbdor,path + "/Metadata/Temperature_logs_CIV",data_index_v)){
                // Update the temperature logs count in database
                Serial.println("SUCCESSFULLY UPDATED");
                oled.clear();
                oled.setCursor(1,0);
                oled.setScale(1);
                oled.println("Recorded ");
                oled.println("successfully");
                oled.println("in cloud");
                oled.update();  // Display reading successfully recorded in cloud message
                delay(2000);
              }
              else{
                Serial.println("FAILED");
              }
            }
            else
            {
              Serial.println("FAILED");
              Serial.println("REASON: " + fbdo.errorReason());
              Serial.println("------------------------------------");
              Serial.println();
            } 
          }
          else
          {
            Serial.println("FAILED");
            Serial.println("REASON: " + fbdo.errorReason());
            Serial.println("------------------------------------");
            Serial.println();
          } 
        }
         
     }
     else if (nav_opt == 1){  // If selected option is P.O. (SPO2 and Heart rate reading)
       if (!pox.begin()){  // If pulse oximetry sensor is not active 
          oled.clear();
          oled.setCursor(1,0);
          oled.setScale(1);
          oled.println("[ERROR] Pulse oximetery sensor Not Found");
          oled.update();  // Display error message
          delay(2000);
          Serial.println("Error connecting to MAX sensor. Check wiring.");
       }
       else{
          int cntv = 0; // counter variable for loop
          uint32_t tsLastReport = millis();  // For storing time 
          float hrv = 0; // holds heart rate reading value 
          int bpv =0;  // holsd spo2 reading value
          oled.clear();
          oled.setCursor(1,0);
          oled.setScale(1);
          oled.println("Taking readings ...");
          oled.println("Please wait (generally take 10 seconds)...");
          oled.update(); // Display message 
          while(cntv < 10){
            pox.update();  // Setup sensor to take new reading 
            if (millis() - tsLastReport > 1000){ // adds delay of 1 sec between consequentive sensor readings 
              Serial.print("Heart rate:");
              hrv = pox.getHeartRate();  // get heart rate reading from sensor
              Serial.println(hrv);
              Serial.print("bpm / SpO2:");
              bpv = pox.getSpO2();  // get SPO2 reading from sensor
              Serial.print(bpv);
              Serial.println("%");
              tsLastReport = millis(); 
              cntv+=1; // increment the count
            }  
          }
          pox.shutdown();  // put sensor on sleep 
          bool store_to_cloud = false; // holds bool option value to send reading to database or not
          cntv = 0;  // reset the counter variable for next loop
          while (cntv < 20){
            oled.clear();
            oled.setCursor(1,0);
            oled.setScale(1);
            oled.print("Heart rate:");
            oled.println(hrv);
            oled.print("bpm / SpO2:");
            oled.print(bpv);
            oled.println("%");  // Displays the final heart rate and SPO2 readings 
            oled.println("Send to Cloud");  // Display options to send reading to cloud
            pot_val = analogRead(ANALOG_IP); // read the navigation knob potentiometer value
            if (pot_val > 200){  // if Yes option is selected
              oled.setScale(1);
              oled.print("No  ");
              oled.setScale(2);
              oled.println("Yes");
              oled.update();
              store_to_cloud = true;  // set 'store_to_cloud' to true to send data to cloud
            }
            else{  // if No option is selected
              oled.setScale(2);
              oled.print("No");
              oled.setScale(1);
              oled.println("  Yes");
              oled.update();
              store_to_cloud = false;  // set 'store_to_cloud' to false to not send data
            }
            cntv+=1;  // Increment the loop counter 
            delay(200);  // add delay of 0.2 seconds
          }
          if(store_to_cloud){  // If want to send sensor readings data to cloud 
            unsigned long epochTime = timeClient.getEpochTime();
            struct tm *ptm = gmtime ((time_t *)&epochTime); 
            String currentDate = String(ptm->tm_mday)+"-"+ String(ptm->tm_mon+1)+"-"+String(ptm->tm_year+1900);
            json.clear().add("Time",timeClient.getFormattedTime());  // add time of reading to json data payload 
            json.add("Date",currentDate); // add date of reading to json data payload 
            json.add("Heart rate",hrv);  // add heart rate reading to json data payload 
            json.add("Spo2",bpv); // add spo2 reading to json data payload 
            if(Firebase.RTDB.getInt(&fbdor,path + "/Metadata/Pulse_Oximeter_logs_CIV")){
              // Get the count of total pulse oximetry reading logs enteries stored in firebase database
              int data_index_v = fbdor.to<int>();
              if (Firebase.RTDB.setJSON(&fbdo, path + "/Pulse_Oximeter_logs/"+data_index_v,&json)){
                // Send the pulse oximetey readings data json payload to firebase database
                data_index_v+=1; // Increament the pulse oximetry logs count
                if(Firebase.RTDB.setInt(&fbdor,path + "/Metadata/Pulse_Oximeter_logs_CIV",data_index_v)){
                  // Upadate the pulse oximetry logs count in database 
                  Serial.println("SUCCESSFULLY UPDATED");
                  oled.clear();
                  oled.setCursor(1,0);
                  oled.setScale(1);
                  oled.println("Recorded ");
                  oled.println("successfully");
                  oled.println("in cloud");
                  oled.update();  // Display success message
                  delay(2000);      
                }
                else{
                  Serial.println("FAILED");
                }
              }
              else
              {
                Serial.println("FAILED");
                Serial.println("REASON: " + fbdo.errorReason());
                Serial.println("------------------------------------");
                Serial.println();
              } 
            }
            else
            {
              Serial.println("FAILED");
              Serial.println("REASON: " + fbdo.errorReason());
              Serial.println("------------------------------------");
              Serial.println();
            } 
          }
       }
      
      
     }
     else if (nav_opt == 2){  // If selected option is A.T. (Display all medicine alarm times)
         String path = String(DEVICE_ID)+String("/Data"),altd ="";
         if (Firebase.RTDB.getString(&fbdor,path + "/medicine alarm time")){
            // Get the medicine alarm times stored in firebase database
            altd = fbdor.to<const char *>();  // Set the recieved medicine times to device 
            oled.clear();
            oled.setCursor(1,1);
            oled.setScale(1);
            oled.println("All Medicine Alarm");
            oled.println("times :");
            oled.setScale(1);
            oled.println(altd);
            oled.update();  // Display all medicine alarm times
            delay(8000); 
            updateAndDisplayAtime(true); // Update and display the next alarm time
         }  
         else{
           Serial.println(fbdor.errorReason().c_str());
           oled.clear();
           oled.setCursor(1,1);
           oled.setScale(1);
           oled.println("[ERROR] in accessing database");
           oled.update(); 
           delay(3000);
         }
     }
     else if (nav_opt == 3){  // If selected option is M. (Display last message note)
        if(Firebase.RTDB.getInt(&fbdor,path + "/Metadata/Messages_logs_CIV")){
          // Get the count of total message logs enteries stored in firebase database
          oled.clear();
          oled.setCursor(1,1);
          oled.setScale(1);
          int data_index_v = fbdor.to<int>()-1;
          if (Firebase.RTDB.getString(&fbdo, path + "/Messages_logs/"+String(data_index_v)+String("/Content"))){
            // Get the last message/note content stored in firebase database
            oled.println(fbdo.to<String>()); 
          }
          oled.update(); // display the message/note
          delay(3000);
        }
     }

     else if (nav_opt == 4){  // If selected option is R. (Display last sensor readings)
       oled.clear();
       oled.setCursor(1,1);
       oled.setScale(1);
       if(Firebase.RTDB.getInt(&fbdor,path + "/Metadata/Temperature_logs_CIV")){
          // Get the count of total temperature reading logs enteries stored in firebase database
          int data_index_v = fbdor.to<int>()-1;
          if (Firebase.RTDB.getJSON(&fbdo, path + "/Temperature_logs/"+String(data_index_v))){
            // Get the last temperature reading stored in firebase database
            json = fbdo.to<FirebaseJson>();
            json.get(jsonObj, "/Temperature");
            Serial.println("Temperature(F) : "+String(jsonObj.to<float>()));
            oled.println("Temperature(F) : ");
            oled.println(jsonObj.to<float>()); // Display the readings
          }
        }
        if(Firebase.RTDB.getInt(&fbdor,path + "/Metadata/Pulse_Oximeter_logs_CIV")){
          // Get the count of total pulse oximetry reading logs enteries stored in firebase database
          int data_index_v = fbdor.to<int>()-1;
          if (Firebase.RTDB.getJSON(&fbdo, path + "/Pulse_Oximeter_logs/"+String(data_index_v))){
            // Get the last pulse oximetry reading stored in firebase database
            json = fbdo.to<FirebaseJson>();
            json.get(jsonObj, "/Heart rate");
            oled.print("Heart rate : ");
            oled.println(jsonObj.to<float>());
            json = fbdo.to<FirebaseJson>();
            json.get(jsonObj, "/Spo2");
            oled.print("SPO2 : ");
            oled.println(jsonObj.to<int>());  // Display the readings
          }
        }
        
        oled.update();  
        delay(10000);  // Display the readings for 10 seconds
     }
      
    }
    else if (N2_btnst == HIGH) {  // If SOS button is pressed when alarm is not active
      String path = String(DEVICE_ID)+String("/Data/SOS call"),altd ="";
  //sets next medicine time
      if(Firebase.RTDB.getInt(&fbdo,path)){  // Get the SOS call count from firebase database
        if (Firebase.RTDB.setInt(&fbdo, path,fbdo.to<int>()+1)){ // Upadate the incremented SOS call count to firebase database
          oled.clear();
          oled.setCursor(1,1);
          oled.setScale(1);
          oled.print("SOS Request send");
          oled.update();  // Display SOS call success status
          delay(2500);
        }
    }
  }
}


void stream_callback_handler(FirebaseStream data){
  /*
  Input : [FirebaseStream] 'data' (class object instance)
  Output : none
  Description :
  Handles stream callbacks and based on the stream data recieved performs appropiate actions 
  */
  String path = String(DEVICE_ID)+String("/Data/Metadata/Stream_Update"),std ="";
  if (onlaunch_st){  // Skip the first stream callback on device boot 
    onlaunch_st = false;
    return ;
  }
  if (Firebase.RTDB.getString(&fbdor,path)){  // Get the stream update data from firebase database
    std = fbdor.to<String>();
    Serial.println(std.substring(0,1));
    if(std){
      if (std.substring(0,1).equals("A"))  set_at(data);  // "A" stands for alarm time update 
      // If the return string data contains "A" in begining then launch set_at function
      else if (std.substring(0,1).equals("M"))  set_m(data);  // "M" stands for new message/note update 
      // If the return string data contains "M" in begining then launch set_m function
      else return;
    }
  }
}

void set_at(FirebaseStream data){ 
  /*
  Input : [FirebaseStream] 'data' (class object instance)
  Output : none
  Description :
  Recieves stream callback data having alarm times updated in firebase database sets the updated alarm times in device
  and alerts about the status in realtime
  */
  String path = String(DEVICE_ID)+String("/Data"),altd ="";
  if (Firebase.RTDB.getString(&fbdor,path + "/medicine alarm time")){
    // Get the new updated medicine alarm times stored in firebase database
    altd = fbdor.to<String>(); // Update the new medicine alarm times in device
    timeClient.update(); 
    Serial.println(altd);
    Serial.println(timeClient.getFormattedTime());
    Serial.println(daysOfTheWeek[timeClient.getDay()]);  
    String temp_ntime = getNextAlarmTime(altd,timeClient.getHours(),timeClient.getMinutes(),timeClient.getDay());  
    //  get the new next medicine alarm time
    med_time[0] = temp_ntime.substring(0,3).toInt();
    med_time[1] = temp_ntime.substring(3,6).toInt();
    med_time[2] = temp_ntime.substring(6).toInt();
    //  Store the next medicine alarm time
    oled.clear();
    oled.setScale(1);
    oled.setCursor(0, 0);
    oled.println("New Alarm times set as - ");
    oled.println(altd);  // Display all new medicine alarm times
    Serial.println("Next medicine time : " + getTimeStr(med_time[0],med_time[1],0)+" "+daysOfTheWeek[med_time[2]]);
    oled.println("Next medicine time : " + getTimeStr(med_time[0],med_time[1],0)+" "+daysOfTheWeek[med_time[2]]);
    // Display the next medicine alarm time  
    oled.update();  
    // Sound the buzzer in blink fashion for 7 seconds
    digitalWrite(BUZZER,1);
    delay(1000);
    digitalWrite(BUZZER,0);
    delay(1000);
    digitalWrite(BUZZER,1);
    delay(1000);
    digitalWrite(BUZZER,0);
    delay(1000);
    digitalWrite(BUZZER,1);
    delay(1000);
    digitalWrite(BUZZER,0);
    delay(1000);
    digitalWrite(BUZZER,1);
    delay(1000);
    digitalWrite(BUZZER,0);
    }
    else{
        Serial.println("FAILED");
        Serial.println("REASON: " + fbdo.errorReason());
        Serial.println("------------------------------------");
        Serial.println();
    }
}

void set_m(FirebaseStream data)
{
  /*
  Input : [FirebaseStream] 'data' (class object instance)
  Output : none
  Description :
  Recieves stream callback data having new message/note entry in firebase database alert and displays the new message/note in device
  in realtime
  */
  String path = String(DEVICE_ID)+String("/Data"),altd ="";
  if(Firebase.RTDB.getInt(&fbdor,path + "/Metadata/Messages_logs_CIV")){
     // Get the count of total message logs enteries stored in firebase database
     oled.clear();
     oled.setCursor(1,1);
     oled.setScale(1);
     oled.println("New Message :");
     int data_index_v = fbdor.to<int>()-1;
     if (Firebase.RTDB.getString(&fbdor, path + "/Messages_logs/"+String(data_index_v)+"/Content")){
        // Get the last message/note content stored in firebase database
        oled.println(fbdor.to<String>());  // Display the note/message
     }
     oled.update(); 
    // Sound the buzzer in blink fashion for 7 seconds
    digitalWrite(BUZZER,1);
    delay(1000);
    digitalWrite(BUZZER,0);
    delay(1000);
    digitalWrite(BUZZER,1);
    delay(1000);
    digitalWrite(BUZZER,0);
    delay(1000);
    digitalWrite(BUZZER,1);
    delay(1000);
    digitalWrite(BUZZER,0);
    delay(1000);
    digitalWrite(BUZZER,1);
    delay(1000);
    digitalWrite(BUZZER,0);
    delay(1000);
  }
}

void streamTimeoutCallback(bool timeout)
{
  /*
  Input : [bool] 'timeout' (if stream timeout happens)
  Output : none
  Description :
  If input argument is true serial display the stream timeout status
  */
  if (timeout)
    Serial.println("stream timed out, resuming...\n");
}

void updateAndDisplayAtime(bool at_startup=false){
  /*
  Input : [bool] 'at_startup' (Function parameter to set next alarm time at what state of device)
  Output : none
  Description :
  Updates the medicine alarm times in device from firebase database and sets next medicine alarm time 
  */
  String path = String(DEVICE_ID)+String("/Data"),altd ="";
  int cv = 1;  // Add extra minute to current time for getNextAlarmTime function
  if (at_startup) cv=0;  // if the function executes at start of the device then add no additional minute 
  if (Firebase.RTDB.getString(&fbdor,path + "/medicine alarm time")){
    // Get the new updated medicine alarm times stored in firebase database
    altd = fbdor.to<String>();    // Set the recieved medicine times to device 
    String temp_ntime = getNextAlarmTime(altd,timeClient.getHours(),timeClient.getMinutes()+cv,timeClient.getDay());
    // Get the next medicine alarm time
    med_time[0] = temp_ntime.substring(0,3).toInt();
    med_time[1] = temp_ntime.substring(3,6).toInt();
    med_time[2] = temp_ntime.substring(6).toInt();
    // Set the next medicine alarm time to device
    oled.setScale(1);
    oled.println("");
    Serial.println("Next medicine time : " + getTimeStr(med_time[0],med_time[1],0)+" "+daysOfTheWeek[med_time[2]]);
    oled.println("Next medicine time : " + getTimeStr(med_time[0],med_time[1],0)+" "+daysOfTheWeek[med_time[2]]);
    oled.update();  // Display the next medicine alarm time
    delay(3000);
  }  
}

String getTimeStr(int h,int m,int s){
  /*
  Input : [int] 'h' (hours),[int] 'm' (minutes),[int] 's' (seconds)
  Output : [String] Formatted time string
  Description :
  Formats the time in "hh:mm:ss" format
  */
  String ts = "";
  if (h<10) ts = "0"+String(h);
  else  ts = String(h);
  if (m<10) ts = ts+":0"+String(m);
  else  ts = ts+":"+String(m);
  if (s<10) ts = ts+":0"+String(s);
  else ts = ts+":"+String(s);
  return(ts);   
}

float round2(float var){
  /*
  Input : [float] 'var' (input float value)
  Output : [float] roundoff float value
  Description :
  returns round off input float value upto 2 decimal places 
  */
  float value = (int)(var*100+.5);
  return (float)value/100;  
}

String getNextAlarmTime(String data,int ctime_h,int ctime_m,int ctime_d){
  /*
  Input : [String] 'data' (all medicine alarm times), [int] 'ctime_h' (current hour time), [int] 'ctime_m' (current minute time), 
          [int] 'ctime_d' (current day number)
  Output : [String] next medicine alarm time in format "hour min day_num"
  Description :
  Calculates and returns the next alarm time for input current time and all alarm times max limit upto 7 different alarm times
  */
  bool next_day = true;  // If the next alarm time is in next day 
  int medicine_time[7][2];  // Medicine time buffer
  int next_alarm_time[3];  // Next alarm time
  if (!String(data[data.length()-1]).equals(",")) data+=String(","); 
  // Add additional comma in end of string alarm times data if it is not there 
  String temp = ""; // Holds int time number data
  int it1= 0,it2 = 0;  // it1 increments when one medicine time entry is set 
  // it2 increments when hour,minute or second entry is set and reset after that 
  for(int i=0; i<(int)data.length(); i++){  // Converts string data to 2D int array of medicine times
    if(data[i] == ':'){
        medicine_time[it1][it2] = temp.toInt();
        it2 += 1;
        temp = "";
    }
    else if (data[i] == ','){
      //Serial.println(temp.toInt());
        medicine_time[it1][it2] = temp.toInt();
        it2 = 0;
        it1 += 1;
        temp = "";
    }
    else{
      temp += data[i];
    }
  }

  next_alarm_time[1] = medicine_time[0][0]; 
  next_alarm_time[2] = medicine_time[0][1];
  // Set the first medicine time as next alarm time
  for(int i=0;i<it1;i++){  // Iterate over all medicine alarm times and get the next alarm time
    if (ctime_h*100+ctime_m < medicine_time[i][0]*100+medicine_time[i][1]){  // If current time is less than medicine time
      next_day = false;  // Set next day to false means next alarm time is on the same day
      if ((next_alarm_time[1]*100+next_alarm_time[2] < medicine_time[i][0]*100+medicine_time[i][1]) && (next_alarm_time[1]*100+next_alarm_time[2] > ctime_h*100+ctime_m)){}
      else{ // If next alarm time is greater than equal to medicine time and next alarm time is less than equal to current time
        next_alarm_time[1] = medicine_time[i][0];
        next_alarm_time[2] = medicine_time[i][1];
        // Set medicine time to next alarm time
      }
    }
    else{ // If current time is greater than medicine time
      if((next_alarm_time[1]*100+next_alarm_time[2] < ctime_h*100+ctime_m) && (next_alarm_time[1]*100+next_alarm_time[2] > medicine_time[i][0]*100+medicine_time[i][1])){
        // If current time is greater than next alarm time and next alarm time is greater than medicine time
        next_day = true; // Set next day to true means next medicine alarm time is on next day
        next_alarm_time[1] = medicine_time[i][0];
        next_alarm_time[2] = medicine_time[i][1];
        // Set medicine time to next alarm time
      } 
    }
  }
  if (next_day) ctime_d += 1; // Increment the current day number if next alarm time is on next day 
  return String(next_alarm_time[1])+"  "+String(next_alarm_time[2])+"  "+String(ctime_d);  // Return the next alarm time string format
} 
