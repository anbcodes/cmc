#include "entity.h"

#include <stdlib.h>
#include <GLFW/glfw3.h>

void entity_destroy(Entity *entity) {
  free(entity);
}

void entity_move(Entity *entity, vec3 to) {
  // vel = (dx/dt) = (curr - prev) / (curr_time - prev_time)
  glm_vec3_sub(to, entity->last_pos, entity->vel);
  glm_vec3_divs(entity->vel, glfwGetTime() - entity->last_pos_time, entity->vel);

  glm_vec3_copy(entity->pos, entity->last_pos);
  glm_vec3_copy(to, entity->pos);
  entity->delta_time = glfwGetTime() - entity->last_pos_time;
  entity->last_pos_time = glfwGetTime();
}

void entity_move_relative(Entity *entity, vec3 delta) {
  vec3 to;
  glm_vec3_add(entity->pos, delta, to);
  entity_move(entity, to);
}
