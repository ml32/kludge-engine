#ifndef KL_TIME_H
#define KL_TIME_H

typedef struct kl_timer {
  float time, dt;
} kl_timer_t;

#define KL_TIMER_INIT {\
  .time = -1.0f, .dt = 0.0f\
}

int kl_time_init();

float kl_gettime();
float kl_timer_tick(kl_timer_t* timer);
float kl_timer_delta(kl_timer_t* timer);

#endif