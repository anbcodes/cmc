struct Uniforms {
  view: mat4x4<f32>,
  projection: mat4x4<f32>,
  position: vec3<f32>,
}

@group(0) @binding(0)
var<uniform> uniforms: Uniforms;

struct Vertex {
  @location(0) position: vec3<f32>,
};

struct VertexOutput {
  @builtin(position) position: vec4<f32>,
}

@vertex
fn vs_main(vertex: Vertex) -> VertexOutput {
  var out: VertexOutput;

  out.position = uniforms.projection * uniforms.view * vec4<f32>((vertex.position - vec3(0.5)) * 1.005 + vec3(0.5) + uniforms.position, 1.0);
  return out;
}

fn srgb_to_linear(in: vec4<f32>) -> vec4<f32> {
  return vec4<f32>(pow(in.rgb, vec3<f32>(2.2)), in.a);
}

@fragment
fn fs_main(vertex: VertexOutput) -> @location(0) vec4<f32> {
  return vec4<f32>(0, 0, 0, 1);
}
