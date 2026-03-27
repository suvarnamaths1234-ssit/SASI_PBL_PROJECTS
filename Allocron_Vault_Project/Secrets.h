#ifndef SECRETS_H
#define SECRETS_H

/*************** WIFI DETAILS ****************/
#define WIFI_SSID        "ACTFIBERNET"
#define WIFI_PASSWORD    "act12345"

/*************** ZOHO IOT MQTT DETAILS ***************/
/* Broker URL from Zoho device download */
#define ZOHO_MQTT_BROKER "11622ytybz.zohoiothub.in"

/* Device ID from Zoho */
#define ZOHO_CLIENT_ID   "3995000000166217"

/* Username for MQTT connect (exactly as provided by Zoho) */
#define ZOHO_USERNAME    "/11622ytybz.zohoiothub.in/v1/devices/3995000000166217/connect"

/* Device Token (Password) from Zoho */
#define ZOHO_AUTH_TOKEN  "85fdadab78c6451a55d170b6cf0bd66584241cb93e41858d319eca327ac2e6f"

/* Publish topic MUST be same as username */
//#define ZOHO_PUB_TOPIC   ZOHO_USERNAME
#define ZOHO_PUB_TOPIC  "/devices/3995000000166217/telemetry"


#endif  // SECRETS_H