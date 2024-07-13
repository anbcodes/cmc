struct Uniforms {
  time: f32,
}

@group(0) @binding(0)
var<uniform> uniforms: Uniforms;

struct Vertex {
  @location(0) position: vec2<f32>,
};

struct VertexOutput {
  @builtin(position) position: vec4<f32>,
  @location(0) color: vec4<f32>,
}

@vertex
fn vs_main(vertex: Vertex) -> VertexOutput {
  var out: VertexOutput;
  out.position = vec4<f32>(vertex.position, 0.0, 1.0);
  out.color = vec4<f32>(vec3<f32>(uniforms.time), 1.0);
  return out;
}

@fragment
fn fs_main(vertex: VertexOutput) -> @location(0) vec4<f32> {
  return vertex.color;
}
