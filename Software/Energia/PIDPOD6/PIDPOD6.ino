/*
 * Name:        PIDPOD
 * Date:        2014-12-08
 * Version:     1.0
 * Description: Segway-type self-balancing robot sketch based on the CC3200 development board.
 */

#define ENABLE_MOTORS
//#define ENABLE_WIFI
//Sensors conversion factors
#define ACC_RAW2MPS2 0.001
#define GYRO_RAW2RADPS 0.005
// bias compensation parameters
// samples to take before changing the upright position
#define NUMBER_SAMPLES 100
#define I_ARW 300
// initial uproght position
#define UPRIGHT_POSITION 4500
// upright position offset bias
#define UPRIGHT_OFFSET 10
// maximum allowed bias for the upright position
#define MAX_BIAS 500

#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiServer.h>

char ssid[] = "Workshop";
char password[] = "ecolblimp";
// Don't forget to change the WiFi.begin function as well

WiFiServer server(80);

#include <Wire.h>
#include <MPU9150.h>
#include <Motors.h>

float integral = 0;
float derivate = 0;

float acc_reading = 0;
float gyro_angle = 0;
float gyro_offset = 0;
float angle = 0;
float gyro_integration = 0;
float angle_offset = 0;
float angle_old = 0;
float angle_real = 0;
float distance = 0;
float upright_value_accelerometer = UPRIGHT_POSITION;
float upright_value_accelerometer_default;

float kp = 10;
float ki = 10;
float kd = .5;

//bia compensation
float bia_ki = 0.1;

int16_t speed = 0;

// Pin definitions
#define LED RED_LED
#define SWAG_LED 5
#define DIP4 15

void setup()
{
  Serial.begin(115200);
  pinMode(15, INPUT);
  pinMode(LED, OUTPUT);
  pinMode(SWAG_LED, OUTPUT);
  
  // --------- START WIFI
  #ifdef ENABLE_WIFI
  WiFi.begin(ssid);
  //WiFi.begin(ssid, password);
  while(WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(300);
  }
  
  Serial.println("\nYou're connected to the network");
  Serial.println("Waiting for an ip address");
  
  while(WiFi.localIP() == INADDR_NONE)
  {
    // print dots while we wait for an ip addresss
    Serial.print(".");
    delay(300);
  }
  
  // you're connected now, so print out the status  
  printWifiStatus();
  
  Serial.println("Starting webserver on port 80");
  server.begin();                           // start the web server on port 80
  Serial.println("Webserver started!");
  #endif
  // --------- END WIFI
  
  digitalWrite(SWAG_LED, HIGH);

  #ifdef ENABLE_MOTORS
  motorSetup();
  #endif
  
  // initialize serial communication
  Serial.begin(115200);
  
  // Initialize the 'Wire' class for the I2C-bus needed for IMU
  Wire.begin();
  
  MPU9150_init();
  
  // Find accelerometer and gyroscope offset
  float accelerometer = 0;
  gyro_offset = 0;
  uint8_t i;
  for(i = 0; i < 16; i++)
  {
    accelerometer += MPU9150_readSensor(MPU9150_ACCEL_ZOUT_L, MPU9150_ACCEL_ZOUT_H);
    gyro_offset += MPU9150_readSensor(MPU9150_GYRO_XOUT_L, MPU9150_GYRO_XOUT_H);
  }
  // average
  upright_value_accelerometer = accelerometer/16;
  upright_value_accelerometer_default = upright_value_accelerometer;
  gyro_offset /= 16;
  
  // Setup done
  digitalWrite(LED, HIGH);
}

void loop()
{
  static uint32_t lastTime = micros();
  static uint32_t dt = micros();
  
  acc_reading = -(MPU9150_readSensor(MPU9150_ACCEL_ZOUT_L, MPU9150_ACCEL_ZOUT_H) - upright_value_accelerometer) * ACC_RAW2MPS2;
  gyro_angle = (MPU9150_readSensor(MPU9150_GYRO_XOUT_L, MPU9150_GYRO_XOUT_H) - gyro_offset) * GYRO_RAW2RADPS;
  dt = micros() - lastTime;
  
  angle = (angle + gyro_angle * dt / 1000000) * 0.98 + (acc_reading * 0.02);
  
  integral = integral + angle * .1;
  // ARW
  if(integral > 10)
  {
    integral = 10;
  }
  else if(integral < -10)
  {
    integral = -10;
  }
  
  lastTime = micros();
  
  //speed = angle * 10 + gyro_angle * 0.5 + integral * 10;
  speed = angle * kp + integral * ki + gyro_angle * kd;
  
  // Apply motor speeds
  #ifdef ENABLE_MOTORS
  if(digitalRead(DIP4))
  {
    setSpeed(speed, speed);
  }
  else
  {
    setSpeed(0, 0);
  }
  #endif
  
  #ifdef ENABLE_WIFI
  wifi();
  #endif
  
  //biasCompensation();
  
  delay(8);
}

void biasCompensation()
{
  static uint8_t mem = 0;
  static int16_t memory[NUMBER_SAMPLES] = {0};
  uint16_t speed_sum = 0;
  
  memory[mem] = speed;
    
  mem++;
  if(mem > NUMBER_SAMPLES)
    mem = 0;
  
  /* Sliding average */
  speed_sum = 0;
  for(uint8_t k = 0; k < NUMBER_SAMPLES; k++)
    speed_sum += memory[k];
    
  /* Set setpoint */
  /* upright_value_accelerometer must be bigger for speed > 0, smaller for speed < 0 */
  upright_value_accelerometer += (speed_sum/NUMBER_SAMPLES) * bia_ki;
  
  /* pseudo ARW */
  if(upright_value_accelerometer > upright_value_accelerometer_default + I_ARW)
    upright_value_accelerometer = upright_value_accelerometer_default + I_ARW;
    
  if(upright_value_accelerometer < upright_value_accelerometer -I_ARW)  
    upright_value_accelerometer = upright_value_accelerometer -I_ARW;
    
}

void wifi()
{
  // listen for incoming clients
  WiFiClient client = server.available();
  if (client) {
    //Serial.println("new client");
    // an http request ends with a blank line
    boolean currentLineIsBlank = true;
    char input[20];
    char type;
    uint8_t count = 0;;
    int kint = 0;
    float kfloat = 0;
    uint8_t found = 0;
    char testchar;
    char digit[4];
    while(client.connected())
    {
      if(client.available())
      {
        char c = client.read();
        //Serial.write(c);
        
        // Catch the first 20 lines
        if(count < 20)
        {
          count++,
          input[count] = c;
        }
        
        // if you've gotten to the end of the line (received a newline
        // character) and the line is blank, the http request has ended,
        // so you can send a reply
        if (c == '\n' && currentLineIsBlank) {
          found = 0;
          count = 0;
          // send a standard http response header
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/html");
          client.println("Connection: close");  // the connection will be closed after completion of the response
          client.println();
          client.print("<!DOCTYPE HTML><html><script>");
          client.print("function drag(field,value){document.getElementById(field).innerHTML=value/100;}function sendValues(){var fields=['JP','JI','JD'];for(i=0;i<3;i++){set(fields[i],document.getElementById(fields[i]).value);}}function set(field,value){var xmlhttp = new XMLHttpRequest();xmlhttp.open(\"GET\",\"?\"+field+\"=\"+(value<1000?'0':'')+(value<100?'0':'')+(value<10?'0':'')+value,true);xmlhttp.send(null);}</script>");
          client.print("Upright position: ");
          client.print(upright_value_accelerometer);
          client.print("<br />Set values: <input type=\"button\" value=\"Set\" onclick=\"sendValues();\" /><br />");
          client.print("KP: <span id=\"KP\">10</span> <input style=\"width:100%\" type=\"range\" name=\"kp\" min=\"0\" max=\"2000\" value=\"1000\" step=\"1\" id=\"JP\" oninput=\"drag('KP',this.value)\" /><br />KI: <span id=\"KI\">10</span> <input style=\"width:100%\" type=\"range\" name=\"ki\" min=\"0\" max=\"2000\" value=\"1000\" step=\"1\" id=\"JI\" oninput=\"drag('KI',this.value)\" /><br />KD: <span id=\"KD\">0.5</span> <input style=\"width:100%\" type=\"range\" name=\"kd\" min=\"0\" max=\"2000\" value=\"50\" step=\"1\" id=\"JD\" oninput=\"drag('KD',this.value)\" />");
          client.println("</html>");
          break;
        }
        if (c == '\n') {
          // you're starting a new line
          if(!found)
          {
            type = input[8];
            digit[0] = input[10];
            digit[1] = input[11];
            digit[2] = input[12];
            digit[3] = input[13];
            
            // parse
            kint = (((uint8_t)digit[0])-48)*1000 + (((uint8_t)digit[1])-48)*100 + (((uint8_t)digit[2])-48)*10 + ((uint8_t)digit[3])-48;
            kfloat = (float)kint/100.;
            
            if(kint <= 2000)
            {
              switch(type)
              {
                case 'P':
                  kp = kfloat;
                  break;
                case 'I':
                  ki = kfloat;
                  break;
                case 'D':
                  kd = kfloat;
                  break;
              }
              Serial.println(kint);
            }
            
            found = 1;
          }
          
          currentLineIsBlank = true;
        }
        else if (c != '\r') {
          // you've gotten a character on the current line
          currentLineIsBlank = false;
        }
      }
    }
    // give the web browser time to receive the data
    delay(1);

    // close the connection:
    client.stop();
    //Serial.println("client disonnected");
  }
}

void printWifiStatus() {
  // print the SSID of the network you're attached to:
  Serial.print("Network Name: ");
  Serial.println(WiFi.SSID());

  // print your WiFi shield's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}

