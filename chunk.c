#include <stdbool.h>
#include <stdio.h>
#include "chunk.h"
#include "framework.h"

// Max quads per chunk section times 4 vertices per quad times floats per vertex
static float quads[(16 * 16 * 16 * 3) * 4 * FLOATS_PER_VERTEX];

int face_material_between(int a, int b) {
  if (a == 0 && b == 0) {
    return 0;
  }
  if (a == 0 && b != 0) {
    return -b;
  }
  if (a != 0 && b == 0) {
    return a;
  }
  // At this point, neither a or b are air
  // TODO: Need to lookup block transparency
  bool ta = false;
  bool tb = false;
  if (!ta && !tb) {
    return 0;
  }
  if (ta && !tb) {
    return -b;
  }
  if (!ta && tb) {
    return a;
  }
  // Arbitrary, both are semi-transparent blocks
  return a;
}

void chunk_section_buffer_update_mesh(ChunkSection *section, ChunkSection *neighbors[3], WGPUDevice device) {
  section->num_quads = 0;
  int mask[16 * 16];
  for (int d = 0; d < 3; d += 1) {
    int u = (d + 1) % 3;
    int v = (d + 2) % 3;
    int x[3] = {0, 0, 0};

    // Go over all the slices in this dimension
    for (x[d] = 0; x[d] < 16; x[d] += 1) {

      // Make a mask
      for (x[u] = 0; x[u] < 16; x[u] += 1) {
        for (x[v] = 0; x[v] < 16; x[v] += 1) {
          int above = section->data[x[0] + 16 * (x[1] + 16 * x[2])];
          int xb[3] = {x[0], x[1], x[2]};
          xb[d] -= 1;
          int below;
          if (xb[d] < 0) {
            if (neighbors[d] == NULL) {
              below = 0;
            } else {
              xb[d] = 15;
              below = neighbors[d]->data[xb[0] + 16 * (xb[1] + 16 * xb[2])];
            }
          } else {
            below = section->data[xb[0] + 16 * (xb[1] + 16 * xb[2])];
          }
          mask[x[v] + 16 * x[u]] = face_material_between(below, above);
        }
      }

      // Greedily find a quad where the mask is the same value and repeat
      for (int j = 0; j < 16; j += 1) {
        for (int i = 0; i < 16; ) {
          int m = mask[j + 16 * i];
          if (m == 0) {
            i += 1;
            continue;
          }
          int w = 1;
          while (mask[j + 16 * (i + w)] == m && i + w < 16) {
            w += 1;
          }
          int h = 1;
          bool done = false;
          for (; j + h < 16; h += 1) {
            for (int k = 0; k < w; k += 1) {
              if (mask[(j + h) + 16 * (i + k)] != m) {
                done = true;
                break;
              }
            }
            if (done) {
              break;
            }
          }

          // Add quad
          x[u] = i;
          x[v] = j;
          int du[3] = {0};
          du[u] = w;
          int dv[3] = {0};
          dv[v] = h;
          int q = section->num_quads * 4 * FLOATS_PER_VERTEX;
          quads[q + 0] = x[0];
          quads[q + 1] = x[1];
          quads[q + 2] = x[2];
          quads[q + 3] = 1.0f;
          quads[q + 4] = 0.0f;
          quads[q + 5] = 0.0f;
          quads[q + 6] = 1.0f;
          quads[q + 7] = w;
          quads[q + 8] = h;
          quads[q + 9] = m;
          q += FLOATS_PER_VERTEX;
          quads[q + 0] = x[0] + du[0];
          quads[q + 1] = x[1] + du[1];
          quads[q + 2] = x[2] + du[2];
          quads[q + 3] = 0.0f;
          quads[q + 4] = 1.0f;
          quads[q + 5] = 0.0f;
          quads[q + 6] = 1.0f;
          quads[q + 7] = 0.0f;
          quads[q + 8] = h;
          quads[q + 9] = m;
          q += FLOATS_PER_VERTEX;
          quads[q + 0] = x[0] + du[0] + dv[0];
          quads[q + 1] = x[1] + du[1] + dv[1];
          quads[q + 2] = x[2] + du[2] + dv[2];
          quads[q + 3] = 1.0f;
          quads[q + 4] = 1.0f;
          quads[q + 5] = 1.0f;
          quads[q + 6] = 1.0f;
          quads[q + 7] = 0.0f;
          quads[q + 8] = 0.0f;
          quads[q + 9] = m;
          q += FLOATS_PER_VERTEX;
          quads[q + 0] = x[0] + dv[0];
          quads[q + 1] = x[1] + dv[1];
          quads[q + 2] = x[2] + dv[2];
          quads[q + 3] = 0.0f;
          quads[q + 4] = 0.0f;
          quads[q + 5] = 1.0f;
          quads[q + 6] = 1.0f;
          quads[q + 7] = w;
          quads[q + 8] = 0.0f;
          quads[q + 9] = m;
          section->num_quads += 1;

          // Zero out mask
          for (int l = 0; l < h; l += 1) {
            for (int k = 0; k < w; k += 1) {
              mask[(j + l) + 16 * (i + k)] = false;
            }
          }
        }
      }
    }
  }

  if (section->vertex_buffer != NULL) {
    wgpuBufferRelease(section->vertex_buffer);
  }

  section->vertex_buffer = frmwrk_device_create_buffer_init(
    device,
    &(const frmwrk_buffer_init_descriptor){
      .label = "Vertex Buffer",
      .content = (void *)quads,
      .content_size = section->num_quads * 4 * FLOATS_PER_VERTEX * sizeof(float),
      .usage = WGPUBufferUsage_Vertex,
    }
  );
}
