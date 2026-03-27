#ifndef SECRETS_H
#define SECRETS_H

/*************** WIFI DETAILS ****************/
#define WIFI_SSID        "Raj"
#define WIFI_PASSWORD    "6281632129"

/*************** ZOHO IOT MQTT DETAILS ***************/
/* Broker URL from Zoho device download */
#define ZOHO_MQTT_BROKER "19559ecccs.zohoiothub.in"
	

/* Device ID from Zoho */
#define ZOHO_CLIENT_ID   "6442000000161386"

/* Username for MQTT connect (exactly as provided by Zoho) */
#define ZOHO_USERNAME    "/19559ecccs.zohoiothub.in/v1/devices/6442000000161386/connect"

/* Device Token (Password) from Zoho */
#define ZOHO_AUTH_TOKEN  "93ef4ec162cbb4cd767376a826e3f6feda4a74874e61232a57ded5742e"

/* Publish topic MUST be same as username */
//#define ZOHO_PUB_TOPIC   ZOHO_USERNAME
#define ZOHO_PUB_TOPIC  "/devices/6442000000161386/telemetry"


#endif  // SECRETS_H