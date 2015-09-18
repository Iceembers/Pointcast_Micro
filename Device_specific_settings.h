//This contains the specific settings that need changing for each new device

//WiFi settings
#define WLAN_SSID       "WiFi SSID"           // cannot be longer than 32 characters!
#define WLAN_PASS       "password"
#define WLAN_SECURITY   WLAN_SEC_WPA2  // Security can be WLAN_SEC_UNSEC, WLAN_SEC_WEP, WLAN_SEC_WPA or WLAN_SEC_WPA2

//Data Settings:
char longitude[16] = "134.1441";
char latitude[16] = "10.0000";
char user_id[7] = "100062";
//char alt[15];                         //not currently used as CC3000 won't upload that length json





//Time between uploads
// Sampling interval (e.g. 60,000ms = 1min)
const int updateIntervalInSeconds = 300;   //Set the time between uploads, 300s=5min
//Maximum number of failed connections before it reboots
#define MAX_FAILED_CONNS 3


