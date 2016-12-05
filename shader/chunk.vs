#version 400

uniform mat4 world;
uniform mat4 worldViewProj;

in ivec3 vertex_position;
in int vertex_ao;

out vec3 pos;
out vec4 world_pos;
out float ao;

out vec3 normal;

void main () 
{
	gl_Position = worldViewProj * world * vec4 (vertex_position, 1.0);
	world_pos = world * vec4 (vertex_position, 1.0);
	pos = vertex_position;
   ao = float(vertex_ao)/255;
}