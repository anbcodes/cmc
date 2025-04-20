#include "entity.h"

#include <stdlib.h>

void entity_destroy(Entity *entity) {
  free(entity);
}
