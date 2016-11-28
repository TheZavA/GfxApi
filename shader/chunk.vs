#version 400

uniform mat4 world;
uniform mat4 worldViewProj;

in ivec3 vertex_position;
in ivec3 vertex_normal;
in int vertex_ao;

out vec3 normal;
out vec3 pos;
out vec4 world_pos;
out float material;
out float ao;

void main () 
{
	gl_Position = worldViewProj * world * vec4 (vertex_position, 1.0);
	world_pos = world * vec4 (vertex_position, 1.0);
	normal = vertex_normal;
	pos = vertex_position;
	material = 0;
   ao = float(vertex_ao)/255;
}