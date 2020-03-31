
#include <Arduino.h>
#include <math.h>
#include "QuadrupedRobot.h"

/**
 * Switch on the robot
 */
void QuadrupedRobot::switch_on()
{
  Serial.println("Robot Switch On");
  setup_servos();
  bot_rest();
  delay(2000);
  shut_servos();
  clear_cmd();
}

/**
 * Switch off the robot
 */
void QuadrupedRobot::switch_off()
{
  Serial.println("Robot Switch Off");
  shut_servos();
  clear_cmd();
}

void QuadrupedRobot::clear_cmd()
{
  command_code = -1;
}

void QuadrupedRobot::setup_servos()
{
  for (size_t i = 0; i < DOF; i++)
  {
    if (servos_ptr[i] == nullptr)
      servos_ptr[i] = new Servo();
    servos_ptr[i]->attach(servo_pins[i], SERVOMIN, SERVOMAX);
  }
}

void QuadrupedRobot::shut_servos()
{
  for (size_t i = 0; i < DOF; i++)
  {
    if (servos_ptr[i] != nullptr)
    {
      servos_ptr[i]->detach();
    }
  }
}

/**
 * Check Interrupts
 * New Command from Serial Port
 */
bool QuadrupedRobot::interrupt()
{
  String serial_in_str {""};
  while(Serial.available() > 0) {
    char inChar = Serial.read();
    serial_in_str += (char)inChar;
    // wait for input buffer read
    delay(10);
  }
  if(serial_in_str != "")
  {
    Serial.print("Command received: ");
    Serial.println(serial_in_str);
    command_code = serial_in_str.toInt();
  }
}

void QuadrupedRobot::parse_code_run()
{
  switch(command_code) {
    case 1:
      switch_on();
      break;
    case 2:
      bot_stand();
      break;
    case 3:
      bot_walk();
      break;
    case 0:
      switch_off();
      break;
    case 998:
      adjust();
      break;
    default:
      ;// doing nothing
  }
}

/**
 * Synchronous Servo Move
 * Every servo should move to the desired position at the end of the timestep
 */
void QuadrupedRobot::servo_move(float *angs_ptr)
{
  unsigned long starttime = millis();
  unsigned long timenow = starttime;
  unsigned long servo_time[DOF] {}; // Timestamp of servo
  float joint_angs_now[DOF] {}; 
  float joint_degs_speed[DOF] {};
  for (size_t i = 0; i < DOF; i++)
  {
    joint_degs_speed[i] = (angs_ptr[i] - joint_angs_pre[i]) / timestep;
    joint_angs_now[i] = joint_angs_pre[i]; // Reset Joint angles
    servo_time[i] = starttime; // Reset Servo Time
  }
  while((timenow - starttime) < timestep) {
    for (size_t i = 0; i < DOF; i++)
    {
      timenow = millis();
      joint_angs_now[i] += joint_degs_speed[i] * (timenow - servo_time[i]);
      servo_time[i] = timenow; // update servo time
    }
    servo_write_angs(joint_angs_now, false);
    timenow = millis();
  }
  // last run to assure that all servo has been desired position
  for (size_t i = 0; i < DOF; i++)
  {
    servo_write_angs(angs_ptr, false);
  }
}

/**
 * Servo Write Current Angles
 * Current Angles is in Callibrated Coordinate,
 * which should be transfered to real angle degrees before using.
 */
void QuadrupedRobot::servo_write_angs(float *angs_ptr, bool is_in_degrees)
{
  float real_ang{};
  float pulsewidth{};
  float ang_in_degree{};
  for (size_t i = 0; i < DOF; i++)
  {
    if (is_in_degrees){
      ang_in_degree = angs_ptr[i];
    } else {
      ang_in_degree = angs_ptr[i] * 180 / M_PI;
    }
    real_ang = init_servo_deg[i] + servo_dir[i] * ang_in_degree;
    pulsewidth = 500 + real_ang / 90.0 * 1000;
    if (!servos_ptr[i]->attached())
    {
      servos_ptr[i]->attach(servo_pins[i], SERVOMIN, SERVOMAX);
    }
    servos_ptr[i]->writeMicroseconds(pulsewidth);
    // update current angles buffer
    if (is_in_degrees) {
      joint_angs_pre[i] = angs_ptr[i] * M_PI / 180;
    } else {
      joint_angs_pre[i] = angs_ptr[i];
    }
  }
}

/**
 * Moving body by moving the feet in opposite direction
 * Body Frame xyz
 */
void QuadrupedRobot::body_xyz(float x, float y, float z)
{
  for (size_t i = 0; i < 4; i++)
  {
    foot_pos[3 * i] = -x;
    foot_pos[3 * i + 1] = -y;
    foot_pos[3 * i + 2] = -z;
  }
  inverse_kinematics();
  servo_move(joint_angs_new);
}

/**
 * Calculate the joint angles by foot positions
 */
void QuadrupedRobot::inverse_kinematics()
{
  // FL 0, 1, 2
  joint_angs_new[0] = gamma_left(foot_pos[1], foot_pos[2]); // HipX
  vlegs_len[0] = vleg_left(foot_pos[0], foot_pos[1], joint_angs_new[0]);
  joint_angs_new[2] = beta_front(vlegs_len[0]);
  joint_angs_new[1] = alfa_front(foot_pos[0], joint_angs_new[2], vlegs_len[0]);
  // FR 3, 4, 5
  joint_angs_new[3] = gamma_right(foot_pos[4], foot_pos[5]);
  vlegs_len[3] = vleg_right(foot_pos[3], foot_pos[4], joint_angs_new[3]);
  joint_angs_new[5] = beta_front(vlegs_len[3]);
  joint_angs_new[4] = alfa_front(foot_pos[3], joint_angs_new[5], vlegs_len[3]);
  // RR 6, 7, 8
  joint_angs_new[6] = gamma_right(foot_pos[7], foot_pos[8]);
  vlegs_len[6] = vleg_right(foot_pos[6], foot_pos[7], joint_angs_new[6]);
  joint_angs_new[8] = beta_rear(vlegs_len[6]);
  joint_angs_new[7] = alfa_rear(foot_pos[6], joint_angs_new[8], vlegs_len[6]);
  // RL 9, 10, 11
  joint_angs_new[9] = gamma_left(foot_pos[10], foot_pos[11]);
  vlegs_len[9] = vleg_left(foot_pos[9], foot_pos[10], joint_angs_new[9]);
  joint_angs_new[11] = beta_rear(vlegs_len[9]);
  joint_angs_new[10] = alfa_rear(foot_pos[9], joint_angs_new[11], vlegs_len[9]);
}

// Gamma: Hip Z angle
float QuadrupedRobot::gamma_left(float dy, float dz)
{
  float res = atan((toe_out0 - dz) / (height0 - dy)) - gamma0;
  return res;
}

float QuadrupedRobot::gamma_right(float dy, float dz)
{
  float res = gamma0 - atan((toe_out0 + dz) / (height0 - dy));
  return res;
}
// virtual leg length
float QuadrupedRobot::vleg_left(float dx, float dy, float gamma)
{
  float res = sqrt(pow(vleg_len0 - (dy / cos(gamma0 + gamma)), 2) + pow(dx, 2));
  if (res > thigh + calf)
    res = thigh + calf;
  return res;
}

float QuadrupedRobot::vleg_right(float dx, float dy, float gamma)
{
  float res = sqrt(pow(vleg_len0 - (dy / cos(gamma0 - gamma)), 2) + pow(dx, 2));
  if (res > thigh + calf)
    res = thigh + calf;
  return res;
}

float QuadrupedRobot::beta_front(float vleg_len)
{
  float res = M_PI - acos(
      (thigh * thigh + calf * calf - vleg_len * vleg_len) /
      (2 * thigh * calf));
  return res;
}

float QuadrupedRobot::beta_rear(float vleg_len)
{
  float res = acos(
      (thigh * thigh + calf * calf - vleg_len * vleg_len) /
      (2 * thigh * calf)) - M_PI;
  return res;
}

float QuadrupedRobot::alfa_front(float dx, float beta, float vleg_len)
{
  float res = asin(dx / vleg_len);
  res -= acos(
    (thigh * thigh + vleg_len * vleg_len - calf * calf) /
    (2 * thigh * vleg_len)
  );
  return res;
}

float QuadrupedRobot::alfa_rear(float dx, float beta, float vleg_len)
{
  float res = asin(dx / vleg_len);
  res += acos(
    (thigh * thigh + vleg_len * vleg_len - calf * calf) /
    (2 * thigh * vleg_len)
  );
  return res;
}

/**
 * Only used for adjusting legs
 */
void QuadrupedRobot::adjust()
{
  servo_write_angs(adjust_angs, true);
  clear_cmd();
}

/**
 * Stand up
 * Non-periodic
 */
void QuadrupedRobot::bot_stand()
{
  servo_write_angs(stand_angs, true);
  clear_cmd();
}

/**
 * Sleep mode
 * Non-periodic
 */
void QuadrupedRobot::bot_rest()
{
  servo_write_angs(rest_angs, true);
  clear_cmd();
}

/**
 * walk
 * Periodic
 */
void QuadrupedRobot::bot_walk()
{
  Serial.println("walk");
  body_xyz(0, 0, 0);
  clear_cmd();
}

QuadrupedRobot::~QuadrupedRobot()
{
  for (size_t i = 0; i < DOF; i++)
  {
    delete servos_ptr[i];
  }
}
