// glw_imgui
// Copyright (C) 2016 Iakov Sumygin - BSD license

#include "platform_sdl.h"

#include <GL/glew.h>
#include <SDL_opengl.h>
#include <SDL.h>
#include <fstream>

#include <stdio.h>
#include <cstddef>
#ifdef WIN32
#include <windows.h>
#endif

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

extern SDL_Window* gWindow;
extern SDL_Renderer* gRenderer;

namespace imgui {
namespace {
const SDL_SystemCursor mapCursor[CURSOR_COUNT] = {SDL_SYSTEM_CURSOR_ARROW, SDL_SYSTEM_CURSOR_SIZEWE,
												  SDL_SYSTEM_CURSOR_SIZENS,
												  SDL_SYSTEM_CURSOR_SIZENWSE};
}
PlatformSDL::PlatformSDL() {
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		printf("SDL could not initialize! SDL Error: %s\n", SDL_GetError());
		throw;
	}
	for (int i = 0; i < CURSOR_COUNT; ++i) _cursors[i] = SDL_CreateSystemCursor(mapCursor[i]);
}
PlatformSDL::~PlatformSDL() {
	for (int i = 0; i < CURSOR_COUNT; ++i) SDL_FreeCursor(_cursors[i]);
	SDL_Quit();
}
void PlatformSDL::set_cursor(CURSOR cursor) {
	SDL_SetCursor(_cursors[cursor]);
}
void PlatformSDL::capture_mouse(bool set) {
	SDL_CaptureMouse(set ? SDL_TRUE : SDL_FALSE);
}

void* PlatformSDL::load_file(const char* path, size_t& buf_size) {
	using namespace std;

	streampos size;
	ifstream file(path, ios::in | ios::binary | ios::ate);
	void* memblock = nullptr;
	if (file.is_open()) {
		size = file.tellg();
		buf_size = (size_t)size;
		memblock = malloc(buf_size);
		file.seekg(0, ios::beg);
		file.read((char*)memblock, buf_size);
	}
	return memblock;
}
// Graphics program
GLuint gProgramID = 0;
GLint gVertexPos3DLocation = -1;
GLint gVertexClrLocation = -1;
GLint gVertexTxtLocation = -1;
GLint gScreenSizeLocation = -1;
GLuint vao, vbo;

void printShaderLog(GLuint shader);
void printProgramLog(GLuint program);

void checkError() {
	GLenum err = GL_NO_ERROR;
	while ((err = glGetError()) != GL_NO_ERROR) {
		printf("%s", glewGetErrorString(err));
#ifdef WIN32
		OutputDebugString((LPCSTR)glewGetErrorString(err));
#endif
	}
}

unsigned int compile_shader(const GLchar** vertex_source, const GLchar** fragment_source);

bool RenderSDL::create() {
	bool success = true;

	const GLchar* vertexShaderSource[] = {
		"#version 140\n"
		"uniform vec2 screen_size;\n"
		"in vec3 in_vertex;\n"
		"in vec2 in_texcoord;\n"
		"in vec4 in_color;\n"
		"out vec4 var_color;\n"
		"out vec2 Texcoord;\n"
		"void main() { \n"
		"Texcoord = in_texcoord;\n"
		"gl_Position = vec4(2*in_vertex.x/screen_size.x-1.0, "
		"2*in_vertex.y/screen_size.y-1.0, in_vertex.z, 1 );var_color = "
		"in_color;}"};

	const GLchar* fragmentShaderSource[] = {"#version 140\n"
											"precision highp float;\n"
											"in vec2 Texcoord;\n"
											"in  vec4 var_color;\n"
											"out vec4 FragColor;\n"
											"uniform sampler2D tex;\n"
											"void main() { FragColor = texture(tex, "
											"Texcoord)*var_color; "
											"}"};

	gProgramID = compile_shader(vertexShaderSource, fragmentShaderSource);

	/* Allocate and assign a Vertex Array Object to our handle */
	glGenVertexArrays(1, &vao);

	/* Bind our Vertex Array Object as the current used object */
	glBindVertexArray(vao);

	/* Allocate and assign two Vertex Buffer Objects to our handle */
	glGenBuffers(1, &vbo);

	/* Bind our first VBO as being the active buffer and storing vertex attributes (coordinates) */
	glBindBuffer(GL_ARRAY_BUFFER, vbo);

	// Get vertex attribute location
	gVertexPos3DLocation = 0;
	gVertexClrLocation = 1;
	gVertexTxtLocation = 2;

	gScreenSizeLocation = glGetUniformLocation(gProgramID, "screen_size");

	checkError();
	return success;
}

unsigned int compile_shader(const GLchar** vertex_source, const GLchar** fragment_source) {
	// Generate program
	GLuint programID = glCreateProgram();

	// Create vertex shader
	GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);

	// Set vertex source
	glShaderSource(vertexShader, 1, vertex_source, NULL);

	// Compile vertex source
	glCompileShader(vertexShader);

	// Check vertex shader for errors
	GLint vShaderCompiled = GL_FALSE;
	glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &vShaderCompiled);
	if (vShaderCompiled != GL_TRUE) {
		printf("Unable to compile vertex shader %d!\n", vertexShader);
		printShaderLog(vertexShader);
		return 0;
	}

	// Attach vertex shader to program
	glAttachShader(programID, vertexShader);

	// Create fragment shader
	GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);

	// Set fragment source
	glShaderSource(fragmentShader, 1, fragment_source, NULL);

	// Compile fragment source
	glCompileShader(fragmentShader);

	// Check fragment shader for errors
	GLint fShaderCompiled = GL_FALSE;
	glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &fShaderCompiled);
	if (fShaderCompiled != GL_TRUE) {
		printf("Unable to compile fragment shader %d!\n", fragmentShader);
		printShaderLog(fragmentShader);
		return 0;
	}

	// Attach fragment shader to program
	glAttachShader(programID, fragmentShader);

	/* Bind attribute index 0 (coordinates) to in_Position and attribute index 1 (color) to in_Color
	*/
	/* Attribute locations must be setup before calling glLinkProgram. */
	glBindAttribLocation(programID, 0, "in_vertex");
	glBindAttribLocation(programID, 1, "in_color");
	glBindAttribLocation(programID, 2, "in_texcoord");

	// Link program
	glLinkProgram(programID);

	// Check for errors
	GLint programSuccess = GL_TRUE;
	glGetProgramiv(programID, GL_LINK_STATUS, &programSuccess);
	if (programSuccess != GL_TRUE) {
		printf("Error linking program %d!\n", programID);
		printProgramLog(programID);
		return 0;
	}
	return programID;
}

void getDisplayScaleFactor(float& x, float& y) {
	int w, h, low_dpi_w, low_dpi_h;
	SDL_GL_GetDrawableSize(gWindow, &w, &h);
	SDL_GetWindowSize(gWindow, &low_dpi_w, &low_dpi_h);
	x = (float)w / low_dpi_w;
	y = (float)h / low_dpi_h;
}

bool RenderSDL::begin(uint width, uint height) {
	int wnd_width, wnd_height;
	SDL_GL_GetDrawableSize(gWindow, &wnd_width, &wnd_height);
	glViewport(0, 0, wnd_width, wnd_height);

	initialize_render(width, height);
	return true;
}

void RenderSDL::initialize_render(uint width, uint height) {
	glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glDisable(GL_CULL_FACE);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	checkError();

	// Bind program
	glUseProgram(gProgramID);
	glEnableVertexAttribArray(gVertexPos3DLocation);
	glEnableVertexAttribArray(gVertexClrLocation);
	glEnableVertexAttribArray(gVertexTxtLocation);

	glUniform2f(gScreenSizeLocation, (float)width, float(height));
	checkError();
}
bool RenderSDL::render_mesh(const render_vertex_3d_t* tris, int count, bool b) {
	_mesh.insert(_mesh.end(), tris, tris + count);
	render((int)_mesh.size() - count, count);
	return true;
}
void RenderSDL::render(int start, int count) {
	// The following commands will talk about our 'vertexbuffer' buffer
	glBindBuffer(GL_ARRAY_BUFFER, vbo);

	// Give our vertices to OpenGL.
	glBufferData(GL_ARRAY_BUFFER, sizeof(render_vertex_3d_t) * _mesh.size(), &_mesh[0],
				 GL_DYNAMIC_DRAW);
	checkError();

	// bind vertex attributes
	glVertexAttribPointer(gVertexPos3DLocation, 3, GL_FLOAT, GL_FALSE, sizeof(render_vertex_3d_t),
						  0);
	glVertexAttribPointer(gVertexClrLocation, 4, GL_UNSIGNED_BYTE, GL_TRUE,
						  sizeof(render_vertex_3d_t), (void*)offsetof(render_vertex_3d_t, clr));
	glVertexAttribPointer(gVertexTxtLocation, 2, GL_FLOAT, GL_FALSE, sizeof(render_vertex_3d_t),
						  (void*)offsetof(render_vertex_3d_t, u));
	checkError();

	glDrawArrays(GL_TRIANGLES, start, count);
}
bool RenderSDL::end() {
	if (_mesh.empty())
		return true; // nothing to draw

	_mesh.clear();
	// GLenum e = glGetError();
	// printf("%s", glewGetErrorString(e));

	checkError();

	glDisable(GL_SCISSOR_TEST);

	// Disable vertex position
	glDisableVertexAttribArray(gVertexPos3DLocation);
	glDisableVertexAttribArray(gVertexClrLocation);
	glDisableVertexAttribArray(gVertexTxtLocation);
	checkError();

	// Unbind program
	glUseProgram(NULL);

	// /on_render_finished();
	return true;
}

void RenderSDL::set_blend_mode(BlendMode mode) {
	switch (mode) {
	case BLEND_NONE:
		glDisable(GL_BLEND);
		break;
	case BLEND_TEXT:
		glBlendFunc(GL_ONE, GL_ONE);
		glEnable(GL_BLEND);
		break;
	case BLEND_RECT:
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_BLEND);
		break;
	}
}
unsigned char* RenderSDL::load_image(const char* filename, int* width, int* height, int* channels) {
	return stbi_load(filename, width, height, channels, 0);
}

unsigned int RenderSDL::create_texture(unsigned int width, unsigned int height,
									   unsigned int channels, void* bmp) {
	// can free ttf_buffer at this point
	checkError();

	GLuint ftex;
	glGenTextures(1, &ftex);
	checkError();

	glBindTexture(GL_TEXTURE_2D, ftex);
	checkError();

	// convert luminance image manually
	if (channels == 1) {
		unsigned char* new_bmp = (unsigned char*)malloc(width * height * 3);
		unsigned char* old_bmp = (unsigned char*)bmp;
		int j = 0;
		for (uint i = 0; i < height * width; ++i) {
			new_bmp[j++] = old_bmp[i];
			new_bmp[j++] = old_bmp[i];
			new_bmp[j++] = old_bmp[i];
		}
		bmp = new_bmp;
	}

	GLenum bmpFormat;
	switch (channels) {
	case 1:
		bmpFormat = GL_RGB;
		break;
	case 3:
		bmpFormat = GL_RGB;
		break;
	case 4:
		bmpFormat = GL_RGBA;
		break;
	}
	if (channels == 1)
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, bmpFormat, GL_UNSIGNED_BYTE, bmp);
	else
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, bmpFormat, GL_UNSIGNED_BYTE, bmp);
	checkError();

	if (channels == 1)
		free(bmp);

	// can free temp_bitmap at this point
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	checkError();
	return ftex;
}
bool RenderSDL::copy_sub_texture(unsigned int target, unsigned int x, unsigned int y,
								 unsigned int width, unsigned int height, void* bmp) {
	glBindTexture(GL_TEXTURE_2D, target);
	glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, width, height, GL_RGBA, GL_UNSIGNED_BYTE, bmp);
	checkError();
	return true;
}

bool RenderSDL::remove_texture(unsigned int texture) {
	glDeleteTextures(1, &texture);
	return true;
}
bool RenderSDL::bind_texture(unsigned int texture) {
	glBindTexture(GL_TEXTURE_2D, texture);
	// glBlendFunc(GL_ONE, GL_ONE);
	checkError();
	return true;
}
void RenderSDL::set_scissor(int x, int y, int w, int h, bool set) {
	float scale_x, scale_y;
	getDisplayScaleFactor(scale_x, scale_y);

	if (set)
		glEnable(GL_SCISSOR_TEST);
	else
		glDisable(GL_SCISSOR_TEST);
	glScissor((int)(scale_x*x), (int)(scale_y*y), (int)(scale_x*w), (int)(scale_y*h));
}
void printProgramLog(GLuint program) {
	// Make sure name is shader
	if (glIsProgram(program)) {
		// Program log length
		int infoLogLength = 0;
		int maxLength = infoLogLength;

		// Get info string length
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);

		// Allocate string
		char* infoLog = new char[maxLength];

		// Get info log
		glGetProgramInfoLog(program, maxLength, &infoLogLength, infoLog);
		if (infoLogLength > 0) {
			// Print Log
			printf("%s\n", infoLog);
		}

		// Deallocate string
		delete[] infoLog;
	}
	else {
		printf("Name %d is not a program\n", program);
	}
}

void printShaderLog(GLuint shader) {
	// Make sure name is shader
	if (glIsShader(shader)) {
		// Shader log length
		int infoLogLength = 0;
		int maxLength = infoLogLength;

		// Get info string length
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);

		// Allocate string
		char* infoLog = new char[maxLength];

		// Get info log
		glGetShaderInfoLog(shader, maxLength, &infoLogLength, infoLog);
		if (infoLogLength > 0) {
			// Print Log
			printf("%s\n", infoLog);
		}

		// Deallocate string
		delete[] infoLog;
	}
	else {
		printf("Name %d is not a shader\n", shader);
	}
}
}
