#include "entity.h"

#include <alloca.h>
#include <assert.h>
#include <stdlib.h>
#include <GLFW/glfw3.h>
#include <yyjson.h>
#include "texture_sheet.h"

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

void entity_update_instance_buffer(Entity *entity, int index, WGPUQueue queue, WGPUBuffer instance_buffer) {
  EntityInstance entity_instance;
  glm_vec3_copy(entity->pos, entity_instance.pos);
  glm_vec3_copy(entity->last_pos, entity_instance.last_pos);
  entity_instance.last_pos_time = entity->last_pos_time;
  entity_instance.delta_time = entity->delta_time;
  wgpuQueueWriteBuffer(queue, instance_buffer, sizeof(EntityInstance) * index, &entity_instance, sizeof(EntityInstance));
}

void entity_move_relative(Entity *entity, vec3 delta) {
  vec3 to;
  glm_vec3_add(entity->pos, delta, to);
  entity_move(entity, to);
}

#define ENTITY_PATH "data/assets/minecraft/textures/entity/"

// void entity_render_cubiod(EntityInstance* instance, Entity* entity, EntityInfo* info, int sx, int sy, int ex, int ey, vec4 uvs[]) {

// }

// void entity_render_cow(WGPUQueue queue, WGPUBuffer instance_buffer, int start_index, EntityInfo *info, Entity* entity) {
//   EntityInstance *instances = alloca(info->instance_count * sizeof(EntityInstance));
//   int c = 0;
//   entity_render_cubiod(&instances[c++], entity, info, 0, 0, 10, 10, (vec4[]){
//     {0, 0, 10, 10},
//     {0, 0, 10, 10},
//     {0, 0, 10, 10},
//     {0, 0, 10, 10},
//     {0, 0, 10, 10},
//     {0, 0, 10, 10},
//   });

//   wgpuQueueWriteBuffer(queue, instance_buffer, sizeof(EntityInstance) * start_index, &instances, info->instance_count * sizeof(EntityInstance));
// }

void entity_register_cow(EntityInfo entity_info[], EntityTextureSheet* textures, yyjson_val *registry) {
  yyjson_val* info = yyjson_obj_get(registry, "minecraft:cow");
  int index = yyjson_get_int(yyjson_obj_get(info, "protocol_id"));

  entity_info[index].id = index;
  entity_info[index].name = "cow";
  glm_ivec2_copy(textures->current_pos, entity_info[index].texture_start);
  entity_texture_sheet_add_file(textures, ENTITY_PATH "cow/temperate_cow.png");
}

void entity_register_pig(EntityInfo entity_info[], EntityTextureSheet* textures, yyjson_val *registry) {
  yyjson_val* info = yyjson_obj_get(registry, "minecraft:pig");
  int index = yyjson_get_int(yyjson_obj_get(info, "protocol_id"));

  entity_info[index].id = index;
  entity_info[index].name = "pig";
  glm_ivec2_copy(textures->current_pos, entity_info[index].texture_start);
  entity_texture_sheet_add_file(textures, ENTITY_PATH "pig/temperate_pig.png");
}

void entity_register_entities(EntityInfo entity_info[], EntityTextureSheet* textures) {
  yyjson_doc* json = yyjson_read_file("data/registries.json", 0, NULL, NULL);
  assert(json != NULL);
  yyjson_val* root = yyjson_doc_get_root(json);
  assert(root != NULL);
  yyjson_val* entities = yyjson_obj_get(yyjson_obj_get(root, "minecraft:entity_type"), "entries");
  assert(entities != NULL);

  entity_register_cow(entity_info, textures, entities);
  entity_register_pig(entity_info, textures, entities);

  yyjson_doc_free(json);
}
