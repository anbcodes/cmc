struct Uniforms {
  view: mat4x4<f32>,
  projection: mat4x4<f32>,
}

@group(0) @binding(0)
var<uniform> uniforms: Uniforms;

struct Vertex {
  @location(0) position: vec3<f32>,
  @location(1) color: vec4<f32>,
  @location(2) coord: vec2<f32>,
  @location(3) material: f32,
};

struct VertexOutput {
  @builtin(position) position: vec4<f32>,
  @location(0) color: vec4<f32>,
}

@vertex
fn vs_main(vertex: Vertex) -> VertexOutput {
  var out: VertexOutput;
  out.position = uniforms.projection * uniforms.view * vec4<f32>(vertex.position, 1.0);
  out.color = vertex.color;
  return out;
}

@fragment
fn fs_main(vertex: VertexOutput) -> @location(0) vec4<f32> {
  return vertex.color;
}
