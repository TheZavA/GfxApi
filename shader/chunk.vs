#version 400

uniform mat4 world;
uniform mat4 worldViewProj;

in vec3 vertex_position;
in vec3 vertex_normal;
in ivec2 vertex_material;


out vec3 normal;
out vec3 pos;
out vec4 world_pos;
out float material;


void main () 
{
	gl_Position = worldViewProj * world * vec4 (vertex_position, 1.0);
	world_pos = world * vec4 (vertex_position, 1.0);
	normal = vertex_normal;
	pos = vertex_position;
	material = vertex_material.x;
}