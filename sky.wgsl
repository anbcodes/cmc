struct Uniforms {
  look: vec3<f32>,
  aspect: f32,
  sky_color: vec3<f32>,
  time_of_day: f32,
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

fn srgb_to_linear(in: vec4<f32>) -> vec4<f32> {
  return vec4<f32>(pow(in.rgb, vec3<f32>(2.2)), in.a);
}

@vertex
fn vs_main(vertex: Vertex) -> VertexOutput {
  var out: VertexOutput;
  out.position = vec4<f32>(vertex.position, 1.0, 1.0);
  out.color = srgb_to_linear(vec4<f32>(uniforms.sky_color, 1.0));
  return out;
}

@fragment
fn fs_main(vertex: VertexOutput) -> @location(0) vec4<f32> {
  // var angle = acos(dot(uniforms.look, vec3<f32>(0.0, 0.0, 1.0)));
  // return vertex.color * angle;
  return vertex.color;
}
