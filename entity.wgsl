struct Uniforms {
  view: mat4x4<f32>,
  projection: mat4x4<f32>,
}

@group(0) @binding(0)
var<uniform> uniforms: Uniforms;

struct Vertex {
  @location(0) position: vec3<f32>,
  @location(1) location: vec3<f32>,
};

struct VertexOutput {
  @builtin(position) position: vec4<f32>,
}

@vertex
fn vs_main(vertex: Vertex) -> VertexOutput {
  var out: VertexOutput;

  out.position = uniforms.projection * uniforms.view * vec4<f32>((vertex.position - vec3(0.5)) * 1.005 + vec3(0.5) + vertex.location, 1.0);
  return out;
}

@fragment
fn fs_main(vertex: VertexOutput) -> @location(0) vec4<f32> {
  return vec4<f32>(1, 0, 0, 1);
}
