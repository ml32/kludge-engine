#include "time.h"

#include "platform-glfw.h"

int kl_time_init() {
  kl_glfw_init();
}

float kl_gettime() {
  return glfwGetTime();
}

float kl_timer_tick(kl_timer_t* timer) {
  float time = kl_gettime();
  if (timer->time < 0.0f) {
    timer->time = time;
    timer->dt   = 0.0f;
    return 0.0f;
  }
  float dt = time - timer->time;
  timer->time = time;
  timer->dt   = dt;
  return dt;
}

float kl_timer_delta(kl_timer_t* timer) {
  return timer->dt;
}