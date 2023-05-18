#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <SOIL/SOIL.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>
#include <glm/gtx/string_cast.hpp>
#include <unordered_map>
#include <algorithm>
#include <set>
#include <iostream>
#include <vector>
#include "scolor.hpp"
#include "tiny_obj_loader.h"
#include "imgui.h"
#include "examples/imgui_impl_glfw.h"
#include "examples/imgui_impl_opengl3.h"

using namespace glm;
using namespace std;

void mouse_callback(GLFWwindow *window, double xposIn, double yposIn);
void reset_grass_colors();

float height;
float width;

glm::vec3 cameraPos = glm::vec3(0.0f, 20.0f, 0.0f);
glm::vec3 cameraFront = glm::vec3(50.0f, 0.0f, 50.0f);
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);

bool firstMouse = true;
float camYaw = 45.0f; // cuts the x and z axis in half
float camPitch = -16.0f; // did some trig things with the dimensions of the field to properly face down toward the opposite corner
float lastX = 0;
float lastY = 0;
float fov = 90.0f;

bool cursor_free = false;
bool draw_menu = true;

// ImGUI slider options
int mower_speed = 1;
int mower_scale = 20;

float wind_speed = 0.02f;

float grass_len_modifier = 0;

float grass_bottom_r = 244.0f / 255;
float grass_bottom_g = 165.0f / 255;
float grass_bottom_b = 96.0f / 255;

float grass_bottom_arr[3] = {grass_bottom_r, grass_bottom_g, grass_bottom_b};

float grass_top_r = 124.0f / 255;
float grass_top_g = 252.0f / 255;
float grass_top_b = 0.0f;

bool rainbow_mode = false;

float grass_top_arr[3] = {grass_top_r, grass_top_g, grass_top_b};

// timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// Will be used for a lot of stuff throughout the demo
// NOTE:  general_buffer is NOT thread safe.  Don't try to load shaders in parallel!
// NOTE on the NOTE:  You probably shouldn't do that anyway!
#define GBLEN (1024 * 32)
char *general_buffer;

GLuint make_shader(const char *filename, GLenum shaderType)
{
	int fd = open(filename, O_RDONLY);
	size_t readlen = read(fd, general_buffer, GBLEN);
	close(fd);
	if (readlen == GBLEN)
	{
		printf(RED("Buffer Length of %d bytes Inadequate for File %s\n").c_str(), GBLEN, filename);
		return 0;
	}
	general_buffer[readlen] = 0;
	printf(DGREEN("Read shader in file %s (%d bytes)\n").c_str(), filename, readlen);
	puts(general_buffer);
	unsigned int s_reference = glCreateShader(shaderType);
	glShaderSource(s_reference, 1, (const char **)&general_buffer, 0);
	glCompileShader(s_reference);
	glGetShaderInfoLog(s_reference, GBLEN, NULL, general_buffer);
	puts(general_buffer);
	GLint compile_ok;
	glGetShaderiv(s_reference, GL_COMPILE_STATUS, &compile_ok);
	if (compile_ok)
	{
		puts(GREEN("Compile Success").c_str());
		return s_reference;
	}
	puts(RED("Compile Failed\n").c_str());
	return 0;
}

GLuint make_program(const char *v_file, const char *tcs_file, const char *tes_file, const char *g_file, const char *f_file)
{
	unsigned int vs_reference = make_shader(v_file, GL_VERTEX_SHADER);
	unsigned int tcs_reference = 0, tes_reference = 0;
	if (tcs_file)
		if (!(tcs_reference = make_shader(tcs_file, GL_TESS_CONTROL_SHADER)))
			return 0;
	if (tes_file)
		if (!(tes_reference = make_shader(tes_file, GL_TESS_EVALUATION_SHADER)))
			return 0;
	unsigned int gs_reference = 0;
	if (g_file)
		gs_reference = make_shader(g_file, GL_GEOMETRY_SHADER);
	unsigned int fs_reference = make_shader(f_file, GL_FRAGMENT_SHADER);
	if (!(vs_reference && fs_reference))
		return 0;
	if (g_file && !gs_reference)
		return 0;

	unsigned int program = glCreateProgram();
	glAttachShader(program, vs_reference);
	if (g_file)
		glAttachShader(program, gs_reference);
	if (tcs_file)
		glAttachShader(program, tcs_reference);
	if (tes_file)
		glAttachShader(program, tes_reference);
	glAttachShader(program, fs_reference);
	glLinkProgram(program);
	GLint link_ok;
	glGetProgramiv(program, GL_LINK_STATUS, &link_ok);
	if (!link_ok)
	{
		glGetProgramInfoLog(program, GBLEN, NULL, general_buffer);
		puts(general_buffer);
		puts(RED("Link Failed").c_str());
		return 0;
	}

	return program;
}

void resize(GLFWwindow *, int new_width, int new_height)
{
	width = new_width;
	height = new_height;
	printf("Window resized, now %f by %f\n", width, height);
	glViewport(0, 0, width, height);
}

struct Vertex {
	vec3 pos;
	vec2 texCoord;

	bool operator==(const Vertex& other) const {
		return pos == other.pos && texCoord == other.texCoord;
	}
};// Can't use  __attribute__((packed)) due to non-POD vec2 and vec3

namespace std {
	template<> struct hash<Vertex> {
		size_t operator()(Vertex const& vertex) const {
			return ((hash<glm::vec3>()(vertex.pos) ^ 
						(hash<glm::vec2>()(vertex.texCoord) << 1)));
		}
	};
}

class LoadedObject {
public:
	GLuint vbuf, ebuf, tbuf;
	size_t element_count;
	void load_object(const char* filename){
	
		tinyobj::attrib_t attrib;
		std::vector<tinyobj::shape_t> shapes;
		std::vector<tinyobj::material_t> materials;
		std::string warn, err;
		vector<Vertex> vertices;
		vector<uint32_t> indices;
		if(!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filename))
			throw std::runtime_error(err);
	
		unordered_map<Vertex, uint32_t> uniqueVertices = {};
	
		for(const auto& shape : shapes){
			for(const auto& index : shape.mesh.indices){
				Vertex vertex = {};
				vertex.pos = {
					attrib.vertices[3 * index.vertex_index + 0],
					attrib.vertices[3 * index.vertex_index + 1],
					attrib.vertices[3 * index.vertex_index + 2]
				};		
				vertex.texCoord = {
					attrib.texcoords[2 * index.texcoord_index + 0],
					1.0 - attrib.texcoords[2 * index.texcoord_index + 1]
				};
				if(uniqueVertices.count(vertex) == 0){
					uniqueVertices[vertex] = (uint32_t)vertices.size();
					vertices.push_back(vertex);
				}
				indices.push_back(uniqueVertices[vertex]);
			}
		}
	
		float *non_padded_vertex = (float*)malloc(3 * sizeof(float) * vertices.size());
		float *non_padded_texture = (float*)malloc(2 * sizeof(float) * vertices.size());
		for(size_t i = 0; i < vertices.size(); i++){
			non_padded_vertex[i*3 + 0] = vertices[i].pos.x;
			non_padded_vertex[i*3 + 1] = vertices[i].pos.y;
			non_padded_vertex[i*3 + 2] = vertices[i].pos.z;
			non_padded_texture[i*2 + 0] = vertices[i].texCoord.x;
			non_padded_texture[i*2 + 1] = vertices[i].texCoord.y;
		}
	
		glGenBuffers(1, &vbuf);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, vbuf);
		glBufferData(GL_SHADER_STORAGE_BUFFER, 3 * sizeof(float) * vertices.size(), non_padded_vertex, GL_STATIC_DRAW); 
		free(non_padded_vertex);
		std::cout << "Loaded " << vertices.size() << " Vertices\n";
	
		glGenBuffers(1, &tbuf);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, tbuf);
		glBufferData(GL_SHADER_STORAGE_BUFFER, 2 * sizeof(float) * vertices.size(), non_padded_texture, GL_STATIC_DRAW); 
		free(non_padded_texture);
	
		glGenBuffers(1, &ebuf);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebuf);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * indices.size(), indices.data(), GL_STATIC_DRAW);
		element_count = indices.size();
		std::cout << "Loaded " << indices.size() << " Indices\n";
	}
};

void cap_release_cursor(GLFWwindow* window){
	cursor_free = !cursor_free;
	if(cursor_free){
		glfwSetCursorPosCallback(window, NULL);
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	} else {
		glfwSetCursorPosCallback(window, mouse_callback);
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	}
}

void handle_keystrokes(GLFWwindow* window, int key, int scancode, int action, int mods){
	if(key == GLFW_KEY_ESCAPE && action == 0)
		cap_release_cursor(window);
	else if(key == GLFW_KEY_F1 && action == 0)
		draw_menu = !draw_menu;
}

int main(int argc, char **argv)
{
	general_buffer = (char *)malloc(GBLEN);
	glfwInit();
	
	// get monitor resolution
	GLFWmonitor *primary = glfwGetPrimaryMonitor();
	const GLFWvidmode *modes = glfwGetVideoMode(primary);
	printf("Height: %d, Width: %d\n", modes->height, modes->width);
	height = modes->height - 200;
	width = modes->width - 200;
	GLFWwindow *window = glfwCreateWindow(width, height, "Project 2", 0, 0);
	glfwSetFramebufferSizeCallback(window, resize);
	glfwMakeContextCurrent(window);
	glewInit();
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	glfwSetCursorPosCallback(window, mouse_callback);
	glfwSetKeyCallback(window, handle_keystrokes); 	


	// Initialization part

	float grass_vertices[] = {
		0.0f, 0.0f, 0.0f,
		0.0f, 0.5f, 0.0f
	};


	float ground_vertices[] = { 	// square covering bottom of grass field
		0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f,
		1.0f, 0.0f, 0.0f,
		1.0f, 0.0f, 0.0f,
		1.0f, 0.0f, 1.0f,
		0.0f, 0.0f, 1.0f
	};


	unsigned int grass_vbuf;
	glGenBuffers(1, &grass_vbuf);
	glBindBuffer(GL_ARRAY_BUFFER, grass_vbuf);
	glBufferData(GL_ARRAY_BUFFER, sizeof(grass_vertices), grass_vertices, GL_STATIC_DRAW);

	unsigned int ground_vbuf;
	glGenBuffers(1, &ground_vbuf);
	glBindBuffer(GL_ARRAY_BUFFER, ground_vbuf);
	glBufferData(GL_ARRAY_BUFFER, sizeof(ground_vertices), ground_vertices, GL_STATIC_DRAW);

	float ground_colors[] = {
		150.0f / 255, 75.0f / 255, 0.0f,
		150.0f / 255, 75.0f / 255, 0.0f,
		150.0f / 255, 75.0f / 255, 0.0f,
		150.0f / 255, 75.0f / 255, 0.0f,
		150.0f / 255, 75.0f / 255, 0.0f,
		150.0f / 255, 75.0f / 255, 0.0f,
	};

	unsigned int ground_cbuf;
	glGenBuffers(1, &ground_cbuf);
	glBindBuffer(GL_ARRAY_BUFFER, ground_cbuf);
	glBufferData(GL_ARRAY_BUFFER, sizeof(ground_colors), ground_colors, GL_STATIC_DRAW);

	unsigned int grass_indices[] = {
		0, 1};

	unsigned int ground_indices[] = {
		0, 1, 2, 3, 4, 5
	};

	unsigned int grass_indicesbuf;
	glGenBuffers(1, &grass_indicesbuf);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, grass_indicesbuf);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(grass_indices), grass_indices, GL_STATIC_DRAW);

	unsigned int ground_indicesbuf;
	glGenBuffers(1, &ground_indicesbuf);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ground_indicesbuf);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(ground_indices), ground_indices, GL_STATIC_DRAW);

	GLuint grass_program = make_program("grass_vs.glsl", 0, 0, 0, "grass_fs.glsl");
	if (!grass_program)
		return 1;

	GLuint ground_program = make_program("ground_vs.glsl", 0, 0, 0, "ground_fs.glsl");
	if (!ground_program)
		return 1;
	
	LoadedObject mower;
	mower.load_object("mower.obj");

	GLuint mower_program = make_program("mower_vs.glsl", 0, 0, 0, "mower_fs.glsl");
	if (!mower_program)
		return 1;

	int grass_count = 600000;
	srandom(time(NULL));

	float *grass_locations = (float*)malloc(grass_count * 2 * sizeof(float)); // 2x the grass_count because we need an x and a z offset
	for (int i = 0; i < grass_count * 2; i++)
	{
		grass_locations[i] = ((float)(random() / (float)(RAND_MAX / 50)));
	}

	float *grass_len_offset = (float*)malloc(grass_count * 2 * sizeof(float)); // 2x the grass_count for restoring the grass length back to what it was after mowing 2nd time
	for (int i = 0; i < grass_count * 2; i+=2)
	{
		grass_len_offset[i] = 0.2f + ((float)(random() / (float)(RAND_MAX / 0.6)));
		grass_len_offset[i+1] = grass_len_offset[i];
		//printf("i=%d, val=%f\n", i, grass_len_offset[i]);
	}

	unsigned int grasslocs_buffer;
	glGenBuffers(1, &grasslocs_buffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, grasslocs_buffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, grass_count * 2 * sizeof(float), grass_locations, GL_STATIC_DRAW);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, grasslocs_buffer);

	unsigned int grasslen_buffer;
	glGenBuffers(1, &grasslen_buffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, grasslen_buffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, grass_count * 2 * sizeof(float), grass_len_offset, GL_STATIC_DRAW);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, grasslen_buffer);

	unsigned int grass_v_attrib = glGetAttribLocation(grass_program, "in_vertex");
	unsigned int grass_top_c_uniform = glGetUniformLocation(grass_program, "top_color");
	unsigned int grass_bottom_c_uniform = glGetUniformLocation(grass_program, "bottom_color");
	unsigned int grass_mvp_uniform = glGetUniformLocation(grass_program, "mvp");
	unsigned int grass_count_uniform = glGetUniformLocation(grass_program, "count");
	unsigned int wind_speed_uniform = glGetUniformLocation(grass_program, "wind_speed");
	unsigned int grass_len_modifier_uniform = glGetUniformLocation(grass_program, "grass_len_modifier");
	unsigned int mower_position_uniform_grass = glGetUniformLocation(grass_program, "mower_pos");
	unsigned int finished_mowing_uniform = glGetUniformLocation(grass_program, "finished_mowing");
	unsigned int mower_scale_uniform_grass = glGetUniformLocation(grass_program, "mower_scale");

	unsigned int ground_v_attrib = glGetAttribLocation(ground_program, "in_vertex");
	unsigned int ground_c_attrib = glGetAttribLocation(ground_program, "in_color");
	unsigned int ground_mvp_uniform = glGetUniformLocation(ground_program, "mvp");

	unsigned int mower_v_attrib = glGetAttribLocation(mower_program, "in_vertex");
	unsigned int mower_t_attrib = glGetAttribLocation(mower_program, "in_texcoord");
	unsigned int mower_model_uniform = glGetUniformLocation(mower_program, "in_model");
	unsigned int mower_view_uniform = glGetUniformLocation(mower_program, "view");
	unsigned int mower_projection_uniform = glGetUniformLocation(mower_program, "projection");
	unsigned int mower_position_uniform = glGetUniformLocation(mower_program, "pos");
	unsigned int mower_scale_uniform = glGetUniformLocation(mower_program, "mower_scale");
	


	int img_width, img_height;
	unsigned char* image = SOIL_load_image("mower.png", &img_width, &img_height, 0, SOIL_LOAD_RGB); 
	if(!image){
		puts(RED("No image, giving up!").c_str());
		puts(SOIL_last_result());
		return 1;
	}
	else
		printf(PURPLE("Image size:  %d by %d\n").c_str(), img_width, img_height);

	GLuint tex;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, img_width, img_height, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
	free(image);

	int size;
	
	int count = 0;
	int mower_count = 0;
	int turn_mower = 0;
	int finished_mowing = 0;
	int outer_corners = 50 * mower_scale - 40;
	int last_scale = mower_scale;
	int turn_offset = 0;

	// rainbow mode variables

	char last_color = 2;
	int color_num = 0;
	bool first_frame = true;
	int rainbow_speed = 3;
	
	glm::mat4 mower_model = glm::rotate(glm::mat4(1.0), (float)glm::radians(-90.0f), glm::vec3(1, 0, 0));
	glm::mat4 mower_model90 = glm::rotate(mower_model, (float)glm::radians(-90.0f), glm::vec3(0, 0, 1));
	glm::mat4 mower_model180 = glm::rotate(mower_model, (float)glm::radians(180.0f), glm::vec3(0, 0, 1));
	glm::mat4 mower_model270 = glm::rotate(mower_model, (float)glm::radians(90.0f), glm::vec3(0, 0, 1));

	glm::vec3 pos;
	glm::mat4 model;

	IMGUI_CHECKVERSION();
	using namespace ImGui;
	CreateContext();
	ImGuiIO& io = GetIO(); (void)io;
	StyleColorsDark();
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init("#version 460");

	glEnable(GL_DEPTH_TEST);
	while (!glfwWindowShouldClose(window))
	{
		if (last_scale != mower_scale) { // reset the position of the mower after a scale change
			//outer_corners = 50 * mower_scale - 20 * (60 / (float)mower_scale);
			outer_corners = 50 * mower_scale;
			mower_count = 0;
			turn_mower = 0;
		}

		last_scale = mower_scale;
		if (mower_count > outer_corners) {
			turn_mower++;
			mower_count = 0;
			if (turn_mower > 2 && (turn_mower % 4 == 1) || (turn_mower % 4 == 3)) // shorten the distance the mower needs to go
				outer_corners -= 2 * 40;

			if (outer_corners < 0) { // when the mower gets to the end of mowing, reset things
				outer_corners = 50 * mower_scale - 40;
				turn_mower = 0;
				finished_mowing ? finished_mowing = 0 : finished_mowing = 1;
			}
		}
		//printf("Mower count: %d\n", mower_count);
		//printf("turn_mower %d, %%4: %d\n", turn_mower, turn_mower % 4);
		//printf("outer_corners: %d\n", outer_corners);
			
		count++;
		mower_count += mower_speed;

		glfwPollEvents();
		glClearColor(0, 0, 0, 1.0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// calculate the turn offset
		turn_offset = 2 * (turn_mower / 4 * (mower_scale * (40 / (float) mower_scale)));
		//printf("turn_offset: %d\n", turn_offset);
		
		// calculate mower position
		
		pos = vec3(0,0,0);
		if (turn_mower == 0) {
			model = mower_model;
			pos += vec3(mower_count, -30, 0);

		} else if (turn_mower % 4 == 1) {
			model = mower_model90;
			pos += vec3(mower_count + turn_offset, 50 * mower_scale - 30 - turn_offset, 0);
			
		} else if (turn_mower % 4 == 2) {
			model = mower_model180;
			pos += vec3(-1*(50* mower_scale) + turn_offset + mower_count, 50* mower_scale - 30 - turn_offset, 0);

		} else if (turn_mower % 4 == 3) {
			model = mower_model270;
			pos += vec3(-1*(50* mower_scale) + turn_offset + mower_count, -30 - turn_offset, 0);

		} else {
			model = mower_model;
			pos += vec3(mower_count + turn_offset - 2 * 40, -30 - turn_offset, 0);
		}
		//std::cout << glm::to_string(model * glm::vec4(pos, 1.0) / (float)mower_scale) << std::endl;

		float currentFrame = glfwGetTime();
		deltaTime = currentFrame - lastFrame;
		lastFrame = currentFrame;
		float cameraSpeed = 10.0f * deltaTime;
		if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
			cameraPos += cameraSpeed * cameraFront;
		if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
			cameraPos -= cameraSpeed * cameraFront;
		if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
			cameraPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
		if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
			cameraPos += glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;

		// printf("Camerapos x: %f, y: %f, z: %f\n", cameraPos.x, cameraPos.z, cameraPos.z);
		glm::vec3 axis_y(0, 1, 0);
		// glm::mat4 model = glm::rotate(glm::mat4(1.0), (float)glfwGetTime(), glm::vec3(0.0f, 0.0f, 1.0f));
		glm::mat4 grass_model = glm::mat4(1.0);
		glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
		glm::mat4 projection = glm::perspective(radians(fov), width / height, 0.1f, 100.0f);
		glm::mat4 mvp = projection * view * grass_model;

		// draw the grass

		if (rainbow_mode) {

			grass_top_arr[(last_color + 1) % 3] = (255 - color_num) / 255.0f;
			grass_top_arr[last_color % 3] = color_num / 255.0f;

			grass_bottom_arr[(last_color + 3) % 3] = (255 - color_num) / 255.0f;
			grass_bottom_arr[(last_color + 2) % 3] = color_num / 255.0f;

			color_num += rainbow_speed;

			if (color_num > 255) {
				color_num = 0;
				// 0 = r, 1 = g, 2 = b
				if (last_color == 0)
					last_color = 1;
				else if (last_color == 1)
					last_color = 2;
				else if (last_color == 2)
					last_color = 0;
			}
			
			for (int dec_color = 0; dec_color < 3; dec_color++) {
				int inc_color = dec_color == 2 ? 0 : dec_color + 1;

			}
		}

		glUseProgram(grass_program);
		glUniform1i(grass_count_uniform, count);
		glUniform1i(mower_scale_uniform_grass, mower_scale);
		glUniform1i(finished_mowing_uniform, finished_mowing);
		glUniform1f(grass_len_modifier_uniform, grass_len_modifier);
		glUniform1f(wind_speed_uniform, wind_speed);
		glUniform3fv(grass_top_c_uniform, 1, grass_top_arr);
		glUniform3fv(grass_bottom_c_uniform, 1, grass_bottom_arr);
		glBindBuffer(GL_ARRAY_BUFFER, grass_vbuf);
		glVertexAttribPointer(grass_v_attrib, 3, GL_FLOAT, GL_FALSE, 0, 0);
		glEnableVertexAttribArray(grass_v_attrib);
		

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, grass_indicesbuf);
		glUniformMatrix4fv(grass_mvp_uniform, 1, 0, glm::value_ptr(mvp));
		glUniform4fv(mower_position_uniform_grass, 1, glm::value_ptr(model * glm::vec4(pos, 1.0) / (float)mower_scale));
		glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &size);
		glDrawElementsInstanced(GL_LINES, size / sizeof(GLuint), GL_UNSIGNED_INT, 0, grass_count);

		// draw the ground

		glUseProgram(ground_program);
		
		glBindBuffer(GL_ARRAY_BUFFER, ground_vbuf);
		glVertexAttribPointer(ground_v_attrib, 3, GL_FLOAT, GL_FALSE, 0, 0);
		glEnableVertexAttribArray(ground_v_attrib);

		glBindBuffer(GL_ARRAY_BUFFER, ground_cbuf);
		glVertexAttribPointer(ground_c_attrib, 3, GL_FLOAT, GL_FALSE, 0, 0);
		glEnableVertexAttribArray(ground_c_attrib);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ground_indicesbuf);
		glUniformMatrix4fv(ground_mvp_uniform, 1, 0, glm::value_ptr(mvp));
		glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &size);
		glDrawElementsInstanced(GL_TRIANGLES, size / sizeof(GLuint), GL_UNSIGNED_INT, 0, 1);
		
		// draw the mower
		
		glm::mat4 mower_view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
		glm::mat4 mower_projection = glm::perspective(radians(fov), width / height, 0.1f, 100.0f);

		glUseProgram(mower_program);
		glUniform1i(mower_scale_uniform, mower_scale);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, tex);
		// vertices
		glEnableVertexAttribArray(mower_v_attrib);
		glBindBuffer(GL_ARRAY_BUFFER, mower.vbuf);
		glVertexAttribPointer(mower_v_attrib, 3, GL_FLOAT, GL_FALSE, 0, 0);
		float cur_vertex[4];
		
		// texture coordinates
		glEnableVertexAttribArray(mower_t_attrib);
		glBindBuffer(GL_ARRAY_BUFFER, mower.tbuf);
		glVertexAttribPointer(mower_t_attrib, 2, GL_FLOAT, GL_FALSE, 0, 0);
		// Edges
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mower.ebuf);

		glUniformMatrix4fv(mower_model_uniform, 1, 0, glm::value_ptr(model));
		glUniformMatrix4fv(mower_view_uniform, 1, 0, glm::value_ptr(mower_view));
		glUniformMatrix4fv(mower_projection_uniform, 1, 0, glm::value_ptr(mower_projection));
		glUniform3fv(mower_position_uniform, 1, glm::value_ptr(pos));
		glDrawElementsInstanced(GL_TRIANGLES, mower.element_count, GL_UNSIGNED_INT, 0, 1);

		if (draw_menu) {
			ImGui_ImplOpenGL3_NewFrame();
			ImGui_ImplGlfw_NewFrame();
			NewFrame();
			Begin("OurWindow");
			Text("FPS:  %.1f FPS", GetIO().Framerate);
			SliderInt("Mower Speed", &mower_speed, 0, 500);
			SliderInt("Mower Scale", &mower_scale, 3, 500);
			SliderFloat("Grass Length Offset", &grass_len_modifier, 0, 50);
			SliderFloat("Wind Speed", &wind_speed, 0.0f, 0.3f);
			SliderFloat3("Top Grass Color", grass_top_arr, 0.0f, 1.0f);
			SliderFloat3("Bottom Grass Color", grass_bottom_arr, 0.0f, 1.0f);
			Checkbox("Rainbow Mode (be careful)", &rainbow_mode);
			SliderInt("Rainbow Speed", &rainbow_speed, 1, 20);
			if (Button("Reset Colors"))
				reset_grass_colors();
			End();
			Render();
			ImGui_ImplOpenGL3_RenderDrawData(GetDrawData());
		}
	
		glfwSwapBuffers(window);
	}

	glDeleteProgram(grass_program);
	glfwDestroyWindow(window);
	glfwTerminate();
	free(general_buffer);
	free(grass_len_offset);
	free(grass_locations);
}
// g++ load_object.cpp -lGL -lglfw -lGLEW

void mouse_callback(GLFWwindow *window, double xposIn, double yposIn)
{
	float xpos = (float)xposIn;
	float ypos = (float)yposIn;

	if (firstMouse) {
		lastX = xpos;
		lastY = ypos;
		firstMouse = false;
	}

	float xoffset = xpos - lastX;
	float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top
	lastX = xpos;
	lastY = ypos;

	float sensitivity = 0.1f; // change this value to your liking
	xoffset *= sensitivity;
	yoffset *= sensitivity;

	camYaw += xoffset;
	camPitch += yoffset;

	// make sure that when pitch is out of bounds, screen doesn't get flipped
	if (camPitch > 89.0f)
		camPitch = 89.0f;
	if (camPitch < -89.0f)
		camPitch = -89.0f;

	glm::vec3 front;
	front.x = cos(glm::radians(camYaw)) * cos(glm::radians(camPitch));
	front.y = sin(glm::radians(camPitch));
	front.z = sin(glm::radians(camYaw)) * cos(glm::radians(camPitch));
	cameraFront = glm::normalize(front);
}

void reset_grass_colors() {
	if (rainbow_mode) rainbow_mode = !rainbow_mode;
	grass_bottom_arr[0] = 244.0f / 255;
	grass_bottom_arr[1] = 165.0f / 255;
	grass_bottom_arr[2] = 96.0f / 255;
	grass_top_arr[0] = 124.0f / 255;
	grass_top_arr[1] = 252.0f / 255;
	grass_top_arr[2] = 0;
}
