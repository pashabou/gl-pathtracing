#pragma once

#define GLEW_STATIC
#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "glu32.lib")
#pragma comment(lib, "glfw3.lib")
#pragma comment(lib, "glew32s.lib")
#pragma comment(lib, "glew32.lib")


#include <GL/glew.h> 
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <glm/gtx/rotate_vector.hpp>

#include <stdio.h>
#include <chrono>
#include <thread>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <sstream>
#include <locale>
#include <vector>

GLuint tex;
GLuint vao;
GLuint rayCount;

int windowWidth = 400;
int windowHeight = 300;
int workGroupSizeX;
int workGroupSizeY;

bool spinning = false;
int globalFrameCount = 0;
float globalTime = 0.0;
glm::vec3 lastMousePosition;

double fov = glm::pi<double>() * glm::third<double>();
glm::vec3 eyePos = { 4.0, 5.0, 5.0 };
glm::vec3 lookAt = { 0.0, 0.5, 0.0 };
glm::vec3 upVec = { 0.0, 1.0, 0.0 };
glm::mat4 projectionMatrix;
glm::mat4 viewMatrix;
glm::mat4 inverseProjectionViewMatrix;

std::thread shaderFileListenerThread;

unsigned int rasterShader;
unsigned int computeShader;

std::vector<const char*> shaderFiles = {
	"ComputeShader.glsl", "Vertex.glsl", "Fragment.glsl"
};

char errNames[8][36] = {
	"Unknown OpenGL error",
	"GL_INVALID_ENUM", "GL_INVALID_VALUE", "GL_INVALID_OPERATION",
	"GL_INVALID_FRAMEBUFFER_OPERATION", "GL_OUT_OF_MEMORY",
	"GL_STACK_UNDERFLOW", "GL_STACK_OVERFLOW"
};

glm::vec3 getEyeRay(float x, float y, glm::vec3 eyePos) {
	glm::vec4 temp = inverseProjectionViewMatrix * glm::vec4(x, y, 0, 1);
	glm::vec3 point = glm::vec3(temp.x, temp.y, temp.z) / temp.w;
	return point - eyePos;
}

int nextPowerOfTwo(int x) {
	x--;
	x |= x >> 1;
	x |= x >> 2;
	x |= x >> 4;
	x |= x >> 8;
	x |= x >> 16;
	x++;
	return x;
}

struct ListenerFlags {
	bool load = false;
	bool exit = false;
} listenerFlags;

struct Uniforms {
	unsigned int eye, ray00, ray01, ray10, ray11;
	unsigned int transposeInverseViewMatrix;
	unsigned int frameCount, time;

	void setRays(glm::vec3 eyePos) {
		glm::vec3 temp = eyePos;
		glUniform3f(eye, temp.x, temp.y, temp.z);
		temp = getEyeRay(-1, -1, eyePos);
		glUniform3f(ray00, temp.x, temp.y, temp.z);
		temp = getEyeRay(-1, 1, eyePos);
		glUniform3f(ray01, temp.x, temp.y, temp.z);
		temp = getEyeRay(1, -1, eyePos);
		glUniform3f(ray10, temp.x, temp.y, temp.z);
		temp = getEyeRay(1, 1, eyePos);
		glUniform3f(ray11, temp.x, temp.y, temp.z);
	}

	void setViewMatrix(glm::mat4 mat) {
		glm::mat3x3 viewMatrix = glm::inverse(glm::transpose(mat));
		glUniformMatrix3fv(transposeInverseViewMatrix, 1, GL_FALSE, glm::value_ptr(viewMatrix));
	}

	void setFrameCount(int frames) {
		glUniform1i(frameCount, frames);
	}

	void setTime(float t) {
		glUniform1f(time, t);
	}
} uniforms;

struct Shader {
private:
	std::string readShader(const GLchar* path) {
		std::ifstream file;
		std::stringstream stream;
		file.exceptions(std::ifstream::failbit | std::ifstream::badbit);
		file.open(path);
		stream << file.rdbuf();
		file.close();
		return stream.str();
	}

	unsigned int compileShader(GLenum type, std::string string) {
		int success;
		GLchar infoLog[512];

		unsigned int shader = glCreateShader(type);
		const char* cstr = string.c_str();
		glShaderSource(shader, 1, &cstr, NULL);
		glCompileShader(shader);
		glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

		if (!success) {
			glGetShaderInfoLog(shader, 512, NULL, infoLog);
			std::cout << infoLog << std::endl;
		}

		return shader;
	};

public:
	unsigned int programId;

	Shader(const GLchar* vertexPath, const GLchar* fragmentPath) {
		std::string vertexString, fragmentString;

		try {
			vertexString = readShader(vertexPath);
			fragmentString = readShader(fragmentPath);
		}
		catch (std::ifstream::failure e) {
			std::cout << e.what() << std::endl;
			exit(1);
		}

		unsigned int vertex = compileShader(GL_VERTEX_SHADER, vertexString);
		unsigned int fragment = compileShader(GL_FRAGMENT_SHADER, fragmentString);

		int success;
		GLchar infoLog[512];

		programId = glCreateProgram();
		glAttachShader(programId, vertex);
		glAttachShader(programId, fragment);
		glLinkProgram(programId);
		glGetProgramiv(programId, GL_LINK_STATUS, &success);

		if (!success) {
			glGetProgramInfoLog(programId, 512, NULL, infoLog);
			std::cout << infoLog << std::endl;
		}

		glDeleteShader(vertex);
		glDeleteShader(fragment);
	}

	Shader(const GLchar* computePath) {
		std::string computeString;

		try {
			computeString = readShader(computePath);
		}
		catch (std::ifstream::failure e) {
			std::cout << e.what() << std::endl;
			exit(1);
		}

		unsigned int compute = compileShader(GL_COMPUTE_SHADER, computeString);

		int success;
		GLchar infoLog[512];

		programId = glCreateProgram();
		glAttachShader(programId, compute);
		glLinkProgram(programId);
		glGetProgramiv(programId, GL_LINK_STATUS, &success);

		if (!success) {
			glGetProgramInfoLog(programId, 512, NULL, infoLog);
			std::cout << infoLog << std::endl;
		}

		glDeleteShader(compute);
	}
};
