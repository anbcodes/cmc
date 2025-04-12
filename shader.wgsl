struct Uniforms {
  view: mat4x4<f32>,
  projection: mat4x4<f32>,
  internal_sky_max: f32,
}

const TEXTURE_TILES = 40.0f;

@group(0) @binding(0)
var<uniform> uniforms: Uniforms;

@group(0) @binding(1)
var image_texture: texture_2d<f32>;

@group(0) @binding(2)
var image_sampler: sampler;

struct Vertex {
  @location(0) position: vec3<f32>,
  @location(1) color: vec4<f32>,
  @location(2) coord: vec2<f32>,
  @location(3) material: f32,
  @location(4) overlay_material: f32,
  @location(5) sky_light: f32,
  @location(6) block_light: f32,
  @location(7) normal: f32,
};

struct VertexOutput {
  @builtin(position) position: vec4<f32>,
  @location(0) color: vec4<f32>,
  @location(1) coord: vec2<f32>,
  @location(2) patchCoord: vec2<f32>,
  @location(3) patchOverlayCoord: vec2<f32>,
  @location(4) sky_light: f32,
  @location(5) block_light: f32,
  @location(6) normal: f32,
}

@vertex
fn vs_main(vertex: Vertex) -> VertexOutput {
  var out: VertexOutput;
  var patchCount = vec2<f32>(TEXTURE_TILES, TEXTURE_TILES);
  var patchIndex = vec2<f32>(vertex.material % patchCount.x, floor(vertex.material / patchCount.x));
  var patchOverlayIndex = vec2<f32>(vertex.overlay_material % patchCount.x, floor(vertex.overlay_material / patchCount.x));
  var patchSize = vec2<f32>(1.0 / patchCount.x, 1.0 / patchCount.y);

  out.position = uniforms.projection * uniforms.view * vec4<f32>(vertex.position, 1.0);
  out.color = vertex.color;
  out.coord = vertex.coord;
  out.patchCoord = patchSize * patchIndex;
  out.patchOverlayCoord = patchSize * patchOverlayIndex;
  out.sky_light = vertex.sky_light;
  out.block_light = vertex.block_light;
  out.normal = vertex.normal;
  return out;
}

fn srgb_to_linear(in: vec4<f32>) -> vec4<f32> {
  return vec4<f32>(pow(in.rgb, vec3<f32>(2.2)), in.a);
}

@fragment
fn fs_main(vertex: VertexOutput) -> @location(0) vec4<f32> {
  var patchCount = vec2<f32>(TEXTURE_TILES, TEXTURE_TILES);
  var patchSize = vec2(1.0 / patchCount.x, 1.0 / patchCount.y);
  var texCoord = vertex.patchCoord + fract(vertex.coord) * patchSize;
  var color = textureSample(image_texture, image_sampler, texCoord);
  var tint = srgb_to_linear(vertex.color);
  if (vertex.patchOverlayCoord.x == 0.0f && vertex.patchOverlayCoord.y == 0.0f) {
    color *= tint;
  }
  var overlayColor = textureSample(image_texture, image_sampler, vertex.patchOverlayCoord + fract(vertex.coord) * patchSize) * tint;
  color = mix(color, overlayColor, overlayColor.a);
  if (color.a == 0.0f) {
    discard;
  }
  // -z, -y, -x, 0, +x, +y, +z
  var normal_multipliers: array<f32, 7> = array<f32, 7>(0.5, 0.3, 0.3, 0.0, 0.3, 0.7, 0.5);

  var internal_sky_light = uniforms.internal_sky_max - (1.0 - vertex.sky_light);
  color = vec4<f32>(color.rgb * normal_multipliers[u32(vertex.normal + 3.0f)] * clamp(pow(clamp(internal_sky_light, 0.0, 1.0), 3.0) + 0.1, 0.0, 1.0), color.a);
  return color;
}

@fragment
fn fs_destroy_main(vertex: VertexOutput) -> @location(0) vec4<f32> {
  var patchCount = vec2<f32>(TEXTURE_TILES, TEXTURE_TILES);
  var patchSize = vec2(1.0 / patchCount.x, 1.0 / patchCount.y);
  var texCoord = vertex.patchCoord + fract(vertex.coord) * patchSize;
  var color = textureSample(image_texture, image_sampler, texCoord);
  var tint = srgb_to_linear(vertex.color);
  if (vertex.patchOverlayCoord.x == 0.0f && vertex.patchOverlayCoord.y == 0.0f) {
    color *= tint;
  }
  var overlayColor = textureSample(image_texture, image_sampler, vertex.patchOverlayCoord + fract(vertex.coord) * patchSize) * tint;
  color = mix(color, overlayColor, overlayColor.a);
  if (color.a == 0.0f) {
    discard;
  }

  var internal_sky_light = uniforms.internal_sky_max - (1.0 - vertex.sky_light);
  color = vec4<f32>(color.rgb * clamp(pow(clamp(internal_sky_light, 0.0, 1.0), 3.0) + 0.1, 0.0, 1.0), color.a);
  return color;
}
