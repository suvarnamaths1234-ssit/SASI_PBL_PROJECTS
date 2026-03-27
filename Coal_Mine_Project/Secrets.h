#ifndef SECRETS_H
#define SECRETS_H

/*************** WIFI DETAILS ****************/
#define WIFI_SSID        "Suvarna"
#define WIFI_PASSWORD    "12345678"

/*************** ZOHO IOT MQTT DETAILS ***************/
/* Broker URL from Zoho device download */
#define ZOHO_MQTT_BROKER "12566pcelj.zohoiothub.in"

/* Device ID from Zoho */
#define ZOHO_CLIENT_ID   "7045000000168217"

/* Username for MQTT connect (exactly as provided by Zoho) */
#define ZOHO_USERNAME    "/12566pcelj.zohoiothub.in/v1/devices/7045000000168217/connect"

/* Device Token (Password) from Zoho */
#define ZOHO_AUTH_TOKEN  "49cc5d81595e2bddb78745d7d3c2ebecce73c9eda9ee2b2e43912d5e4b257"

/* Publish topic MUST be same as username */
//#define ZOHO_PUB_TOPIC   ZOHO_USERNAME
#define ZOHO_PUB_TOPIC  "/devices/7045000000168217/telemetry"


#endif  // SECRETS_H

