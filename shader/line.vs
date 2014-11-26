#version 400

uniform mat4 world;
uniform mat4 worldViewProj;

in vec3 vertex_position;
in vec3 vertex_color;

out vec3 color;

void main () 
{
	gl_Position =  worldViewProj * world * vec4 (vertex_position, 1.0);
	color = vertex_color;
}