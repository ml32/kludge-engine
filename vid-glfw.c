#include "vid.h"

#include <stdio.h>

#include <GL/glfw.h>
#include "platform-glfw.h"

int kl_vid_init() {
  kl_glfw_init();

  glfwOpenWindowHint(GLFW_WINDOW_NO_RESIZE, GL_TRUE);
  glfwOpenWindowHint(GLFW_OPENGL_VERSION_MAJOR, 4);
  glfwOpenWindowHint(GLFW_OPENGL_VERSION_MINOR, 1);
  glfwOpenWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
  glfwOpenWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  
  //if (glfwOpenWindow(1280, 720, 8, 8, 8, 0, 24, 8, GLFW_WINDOW) < 0) {
  if (glfwOpenWindow(1920, 1080, 8, 8, 8, 0, 24, 8, GLFW_FULLSCREEN) < 0) {
    fprintf(stderr, "vid-glfw -> failed to create window\n");
    return -1;
  }

  glfwSetWindowTitle("Kludge Engine");

  glfwSwapInterval(0); /* VSync disabled */

  return 0;
}

void kl_vid_size(int *w, int *h) {
  glfwGetWindowSize(w, h);
}

void kl_vid_swap() {
  glfwSwapBuffers();
}
