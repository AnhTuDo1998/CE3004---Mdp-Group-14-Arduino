/*
 * Version: 2.1
 * 
 * Description:
 * This code is part of the MDP 2019 done by group 14. 
 * This code is written for integration testing of Arduino Robot Hardware to Rpi via serial communication. 
 * 
 * Author: 
 * Anh Tu Do U1721947F
 * 
 * Features: Releases 2.1
 * 1. Serial buffer and serial communication to interact with RPI. Data to be sent back to RPI are pending on RPI, Android and Algo team.
 *  1.1. Template for serial communication done, can tolerate a string of 50 characters
 * 2. Calibration using front sensors when reaching a wall // object
 * 3. Accurate & deterministic straight line motion, rotation and simple object avoidance.
 *  3.1. Moving straight 2x2 grid in 1-6 blocks without much deviation. Discrete motion only (not continous yet)
 *  3.2. Rotate Left and Right exactly by 90 degree.
 *  3.3. Motor Characteristic calibrated.
 * 4. Attempt of having 2 operating mode for robot FAST and NORMAL.
 * 5. Sensor able to return distance in grid/block away from obstacle.
 *  5.1. Using sensor with median filter.
 * 
 *  
 * Notes & Issues:
 * - Implementing of Median filter and comparison for best performance
 * - Experimenting with different sample size to ensure most accurate sensor reading but good time complexity
 * 
 * 
 */


/*
 * ===================================
 * Packages 
 * DualVNH5019MotorShield - Library to interface with Pololu motorshield to control DC motors
 * SharpIR - Library to interface with Sharp IR sensors
 * EnableInterrupt - Library to allows interrupts control on any digital pin. Note: Arduino Uno can only configure D0 and D1 as interrupt 
 * pin by AttachInterrupt() without this library.
 * PID_v1 - Library to easy and quick integration of PID Controller
 * ===================================
 */

#include "DualVNH5019MotorShield.h"
#include "SharpIR.h"
#include <EnableInterrupt.h>
#include <PID_v1.h>
#include <ArduinoSort.h>


/*
 * ==============================
 * Variables declaration
 * ==============================
 */

//Define Encoder Pins
int encoder1A = 3;
int encoder1B = 5;
int encoder2A = 11;
int encoder2B = 13;

double distance_cm; //distance in cm that the robot need to move.
double speed1, speed2; //speed (in PWM) to feed to the motorshield.

//ticks parameters for PID
volatile long tick1 = 0;
volatile long tick2 = 0;
long ticks_moved = 0;
double currentTick1, currentTick2, oldTick1, oldTick2;

//Serial buffer for communication
char command[50];
int count = 0;

//Operating states
bool FAST;
bool DEBUG = true;
bool CALIBRATE = true;

//For sensors
#define SAMPLE 30
volatile double sensor_reading[SAMPLE]; //for median filtering

//constructors
DualVNH5019MotorShield md;
SharpIR front_center(SharpIR:: GP2Y0A21YK0F, A0);
SharpIR front_left(SharpIR:: GP2Y0A21YK0F, A1);
SharpIR front_right(SharpIR:: GP2Y0A21YK0F, A2);
SharpIR right_front(SharpIR:: GP2Y0A21YK0F, A3);
SharpIR right_back(SharpIR:: GP2Y0A21YK0F, A4);
SharpIR long_left(SharpIR:: GP2Y0A02YK0F, A5);

//Refer to end of program for explanation on PID
PID PIDControlStraight(&currentTick1, &speed1, &currentTick2, 0 ,0 ,0, DIRECT);
PID PIDControlLeft(&currentTick1, &speed1, &currentTick2, 3, 0, 0, DIRECT);
PID PIDControlRight(&currentTick1, &speed1, &currentTick2, 3, 0, 0, DIRECT);

/*
 * ==============================
 * Main Program
 * ==============================
 */
void setup() {
  Serial.begin(9600);
  Serial.setTimeout(50);
  if (DEBUG){
    Serial.println("Connected");
  }
  
  md.init();
  
  //Attach interrupts to counts ticks
  pinMode(encoder1A,INPUT);
  enableInterrupt(encoder1A, E1_Pos, RISING);
  pinMode(encoder2A,INPUT);
  enableInterrupt(encoder2A, E2_Pos, RISING);

  //init values
  currentTick1 = currentTick2 = oldTick1 = oldTick2 = 0;
  //rotate_left(1);
//  while (true){
//    move_forward(1);
//    delay(50);
//    move_forward(1);
//    delay(50);
//    move_forward(1);
//    delay(50);
//    right_wall_calibrate();
//    delay(50);
//    move_forward(1);
//    delay(50);
//    rotate_right(90);
//    delay(50);
//    front_calibrate();
//    delay(50);
//    rotate_right(90);
//    delay(50);
//    move_forward(1);
//    delay(50);
//    move_forward(1);
//    delay(50);
//    move_forward(1);
//    delay(50);
//    move_forward(1);
//
//    
//  }

while (true){
  int front, left, right;
  for(int i=SAMPLE; i>0; i--){
      sensor_reading[i] = front_center.getDistance(true);
      delay(10);
    }
  sortArray(sensor_reading,SAMPLE);
  front = sensor_reading[SAMPLE/2]; 

    for(int i=SAMPLE; i>0; i--){
      sensor_reading[i] = front_left.getDistance(true);
      delay(10);
    }

  sortArray(sensor_reading,SAMPLE);
  left = sensor_reading[SAMPLE/2]; 

    for(int i=SAMPLE; i>0; i--){
      sensor_reading[i] = front_right.getDistance(true);
      delay(10);
    }

  sortArray(sensor_reading,SAMPLE);
  right = sensor_reading[SAMPLE/2]; 
  Serial.print(front);Serial.print(" "); Serial.print(left); Serial.print(" "); Serial.println(right);
}
}

void loop() {
  get_command();
  //Debug to see all commands collected.
//  if(DEBUG){
//    print_all_commands();
//  }
  count=0;
  while (command[count] != '\0'){
    //while not end of string
    switch(command[count]){
      case 'W':
      case 'w':
        //move the robot forward with stipulated distance unit (blocks)
        switch(command[count+1]){
          //check how many blocks to move
          case '0':
          case '1':
          case '2':
          case '3':
          case '4':
          case '5':
          case '6':
          case '7':
          case '8':
          case '9':
            if(DEBUG){
              Serial.print("Moving Forward by ");
              Serial.print(command[count+1]);
              Serial.println(" units"); 
            }
            move_forward(int(command[count+1])-48); //moving forward by amount stipulcated.
//            for (int i = 0; i< command[count+1]-48; i++){
//              //Serial.println(i);
//              Serial.println("MC");
//            }
            //offset -48 is to convert ASCII to blocks distance (cuz 0 = 48 for ascii decimal and so on so forth)
            break;
            
          default: 
            if(DEBUG) {
                Serial.println("S's ERROR INVALID COMMAND: "); 
                Serial.println(command[count]); 
             }
             break;
             
            }
        count++;
        break;
        
      case 'A':
      case 'a':
        //rotate robot to the left 90 degrees
        if (DEBUG){Serial.println("Rotating Left by 90 degrees");}
        rotate_left(90);
        Serial.println("MC");
        break;
        
      case 'D':
      case 'd':
        //rotate robot to the right 90 degrees
        if (DEBUG){Serial.println("Rotating Right by 90 degrees");}
        rotate_right(90);
        Serial.println("MC");
        break;

      case 'H':
      case 'h':
        //calibrate to right wall hug
        if (DEBUG){Serial.println("Right Wall Calibration");}
        right_wall_calibrate();
        Serial.println("CC");
        break;

      case 'F':
      case 'f':
        //calibrate front
        if (DEBUG){Serial.println("Front Calibrating");}
        front_calibrate();
        Serial.println("CC");
        break;

      case 'S':
      case 's':
        read_all_sensors();
        break;

      case 'E':
      case 'e':
        if (DEBUG){
          Serial.println("Ending and Realigning");
        }
        Serial.println("EC");
        break;
      
      default: //by default means error command
        if(DEBUG) {
          Serial.print("ERROR INVALID COMMAND: "); 
          Serial.println(command[count]); 
        }
        break;
    }
    count++;
  }
  command[0] = '\0';
}

/*
 * =======================================================
 * Motion Controls
 * Methods to move the robot straight, left, right, etc...
 * =======================================================
 */

 //A method to rotate robot to the right by a degree. Using 360 degree as a base line
void rotate_right(double degree)
{
  double target_tick = 0; 
  //target_tick =4.3589*degree - 32.142;
  target_tick = 400;
  //0.2319*degree + 6.4492;
  double tick_travelled = 0;
  if (target_tick<0) return;
  
  // Init values
  tick1 = tick2 = 0; //encoder's ticks (constantly increased when the program is running due to interrupt)
  currentTick1 = currentTick2 = 0; //ticks that we are used to calculate PID. Ticks at the current sampling of PIDController
  oldTick1 = oldTick2 = 0;
  speed1 = rpm_to_speed_1(-70);
  speed2 = rpm_to_speed_2(68.5);

  
  md.setSpeeds(speed1,speed2);
  tick_travelled = (double)tick2;

  PIDControlLeft.SetSampleTime(50); //Controller is called every 50ms
  PIDControlLeft.SetMode(AUTOMATIC); //Controller is invoked automatically.

  while(tick_travelled < target_tick){
      // if not reach destination ticks yet
      currentTick1 = tick1 - oldTick1; //calculate the ticks travelled in this sample interval of 50ms
      currentTick2 = tick2 - oldTick2;

      //Serial.print(currentTick1); //for debug
      //Serial.print(" "); Serial.println(currentTick2);
      
      PIDControlLeft.Compute();
      oldTick2 += currentTick2; //update ticks
      oldTick1 += currentTick1;
      tick_travelled += currentTick2;
      //oldTick2 = tick2;
      //oldTick1 = tick1;
  }

     for (int i = 0; i <= 400; i+=200){
     md.setBrakes(i,i);
     delay(1); 
   }
   PIDControlLeft.SetMode(MANUAL);
   //delay(100);
}

//A method to rotate robot to the left by a degree. Using 360 degree as a base line
void rotate_left(double degree)
{
  double target_tick = 0;
  target_tick = 400;
  //target_tick = 4.1533*degree; 
  double tick_travelled = 0;

  if (target_tick<0) return;
  
  // Init values
  tick1 = tick2 = 0; //encoder's ticks (constantly increased when the program is running due to interrupt)
  currentTick1 = currentTick2 = 0; //ticks that we are used to calculate PID. Ticks at the current sampling of PIDController
  oldTick1 = oldTick2 = 0;
  speed1 = rpm_to_speed_1(70);
  speed2 = rpm_to_speed_2(-69);
  //speed1 = -210;
  //speed2 = 210;

  
  md.setSpeeds(speed1,speed2);
  tick_travelled = (double)tick2;

  PIDControlRight.SetSampleTime(50); //Controller is called every 50ms
  PIDControlRight.SetMode(AUTOMATIC); //Controller is invoked automatically.

  while(tick_travelled < target_tick){
      // if not reach destination ticks yet
      currentTick1 = tick1 - oldTick1; //calculate the ticks travelled in this sample interval of 50ms
      currentTick2 = tick2 - oldTick2;

      //Serial.print(currentTick1); //for debug
      //Serial.print(" "); Serial.println(currentTick2);
      PIDControlRight.Compute();
      oldTick2 += currentTick2; //update ticks
      oldTick1 += currentTick1;
      tick_travelled += currentTick2;
      //oldTick2 = tick2;
      //oldTick1 = tick1;
  }

     for (int i = 0; i <= 400; i+=200){
     md.setBrakes(i,i);
     delay(1); 
   }
   PIDControlRight.SetMode(MANUAL); //turn off PID
   //delay(100);
}

//A method to move robot forward by distance/unit of square
void move_forward(double distance){
  //at 6.10v to 6.20v
   double target_tick = 0; 
   //calibrate by set a random value and measure actual distance
   //295 = 1 block, 600 = 2 blocks,910 = 3 blocks,1217 = 4 blocks, 1524 = 5 blocks, 1840 = 6 blocks  
   //Serial.println(distance); //debug
   target_tick = 308.69*distance - 19.067;
   double tick_travelled = 0;
   
//   if(DEBUG){
//   Serial.print("Target: ");
//   Serial.println(target_tick);
//   }

   if(target_tick<0) return;

   // Init values
   tick1 = tick2 = 0; //encoder's ticks (constantly increased when the program is running due to interrupt)
   currentTick1 = currentTick2 = 0; //ticks that we are used to calculate PID. Ticks at the current sampling of PIDController
   oldTick1 = oldTick2 = 0;

   //Speed in rpm for motor 1 and 2
   speed1 = rpm_to_speed_1(70.75);
   speed2 = rpm_to_speed_2(70.5);
   
//   Serial.println("Speed:");//for debug
//   Serial.print(speed1);Serial.print(" ");
//   Serial.println(speed2);

   //Implementing gradual acceleration to remove jerks
   for (int j = 0; j < speed2; j+=50){
     md.setSpeeds(j,j-2.5);
     delay(5); 
   }

   //Set Final ideal speed and accomodate for the ticks we used in acceleration
   md.setSpeeds(speed1,speed2);
   tick_travelled = (double)tick2;

   PIDControlStraight.SetSampleTime(50); //Controller is called every 50ms
   PIDControlStraight.SetMode(AUTOMATIC); //Controller is invoked automatically.

   while(tick_travelled < target_tick){
      // if not reach destination ticks yet
      currentTick1 = tick1 - oldTick1; //calculate the ticks travelled in this sample interval of 50ms
      currentTick2 = tick2 - oldTick2;
      
//      Serial.print(currentTick1); //for debug
//      Serial.print(" "); Serial.println(currentTick2);
      PIDControlStraight.Compute();

      oldTick2 += currentTick2; //update ticks
      oldTick1 += currentTick1;
      tick_travelled += currentTick2;
      //oldTick2 = tick2;
      //oldTick1 = tick1;
//      if (int(tick_travelled) == 300 || int(tick_travelled) == 600 || int(tick_travelled) == 900 ||int(tick_travelled) == 1200 ||int(tick_travelled) == 1500 || int(tick_travelled) == 1800 ||int(tick_travelled) == 2100 || int(tick_travelled) == 2400 || int(tick_travelled) == 2700)
//        Serial.println(tick_travelled);
//        Serial.println("MC");
//        delay(10);
      
   }
   
   for (int i = 0; i <= 400; i+=50){
     md.setBrakes(i-5,i+5);
     delay(2.5); 
   }
   PIDControlStraight.SetMode(MANUAL);
   Serial.println("MC");
   delay(100);
}

/*
 * ==============================================================
 * Sensors
 * Methods dealing with data acquisition and processing of sensor
 * ==============================================================
 */

 //Methods to return blocks unit from obstacles ++++++++++++
int distance_short_front_center(){
  int distance_cm = 0;
  for(int i=SAMPLE; i>0; i--){
      sensor_reading[i] = front_center.getDistance(false);
      delay(1);
    }
     for(int i=SAMPLE; i>0; i--){
      sensor_reading[i] = front_center.getDistance(true);
      delay(1);
    }
  sortArray(sensor_reading,SAMPLE);
    
  //  sensor_distance_cm[0] = sensor_reading[SAMPLE/2];
  //  Serial.println(sensor_distance_cm[0]);
  
  distance_cm = sensor_reading[SAMPLE/2];
  if (distance_cm <=12)
    return 1;//org 0
  else if (distance_cm <=20)
    return 2; //org 1
  else if (distance_cm <= 35)
    return 3; //org 2
  else
    return -1;
  
}


int distance_short_front_left(){
  int distance_cm = 0;
  int distance_blocks = 0;
  for(int i=SAMPLE; i>0; i--){
      sensor_reading[i] = front_left.getDistance(true);
      delay(1);
    }
   for(int i=SAMPLE; i>0; i--){
      sensor_reading[i] = front_left.getDistance(true);
      delay(1);
    }
  sortArray(sensor_reading,SAMPLE);
//  sensor_distance_cm[1] = sensor_reading[SAMPLE/2];
//  if (DEBUG)
//  Serial.println(sensor_distance_cm[1]);

  distance_cm = sensor_reading[SAMPLE/2]-3;
  
  distance_blocks = (distance_cm /10.3)+1;
  if (distance_blocks > 2)
    return -1;
  return distance_blocks;
    
}

int distance_short_front_right(){
  int distance_cm = 0;
  int distance_blocks = 0;
  
  for(int i=SAMPLE; i>0; i--){
    sensor_reading[i] = front_right.getDistance(true);
    delay(1);
  }  
  for(int i=SAMPLE; i>0; i--){
    sensor_reading[i] = front_right.getDistance(true);
    delay(1);
  }
  sortArray(sensor_reading,SAMPLE);

  //sensor_distance_cm[2] = sensor_reading[SAMPLE/2];
  //if (DEBUG)
  //Serial.println(sensor_distance_cm[2]);

  distance_cm = sensor_reading[SAMPLE/2]-3;
  
  distance_blocks = (distance_cm /10.3)+1;
  if (distance_blocks > 2) //org larger than 1
    return -1;
  return distance_blocks;
}

int distance_short_right_front(){
  for(int i=SAMPLE; i>0; i--){
    sensor_reading[i] = right_front.getDistance(true);
    delay(1);
  }
    for(int i=SAMPLE; i>0; i--){
    sensor_reading[i] = right_front.getDistance(true);
    delay(1);
  }
  sortArray(sensor_reading,SAMPLE);

  //sensor_distance_cm[3] = sensor_reading[SAMPLE/2];
  distance_cm = sensor_reading[SAMPLE/2];
  if (distance_cm <= 15)
    return 1; //org 0
  else if (distance_cm <=25)
    return 2; //org 1
  else
    return -1;
  
   
}

int distance_short_right_back(){
  for(int i=SAMPLE; i>0; i--){
    sensor_reading[i] = right_back.getDistance(true);
    delay(1);
  }
    for(int i=SAMPLE; i>0; i--){
    sensor_reading[i] = right_back.getDistance(true);
    delay(1);
  }
  sortArray(sensor_reading,SAMPLE);

//  sensor_distance_cm[4] = sensor_reading[SAMPLE/2];
//
//  if (DEBUG)
//  Serial.println(sensor_distance_cm[4]);

  distance_cm = sensor_reading[SAMPLE/2];
  if (distance_cm <= 17)
    return 1; //org 0
  else if (distance_cm <=29)
    return 2; //org 1
  else return -1;
}

int distance_long_left(){
  for(int i=SAMPLE; i>0; i--){
    sensor_reading[i] = long_left.getDistance(true);
    //delay(1);
  }
  for(int i=SAMPLE; i>0; i--){
    sensor_reading[i] = long_left.getDistance(true);
    //delay(1);
  }
  sortArray(sensor_reading,SAMPLE);

//  sensor_distance_cm[5] = sensor_reading[SAMPLE/2];
//
//  if (DEBUG)
//  Serial.println(sensor_distance_cm[5]);

  distance_cm = sensor_reading[SAMPLE/2];
  if (distance_cm <= 19)
     return 1; //org 0
  else if (distance_cm <= 25)
     return 2; //org 1
  else if (distance_cm <= 34)
    return 3; //2 
  else if (distance_cm <= 43)
    return 4; //3
  else if (distance_cm <= 53)
    return 5; //4
  else
    return -1;
  
}

void read_all_sensors(){
  int sensor1,sensor2,sensor3,sensor4,sensor5,sensor6;
  sensor1 = distance_short_front_center();
  sensor2 = distance_short_front_left();
  sensor3 = distance_short_front_right();
  sensor4 = distance_short_right_front();
  sensor5 = distance_short_right_back();
  sensor6 = distance_long_left();
  Serial.print("SD|");
  Serial.print(sensor1); Serial.print(";");
  Serial.print(sensor2); Serial.print(";");
  Serial.print(sensor3); Serial.print(";");
  Serial.print(sensor4); Serial.print(";");
  Serial.print(sensor5); Serial.print(";");
  Serial.println(sensor6);
  
}



//method to aid in calibrating sensors to obtain characteristic of sensor
//void sensor_calibrate() {
//for(int i=SAMPLE; i>0; i--){
//    sensor_reading[i] = analogRead(front_center); //change sensor to be read
//  }
//sortArray(sensor_reading,SAMPLE);
//Serial.print("Analog Reading: "); Serial.println(sensor_reading[SAMPLE/2]);
//
//}
//+++++++++++++++++++++++++++++++++++++++++

//Methods for detect object in front of sensors, for clearance purpose ++++++++++++++++++++++++++++

//+++++++++++++++++++++++++++++++++++++++++
/*
 * ==========================================================================
 * Calibration
 * Methods to align wall, calibrate by front sensors, for turning at corner
 * ==========================================================================
 */

//Method for right wall alignment
void right_wall_calibrate(){
double difference = 0;
double distance_front = 0;
double distance_back = 0;
int i = 80;
  while(i > 0){

    for (int i = 10; i>0; i--){
      distance_front += right_front.getDistance(true)-5;
      distance_back += right_back.getDistance(true)-5;
    }
    distance_front /= 10;
    distance_back /= 10;
    
    difference = (distance_front - distance_back);
    if(DEBUG){
    Serial.println(" ");
    Serial.print("distance_front: ");
    Serial.print(distance_front);
    Serial.print("distance_back: ");
    Serial.println(distance_back);
    Serial.print("difference");
    Serial.println(difference);
    }
    delay(1);
    i--;                                          
    if(distance_front <5.5 || distance_back <5.5){ //If robot is too close to the rightwall
      rotate_right(90);
      delay(50);
      front_calibrate();
      delay(50);
      rotate_left(90);
      delay(100);
    }


    
    else if(difference >= 0.03 && distance_back < 25){ //If the robot tilts to the right 
      md.setSpeeds(rpm_to_speed_1(-15),rpm_to_speed_2(15));
      delay(15);
      md.setBrakes(50,50);
    }
    
    else if(difference <= -0.03 && distance_back  < 25){ //If the robot tilts to the left
      md.setSpeeds(rpm_to_speed_1(15),rpm_to_speed_2(-15));
      delay(15);
      md.setBrakes(50,50);
    }
    
    else{ // If difference is in between 0.035 to -0.035
      break;
    }
  }
  md.setBrakes(100,100);
  
}

//method to turn and realign when robot at corner or face obstacle
//NOTE: BY ALGO, ROBOT WILL TURN LEFT FROM ORGINAL DIRECTION. IF OBJECT/WALL IS INFRONT OF ROBOT, IT TURNS LEFT AND GO STRAIGHT TO AVOID OBSTACLES
void right_recalibrate(){
  
}

//Method to calibrate using front sensors
void front_calibrate(){
  double distance_left = 0;
  double distance_right = 0;
  double difference = 0;
  double ideal = 10.9;
  int k = 15;
  Serial.println("Front calibrating");
  
  while((distance_left != ideal || distance_right != ideal)&& k > 0){
    
    for (int i = 10; i>0; i--){
      distance_left += front_left.getDistance(true);
      distance_right += front_right.getDistance(true);
    }
    distance_left /= 10;
    distance_right /= 10;

    difference = distance_left-distance_right;
    Serial.print(distance_left);
    Serial.print("|");
    Serial.print(distance_right);
    Serial.print("|");
    Serial.println(difference);
    delay(1);
    k--;

    if(abs(difference) <= 0.5){
      Serial.println("Almost straight!");
      if(distance_left > ideal && distance_right > ideal){
      md.setSpeeds(rpm_to_speed_1(15),rpm_to_speed_2(15));
        if (DEBUG)
        {
          Serial.print(distance_left);
          Serial.print("|");
          Serial.println(distance_right);
          Serial.println("too far");
        }
      }
      else if(distance_left < ideal && distance_right < ideal){
        md.setSpeeds(rpm_to_speed_1(-15),rpm_to_speed_2(-15));
        if (DEBUG)
        {
          Serial.print(distance_left);
          Serial.print("|");
          Serial.println(distance_right);
          Serial.println("too close");
        }
      }
      else if (distance_left == ideal || distance_right == ideal)
        break;
   }
    
    else {
      Serial.println("Oof!");
      k = 3; //kick loop up
    if(difference>0.5){
      //slanted so rotate right
      md.setSpeeds(rpm_to_speed_1(-15), rpm_to_speed_2(15));
      if (DEBUG)
      {
        Serial.print(distance_left);
        Serial.print("|");
        Serial.println(distance_right);
        Serial.println("slanted to left");
      }
    }
    else if (difference < -0.5){
       //slanted so rotate left
      md.setSpeeds(rpm_to_speed_1(15), rpm_to_speed_2(-15));
      if (DEBUG)
      {
        Serial.print(distance_left);
        Serial.print("|");
        Serial.println(distance_right);
        Serial.println("slanted to right");
      }
    }
    delay(10);
    md.setBrakes(50,50);
   } 
 }
  md.setBrakes(100,100);
  delay(50);
  Serial.println("finished front calibrating");
}


/*
 * ==================================
 * Interrupt Service Routine
 * For counting ticks
 * ================================== 
 */
void E1_Pos(){
  tick1++;
}

void E2_Pos(){
  tick2++;
}

/*
 * ======================================================
 * Conversion
 * Methods to convert stuffs
 * ======================================================
 */

double rpm_to_speed_1(double RPM){
  if (RPM>0)
    return 2.8598*RPM + 51.582;
  else if (RPM == 0)
    return 0;
  else
    return -2.9117*(-1)*RPM - 45.197;
}

double rpm_to_speed_2(double RPM){
  if (RPM>0)
    return 2.7845*RPM + 53.789;
  else if (RPM == 0)
    return 0;
  else
    return -2.8109*(-1)*RPM - 54.221;
}

//convert analog reading to distance (CM)
double short_distance(int reading){
  return reading;
}

/*
 * ======================================================
 * Communication
 * Methods to help in communication with RPI
 * ======================================================
 */


//method to get command string into the buffer
void get_command(){
    int i = 0;
    while(Serial.available()>0){
       command[i] = Serial.read();
       i++;
       delay(2); //essential delay cause of serial being too slow
    }
    command[i] = '\0';

    //Debug print command
    if(DEBUG && command[0]!='\0'){
        Serial.print("COMMAND :");
        Serial.println(command);
    }
}

//method to print all characters of string received (for debug)
void print_all_commands(){
  int i = 0;
  Serial.println("Msg Received: ");
  while (command[i] != '\0'){
    Serial.print(command[i]);
    i++;
  }
  Serial.print("EndOfLine");
  Serial.println();
}

/*
 * ========================
 * Others
 * ========================
 *
 * PID Theory:
 * Kp: Proportional term, decide how fast and how strong the system response to a change. System should be in an oscillating state (turn left and right and left)
 * Kd: Differential term, decide how dampen the oscillation will be. Higher = more dampened
 * Ki: How strong the response is if error is accumulated. (hard turn in extreme case)
 * -------------------------------------------------
 * Differential PID with Master Slave configuration:
 * Master: Stable, slower Motor
 * Slave: Unstable, faster motor
 * We feed a speed into the system, then measure the actual speed of both motor. 
 * We then calculate the error in this speed and 
 * calculate the PID adjustment to be feed back so the error is balanced out.
 * Here, we want to control the speed of unstable motor (1) so that it is as close and stable to stable motor (2)'s speed.
 * We feedback to the system as PWM speed (speed1) of the unstable motor 1.
 * ===================================================
 */
