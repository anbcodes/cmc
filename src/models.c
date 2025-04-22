#include "models.h"
#include <yyjson.h>
#include <cglm/cglm.h>
#include "chunk.h"
#include "datatypes.h"
#include "logging.h"
#include "macros.h"
#include "texture_sheet.h"
#include "mcapi/protocol.h"

void read_json_arr_as_vec4(vec4 dst, yyjson_val * src) {
  if (src) {
    dst[0] = yyjson_get_num(yyjson_arr_get(src, 0));
    dst[1] = yyjson_get_num(yyjson_arr_get(src, 1));
    dst[2] = yyjson_get_num(yyjson_arr_get(src, 2));
    dst[3] = yyjson_get_num(yyjson_arr_get(src, 3));
  } else {
    dst[0] = 0;
    dst[1] = 0;
    dst[2] = 16;
    dst[2] = 16;
  }
}

uint16_t lookup_model_texture(yyjson_mut_val * textures, const char* texture_name, BlockTextureSheet* texture_sheet) {
  static WritableBuffer texture_cache = { 0 };
  if (texture_cache.buf.buffer.ptr == NULL) {
    texture_cache = create_writable_buffer(1024*4);
  }

  if (texture_name[0] == '#') {
    texture_name = yyjson_mut_get_str(yyjson_mut_obj_get(textures, texture_name + 1));
    if (strncmp(texture_name, "minecraft:", 10) == 0) {
      texture_name = texture_name + 10;
    }
  }

  int found_index = -1;
  for (int i = texture_cache.cursor-1; i > 0;) {
    uint8_t len = texture_cache.buf.buffer.ptr[i];
    uint16_t texture_index = (texture_cache.buf.buffer.ptr[i-2] << 8) + texture_cache.buf.buffer.ptr[i-1];
    char* str = (char*)texture_cache.buf.buffer.ptr + i - 3 - len + 1;
    if (len - 1 == strlen(texture_name) && strncmp(str, texture_name, MIN(len, strlen(texture_name))) == 0) {
      found_index = texture_index;
      break;
    }
    i -= 3 + len;
  }

  if (found_index != -1) {
    if (!strcmp(texture_name, "block/grass_block_side_overlay")) {
      DEBUG("Overlay was already loaded");
    }
    return found_index;
  }

  if (!strcmp(texture_name, "block/grass_block_side_overlay")) {
    DEBUG("Should be loading overlay");
  }
  char fname[1000];
  snprintf(fname, 1000, "data/assets/minecraft/textures/%s.png", texture_name);
  uint16_t index = block_texture_sheet_add_file(texture_sheet, fname);
  write_buffer(&texture_cache, string_to_buffer(texture_name));
  write_byte(&texture_cache, 0);
  write_short(&texture_cache, index);
  write_byte(&texture_cache, strlen(texture_name) + 1);

  return index;
}

void read_face_from_json(yyjson_val* faces, char* face_name, MeshFace* into, yyjson_mut_val *textures, BlockTextureSheet* texture_sheet) {
  yyjson_val *face = yyjson_obj_get(faces, face_name);

  if (face == NULL) {
    return;
  }

  read_json_arr_as_vec4(into->uv, yyjson_obj_get(face, "uv"));
  into->texture = lookup_model_texture(textures, yyjson_get_str(yyjson_obj_get(face, "texture")), texture_sheet);
  yyjson_val* tint_index_j = yyjson_obj_get(face, "tintindex");
  into->tint_index = tint_index_j != NULL ? yyjson_get_num(tint_index_j) + 1 : 0;
  // TODO: Figure out how cullface actually works
  into->cull = yyjson_obj_get(face, "cullface") != NULL;
}

// Adds the elements array to the mesh, each element is a MeshCubiod. Returns the number of elements added.
int add_elements_to_blockinfo(BlockInfo* info, BlockTextureSheet* texture_sheet, yyjson_val* elements, yyjson_mut_val* textures) {
  size_t old_count = info->mesh.num_elements;
  info->mesh.num_elements = old_count + yyjson_arr_size(elements);
  size_t start_index = 0;

  if (old_count != 0) {
    // We need to copy the old elements
    MeshCuboid* old = info->mesh.elements;

    info->mesh.elements = calloc(info->mesh.num_elements, sizeof(MeshCuboid));
    memcpy(info->mesh.elements, old, sizeof(MeshCuboid) * old_count);

    start_index = old_count;

    free(old);
  } else {
    info->mesh.elements = calloc(info->mesh.num_elements, sizeof(MeshCuboid));
  }
  if (info->mesh.num_elements == 0) {
    return 0;
  }
  size_t arr_index, max;
  yyjson_val* cur_element;
  yyjson_arr_foreach(elements, arr_index, max, cur_element) {
    MeshCuboid cubiod = {0};
    yyjson_val* jfrom = yyjson_obj_get(cur_element, "from");
    yyjson_val* jto = yyjson_obj_get(cur_element, "to");
    yyjson_val* jfaces = yyjson_obj_get(cur_element, "faces");
    cubiod.from[0] = yyjson_get_num(yyjson_arr_get(jfrom, 0));
    cubiod.from[1] = yyjson_get_num(yyjson_arr_get(jfrom, 1));
    cubiod.from[2] = yyjson_get_num(yyjson_arr_get(jfrom, 2));

    cubiod.to[0] = yyjson_get_num(yyjson_arr_get(jto, 0));
    cubiod.to[1] = yyjson_get_num(yyjson_arr_get(jto, 1));
    cubiod.to[2] = yyjson_get_num(yyjson_arr_get(jto, 2));

    read_face_from_json(jfaces, "up", &cubiod.up, textures, texture_sheet);
    read_face_from_json(jfaces, "down", &cubiod.down, textures, texture_sheet);
    read_face_from_json(jfaces, "north", &cubiod.north, textures, texture_sheet);
    read_face_from_json(jfaces, "south", &cubiod.south, textures, texture_sheet);
    read_face_from_json(jfaces, "east", &cubiod.east, textures, texture_sheet);
    read_face_from_json(jfaces, "west", &cubiod.west, textures, texture_sheet);

    info->mesh.elements[start_index + arr_index] = cubiod;
  }

  return info->mesh.num_elements - old_count;
}

void rotate(float *x, float *y, int angle) {
  float tx = *x;
  float ty = *y;
  if (angle == 90) {
    *x = -(ty - 8) + 8;
    *y = tx;
  } else if (angle == 180) {
    *x = -(tx - 8) + 8;
    *y = -(ty - 8) + 8;
  } else if (angle == 270) {
    *x = ty;
    *y = -(tx - 8) + 8;
  }
}

void order(float *x, float *y) {
  if (*x > *y) {
    float temp = *x;
    *x = *y;
    *y = temp;
  }
}

yyjson_doc *load_json(const char *filename) {
  yyjson_doc* json = yyjson_read_file(filename, 0, NULL, NULL);

  return json;
}

void load_model(yyjson_val* model_spec, BlockInfo* info, BlockTextureSheet* texture_sheet) {
  char fname[1000];

  yyjson_val *model_name = yyjson_obj_get(model_spec, "model");
  const char *model_name_str = yyjson_get_str(model_name);
  if (model_name_str == NULL) {
    WARN("model property not found for %s!", info->name);
    return;
  }

  if (strncmp(model_name_str, "minecraft:", 10) == 0) {
    model_name_str += 10;
  }
  snprintf(fname, 1000, "data/assets/minecraft/models/%s.json", model_name_str);
  yyjson_doc *model = load_json(fname);
  if (model == NULL) {
    WARN("model not found %s", fname);
    return;
  }
  // Read the hierarchy
  int num_read_elements = 0;
  yyjson_mut_doc *textures_doc = yyjson_mut_doc_new(NULL);
  yyjson_mut_val *textures = yyjson_mut_obj(textures_doc);
  yyjson_mut_doc_set_root(textures_doc, textures);
  yyjson_doc *parent_model = model;
  yyjson_val *parent_model_root = yyjson_doc_get_root(parent_model);
  while (parent_model) {
    // We need to read the textures in each time
    yyjson_val * parent_textures = yyjson_obj_get(parent_model_root, "textures");

    if (parent_textures != NULL) {
      size_t ind, max;
      yyjson_val* ptexture;
      yyjson_val* ptexture_name;
      yyjson_obj_foreach(parent_textures, ind, max, ptexture_name, ptexture) {
        const char* ptexture_value = yyjson_get_str(ptexture);
        if (ptexture_value[0] == '#') {
          // Look up real texture
          const char* t = yyjson_mut_get_str(yyjson_mut_obj_get(textures, ptexture_value + 1));
          ptexture_value = t;
        }
        // DEBUG("yyjson=%s", yyjson_get_str(ptexture_name));
        const char* ptexture_name_str = yyjson_get_str(ptexture_name);

        yyjson_mut_obj_put(textures, yyjson_mut_strcpy(textures_doc, ptexture_name_str), yyjson_mut_strcpy(textures_doc, ptexture_value));
      }
    }

    // Other than that, it's just parsing the elements
    yyjson_val* elements = yyjson_obj_get(parent_model_root, "elements");
    if (elements != NULL && num_read_elements == 0) {
      num_read_elements = add_elements_to_blockinfo(info, texture_sheet, elements, textures);
    }

    yyjson_val *parent_item = yyjson_obj_get(parent_model_root, "parent");
    if (parent_item != NULL) {
      const char* parent_name = yyjson_get_str(parent_item);
      if (strncmp(parent_name, "minecraft:", 10) == 0) {
        parent_name += 10;
      }
      snprintf(fname, 1000, "data/assets/minecraft/models/%s.json", parent_name);
      if (parent_model != model) {
        yyjson_doc_free(parent_model);
      }
      parent_model = load_json(fname);
      parent_model_root = yyjson_doc_get_root(parent_model);
    } else {
      parent_model = NULL;
    }
  }
  yyjson_mut_doc_free(textures_doc);

  // Uncomment to skip rotations
  // return;

  // Perform rotation
  yyjson_val *y_rotation_j = yyjson_obj_get(model_spec, "y");
  int y_rotation = 0;
  if (y_rotation_j != NULL) {
    y_rotation = yyjson_get_num(y_rotation_j);
  }

  for (size_t i = info->mesh.num_elements - num_read_elements; i < info->mesh.num_elements; i++) {
    MeshCuboid* el = &info->mesh.elements[i];
    rotate(el->up.uv + 0, el->up.uv + 1, y_rotation);
    rotate(el->up.uv + 2, el->up.uv + 3, y_rotation);
    rotate(el->down.uv + 0, el->down.uv + 1, 360 - y_rotation);
    rotate(el->down.uv + 2, el->down.uv + 3, 360 - y_rotation);
    // rotate(el->north.uv + 0, el->north.uv + 1, y_rotation);
    // rotate(el->north.uv + 2, el->north.uv + 3, y_rotation);
    // rotate(el->south.uv + 0, el->south.uv + 1, y_rotation);
    // rotate(el->south.uv + 2, el->south.uv + 3, y_rotation);
    // rotate(el->east.uv + 0, el->east.uv + 1, y_rotation);
    // rotate(el->east.uv + 2, el->east.uv + 3, y_rotation);
    // rotate(el->west.uv + 0, el->west.uv + 1, y_rotation);
    // rotate(el->west.uv + 2, el->west.uv + 3, y_rotation);
    rotate(el->from + 0, el->from + 2, y_rotation);
    rotate(el->to + 0, el->to + 2, y_rotation);
    order(el->from + 0, el->to + 0);
    order(el->from + 2, el->to + 2);

    if (y_rotation == 90) {
      MeshFace north = el->north;
      el->north = el->east;
      el->east = el->south;
      el->south = el->west;
      el->west = north;
    } else if (y_rotation == 180) {
      MeshFace north = el->north;
      el->north = el->south;
      el->south = north;
      MeshFace east = el->east;
      el->east = el->west;
      el->west = east;
    } else if (y_rotation == 270) {
      MeshFace north = el->north;
      el->north = el->west;
      el->west = el->south;
      el->south = el->east;
      el->east = north;
    }
  }
}

void load_multipart_state(yyjson_val *state, BlockInfo* info, BlockTextureSheet* texture_sheet, yyjson_val *multipart) {
  yyjson_val *properties = yyjson_obj_get(state, "properties");

  // Iterate through multiparts
  // Each one can add elements if its state properties matches the "when" clause
  yyjson_val* part;
  size_t index, max;
  yyjson_arr_foreach(multipart, index, max, part) {
    bool all_properties_match = true;
    yyjson_val* when = yyjson_obj_get(part, "when");
    if (when != NULL) {
      yyjson_val* condition_key, * condition_value;
      size_t index, max;
      yyjson_obj_foreach(when, index, max, condition_key, condition_value) {
        yyjson_val* prop = yyjson_obj_get(properties, yyjson_get_str(condition_key));
        all_properties_match = yyjson_equals(prop, condition_value);
        if (!all_properties_match) {
          break;
        }
      }
    }

    if (!all_properties_match) {
      continue;
    }

    yyjson_val* apply = yyjson_obj_get(part, "apply");
    if (yyjson_is_arr(apply)) {
      // Pick the first element if there are mulitple options for the variant
      apply = yyjson_arr_get(apply, 0);
    }

    load_model(apply, info, texture_sheet);
  }
}


void load_variant_state(yyjson_val* state, BlockInfo* info, BlockTextureSheet* texture_sheet,yyjson_val* variants) {
  char key_buffer[1000];
  char value_buffer[1000];

  yyjson_val* state_id = yyjson_obj_get(state, "id");
  yyjson_val* properties = yyjson_obj_get(state, "properties");

  // Iterate through variants
  // Parse the object key for property values "name=value,name=value,...,name=value"
  // For each name, make sure properties has the same property value
  size_t index, max;
  yyjson_val* variant_key, *variant_value;
  variant_value = yyjson_obj_get(variants, "");
  if (properties != NULL && variant_value == NULL) {
    yyjson_obj_foreach(variants, index, max, variant_key, variant_value) {
      bool all_properties_match = true;
      const char *ch = yyjson_get_str(variant_key);
      while (*ch != '\0') {
        const char *end = ch;
        while (*end != '=') {
          end += 1;
        }
        memcpy(key_buffer, ch, end - ch);
        key_buffer[end - ch] = '\0';
        ch = end + 1;
        end = ch;
        while (*end != ',' && *end != '\0') {
          end += 1;
        }
        memcpy(value_buffer, ch, end - ch);
        value_buffer[end - ch] = '\0';
        if (*end == ',') {
          ch = end + 1;
        } else {
          ch = end;
        }
        yyjson_val* property_value = yyjson_obj_get(properties, key_buffer);
        if (property_value == NULL) {
          all_properties_match = false;
          break;
        }
        if (yyjson_is_true(property_value) && strcmp(value_buffer, "true")) {
          all_properties_match = false;
          break;
        }
        if (yyjson_is_false(property_value) && strcmp(value_buffer, "false")) {
          all_properties_match = false;
          break;
        }
        if (yyjson_is_str(property_value) && strcmp(value_buffer, yyjson_get_str(property_value))) {
          all_properties_match = false;
          break;
        }
        if (yyjson_get_int(property_value) && atoi(value_buffer) != yyjson_get_int(property_value)) {
          all_properties_match = false;
          break;
        }
      }
      if (all_properties_match) {
        break;
      }
    }
  }

  if (variant_value == NULL) {
    WARN("Failed to find variant for state %d", yyjson_get_int(state_id));
    assert(false);
    return;
  }

  if (yyjson_is_arr(variant_value)) {
    // Pick the first element if there are mulitple options for the variant
    variant_value = yyjson_arr_get(variant_value, 0);
  }

  load_model(variant_value, info, texture_sheet);
}

void load_block_states(BlockInfo* block_info, BlockTextureSheet* texture_sheet, const char* block_name, yyjson_val* block) {
  char fname[1000];

  BlockInfo shared_info = {0};
  if (strncmp(block_name, "minecraft:", 10) == 0) {
    block_name += 10;
  }

  yyjson_val* definition = yyjson_obj_get(block, "definition");
  yyjson_val* type_json = yyjson_obj_get(definition, "type");
  const char* type = yyjson_get_str(type_json);
  // TODO deal with this correctly, these pointers get copied into a bunch of spots
  // A mempool for all the blockstates could work well, or just ignore it and assume we never free them
  shared_info.type = copy_string(type);
  shared_info.name = copy_string(block_name);
  if (
    strcmp(type, "minecraft:air") == 0 ||
    strcmp(type, "minecraft:flower") == 0 ||
    strcmp(type, "minecraft:vine") == 0 ||
    strcmp(type, "minecraft:dry_vegetation") == 0 ||
    strcmp(type, "minecraft:firefly_bush") == 0 ||
    strcmp(type, "minecraft:mushroom") == 0 ||
    strcmp(type, "minecraft:liquid") == 0 ||
    strcmp(type, "minecraft:seagrass") == 0 ||
    strcmp(type, "minecraft:tall_seagrass") == 0 ||
    strcmp(type, "minecraft:flower_bed") == 0 ||
    strcmp(type, "minecraft:bush") == 0
  ) {
    shared_info.passable = true;
    shared_info.transparent = true;
  }
  if (strcmp(type, "minecraft:leaf_litter") == 0) {
    shared_info.passable = true;
    shared_info.transparent = true;
    shared_info.dry_foliage = true;
  }
  if (strcmp(type, "minecraft:grass") == 0) {
    shared_info.grass = true;
  }
  if (
    strcmp(type, "minecraft:tall_grass") == 0 ||
    strcmp(type, "minecraft:double_plant") == 0 ||
    strcmp(type, "minecraft:sugar_cane") == 0 ||
    strcmp(type, "minecraft:bush") == 0
  ) {
    shared_info.passable = true;
    shared_info.transparent = true;
    shared_info.grass = true;
  }
  if (
    strcmp(type, "minecraft:leaves") == 0 ||
    strcmp(type, "minecraft:tinted_particle_leaves") == 0 ||
    strcmp(type, "minecraft:waterlily") == 0 ||
    strcmp(type, "minecraft:vine") == 0
  ) {
    shared_info.transparent = true;
    shared_info.foliage = true;
  }

  snprintf(fname, 1000, "data/assets/minecraft/blockstates/%s.json", block_name);
  yyjson_doc* blockstate_doc = load_json(fname);
  if (blockstate_doc == NULL) {
    WARN("blockstate not found for %s", block_name);
    return;
  }
  yyjson_val* blockstate = yyjson_doc_get_root(blockstate_doc);


  yyjson_val* multipart = yyjson_obj_get(blockstate, "multipart");
  yyjson_val* variants = yyjson_obj_get(blockstate, "variants");

  yyjson_val* states = yyjson_obj_get(block, "states");
  if (states != NULL) {
    yyjson_val* state;
    size_t index, max;
    yyjson_arr_foreach(states, index, max, state) {
      int id = -1;
      yyjson_val* state_id = yyjson_obj_get(state, "id");
      if (state_id != NULL && yyjson_is_int(state_id)) {
        id = yyjson_get_int(state_id);
      } else {
        WARN("No state id");
      }

      BlockInfo info = shared_info;
      info.state = id;
      if (multipart != NULL) {
        load_multipart_state(state, &info, texture_sheet, multipart);
      } else if (variants != NULL) {
        load_variant_state(state, &info, texture_sheet, variants);
      } else {
        WARN("Should be multipart or variant");
        assert(false);
      }

      // Calculate if it is a full block
      info.fullblock = info.mesh.num_elements != 0;
      for (size_t el = 0; el < info.mesh.num_elements; el++) {
        if (!(glm_vec3_eq(info.mesh.elements[el].from, 0) && glm_vec3_eq(info.mesh.elements[el].to, 16))) {
          info.fullblock = false;
          break;
        }
      }

      // Save the block state
      if (id != -1) {
        block_info[id] = info;
      }
    }
  } else {
    WARN("No states for %s", shared_info.name);
  }
  yyjson_doc_free(blockstate_doc);
}

void load_blocks(BlockInfo* block_info, BlockTextureSheet* texture_sheet) {
  yyjson_doc* blocks_doc = load_json("data/blocks.json");
  yyjson_val* blocks = yyjson_doc_get_root(blocks_doc);
  yyjson_val* block_name;
  yyjson_val* block_value;
  size_t index, max;
  yyjson_obj_foreach(blocks, index, max, block_name, block_value) {
    load_block_states(block_info, texture_sheet, yyjson_get_str(block_name), block_value);
  }
  yyjson_doc_free(blocks_doc);
}

void load_entity_textures(EntityTextureSheet *texture_sheet) {
  INFO("Starting entity loading");
}
