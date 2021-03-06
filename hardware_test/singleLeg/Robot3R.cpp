/********************************************************************
 * Author: Haoxiang Li (Colin)
 * 3R Robot
 * Source Code
 *******************************************************************/

#include "Arduino.h"
#include "Robot3R.h"
#include <Servo.h>

// servos
Servo hip0_servo;
Servo hip1_servo;
Servo knee_servo;

Robot3R::Robot3R(byte hip0_p, byte hip1_p, byte knee_p) {
  hip0_pin = hip0_p;
  hip1_pin = hip1_p;
  knee_pin = knee_p;
}

void Robot3R::start() {
  Serial.println("start robot in 3 second:");
  hip0_servo.attach(hip0_pin);
  hip1_servo.attach(hip1_pin);
  knee_servo.attach(knee_pin);
  servo_set_angs(current_theta);
  delay(3000);
}

/**
 * Convert Angle Radius to PWM us
 */
int Robot3R::convert_rad_to_pulse_us(double rad) {
  int us = SERVO_US_MIN + (SERVO_US_MAX - SERVO_US_MIN) * rad / 3.14;
  return us;
}

/**
 * Convert Angle Degree to PWM us
 */
int Robot3R::convert_ang_to_pulse_us(int16_t ang) {
  int us = SERVO_US_MIN + static_cast<double>(SERVO_US_MAX - SERVO_US_MIN) * ang / 180;
  return us;
}

void Robot3R::servo_set_angs(int16_t angs[]) {
  int16_t ang0 = INIT_ANG[0] + SERVO_DIR[0] * angs[0];
  int16_t ang1 = INIT_ANG[1] + SERVO_DIR[1] * angs[1];
  int16_t ang2 = INIT_ANG[2] + SERVO_DIR[2] * angs[2];
//  Serial.println(ang0);
//  Serial.println(ang1);
//  Serial.println(ang2);
  hip0_servo.writeMicroseconds(convert_ang_to_pulse_us(ang0));
  hip1_servo.writeMicroseconds(convert_ang_to_pulse_us(ang1));
  knee_servo.writeMicroseconds(convert_ang_to_pulse_us(ang2));
  delayMicroseconds(1000);
}

void Robot3R::run_period() {
  for (int i = 0; i < 100; ++i){
    for (int j = 0; j < 3; ++j) {
      current_theta[j] = task[i][j];
    }
    
    servo_set_angs(current_theta);
    delay(50);
  }
}
