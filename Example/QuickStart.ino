#include <SPI.h>
#include <WiFi.h>
#define DEBUG_RESPONSE 1
#include <Parse.h>

const char* ssid = "Your SSID";
const char* password ="Password"
const char* YOUR_PARSE_URL="yourparseserver.com";
//Settings
#define SLOW_BOOT 0
#define HOSTNAME "PARSE"
#define FORCE_USE_HOTSPOT 0

const char* hostname = "Parse";

String ledState;
const int BUFSIZE = 200;

// set LED state and send state to Parse

void basicObjectTest() {
  Serial.println("basic object operation test");

  Serial.println("create...");
  ParseObjectCreate create;
  create.setClassName("Temperature");
  create.add("temperature", 175.0);
  create.add("leverDown", true);
  ParseResponse createResponse = create.send();
  char* objectId = new char[10];
  strcpy(objectId, createResponse.getString("objectId"));
  assert(createResponse.getErrorCode() == 0);
  createResponse.close();

  Serial.println("update...");
  ParseObjectUpdate update;
  update.setClassName("Temperature");
  update.setObjectId(objectId);
  update.add("temperature", 100);
  ParseResponse updateResponse = update.send();
  assert(updateResponse.getErrorCode() == 0);
  updateResponse.close();

  Serial.println("get...");
  ParseObjectGet get;
  get.setClassName("Temperature");
  get.setObjectId(objectId);
  ParseResponse getResponse = get.send();
  double temp = getResponse.getDouble("temperature");
  //assert(temp == 100);
  getResponse.close();

  Serial.println("delete...");
  ParseObjectDelete del;
  del.setClassName("Temperature");
  del.setObjectId(objectId);
  //ParseResponse delResponse = del.send();
  //String expectedResp = "{}";
  //String actualResp = String(delResponse.getJSONBody());
//  actualResp.trim();
  //assert(expectedResp.equals(actualResp));
  //delResponse.close();

  Serial.println("test passed\n");
}
void objectDataTypesTest() {
  Serial.println("data types test");

  ParseObjectCreate create;
  create.setClassName("TestObject");
  create.addGeoPoint("location", 40.0, -30.0);
  create.addJSONValue("dateField", "{\"__type\":\"Date\",\"iso\":\"2011-08-21T18:02:52.249Z\"}");
  create.addJSONValue("arrayField", "[30,\"s\"]");
  create.addJSONValue("emptyField", "null");
  ParseResponse createResponse = create.send();
  assert(createResponse.getErrorCode() == 0);
  createResponse.close();

  Serial.println("test passed\n");
}

void queryTest() {
  Serial.println("query test");

  ParseObjectCreate create1;
  create1.setClassName("Temperature");
  create1.add("temperature", 88.0);
  create1.add("leverDown", true);
  ParseResponse createResponse = create1.send();
  createResponse.close();

  ParseObjectCreate create2;
  create2.setClassName("Temperature");
  create2.add("temperature", 88.0);
  create2.add("leverDown", false);
  createResponse = create2.send();
  createResponse.close();

  ParseQuery query;
  query.setClassName("Temperature");
  query.whereEqualTo("temperature", 88);
  query.setLimit(2);
  query.setSkip(0);
  query.setKeys("temperature");
  ParseResponse response = query.send();

  int countOfResults = response.count();
  assert(countOfResults == 2);
  while(response.nextObject()) {
    assert(88 == response.getDouble("temperature"));
  }
  response.close();

  Serial.println("test passed\n");
}
// This method must be called after parse.initialize()
void setLedState(char state[]) {
  ledState = state;

  // send current led state to parse
  ParseObjectCreate create;
  create.setClassName("Event");
  create.add("installationId", Parse.getInstallationId());
  create.add("alarm", true);
  String value = "{\"state\":\"";
  value += state;
  value += "\"}";
  create.addJSONValue("value", value);
  ParseResponse response = create.send();

  String eventId;
  Serial.println("\nSetting LED state...");
  Serial.print(response.getJSONBody());
  if (!response.getErrorCode()) {
    eventId = response.getString("objectId");
    Serial.print("Event id:");
    Serial.println(eventId);
  } else {
    Serial.println("Failed to notify Parse the LED state.");
  }
  response.close();
}


void connectWifi() {
	int connect_timeout;

#if defined(ESP32)
	WiFi.setHostname(HOSTNAME);
#else
	WiFi.hostname(HOSTNAME);
#endif
	Serial.println("Begin wifi...");


	
		//Try to connect with stored credentials, fire up an access point if they don't work.
		#if defined(ESP32)
			WiFi.begin(ssid, password);
		#else
			WiFi.begin(stored_ssid, stored_pass);
		#endif
		connect_timeout = 28; //7 seconds
		while (WiFi.status() != WL_CONNECTED && connect_timeout > 0) {
			delay(250);
			Serial.print(".");
			connect_timeout--;
		}
	
	
	if (WiFi.status() == WL_CONNECTED) {
		Serial.println(WiFi.localIP());
		Serial.println("Wifi started");

	}
}

void setup() {
 //Initialize serial and wait for port to open:
  Serial.begin(115200);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for Leonardo only
  }

connectWifi();


// appId and clientKey will be provided in provisioning
  Parse.begin("any-random-number-of-strings", "kjlsfkjlsfjh");
  Parse.setServerURL(YOUR_PARSE_URL);

  // do provisioning now
  Serial.println("Please go to arduino.local/parse_config.html to complete device provisioning. Press y when you are done.");
 
  Serial.println("Parse blinky example started: InstalltionID");
  Serial.println(Parse.getInstallationId());
  //Serial.println(Parse.getSessionToken());

  // Turn off LED
  setLedState("off");
basicObjectTest();
  /* start push service */
  //if(Parse.startPushService()) {
    //Serial.println("\nParse push started\n");
  //}


}

void loop() {

}
