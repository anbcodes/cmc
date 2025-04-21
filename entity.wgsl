struct Uniforms {
  view: mat4x4<f32>,
  projection: mat4x4<f32>,
  time: f32,
}

@group(0) @binding(0)
var<uniform> uniforms: Uniforms;

struct Vertex {
  @location(0) position: vec3<f32>,
  @location(1) last_entity_pos: vec3<f32>,
  @location(2) entity_pos: vec3<f32>,
  @location(3) last_time: f32,
  @location(4) delta_time: f32,
};

struct VertexOutput {
  @builtin(position) position: vec4<f32>,
}

@vertex
fn vs_main(vertex: Vertex) -> VertexOutput {
  var out: VertexOutput;
  var interp_pos: vec3<f32> = mix(vertex.last_entity_pos, vertex.entity_pos, (uniforms.time - vertex.last_time) / vertex.delta_time);

  out.position = uniforms.projection * uniforms.view * vec4<f32>((vertex.position - vec3(0.5)) * 1.005 + vec3(0.5) + interp_pos, 1.0);
  return out;
}

@fragment
fn fs_main(vertex: VertexOutput) -> @location(0) vec4<f32> {
  return vec4<f32>(1, 0, 0, 1);
}
