#include "platform-glfw.h"

#include <stdbool.h>
#include <GL/glfw.h>

void kl_glfw_init() {
  static bool initialized = false;
  if (initialized) return;
  glfwInit();
  initialized = true;
}
