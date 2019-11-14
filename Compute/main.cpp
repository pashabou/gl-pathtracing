

#include "main.h"

bool checkForOpenGLErrors() {
	int numErrors = 0;
	GLenum err;
	while ((err = glGetError()) != GL_NO_ERROR) {
		numErrors++;
		int errNum = 0;
		switch (err) {
		case GL_INVALID_ENUM:
			errNum = 1;
			break;
		case GL_INVALID_VALUE:
			errNum = 2;
			break;
		case GL_INVALID_OPERATION:
			errNum = 3;
			break;
		case GL_INVALID_FRAMEBUFFER_OPERATION:
			errNum = 4;
			break;
		case GL_OUT_OF_MEMORY:
			errNum = 5;
			break;
		case GL_STACK_UNDERFLOW:
			errNum = 6;
			break;
		case GL_STACK_OVERFLOW:
			errNum = 7;
			break;
		}
		printf("OpenGL ERROR: %s.\n", errNames[errNum]);
	}
	return (numErrors != 0);
}

void resizeComputeTexture() {
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, windowWidth, windowHeight, 0, GL_RGBA, GL_FLOAT, NULL);
	glBindTexture(GL_TEXTURE_2D, 0);
}

void createMatrices() {
	projectionMatrix = glm::perspective(fov, (double)windowWidth / windowHeight, 1.0, 2.0);
	viewMatrix = glm::lookAt(eyePos, lookAt, upVec);
	inverseProjectionViewMatrix = glm::inverse(projectionMatrix * viewMatrix);
}

void createBufferObjects() {
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	glGenBuffers(1, &rayCount);
	glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, rayCount);
	glBufferData(GL_ATOMIC_COUNTER_BUFFER, sizeof(int), NULL, GL_DYNAMIC_DRAW);
	glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 1, rayCount);
}

void loadShaders() {
	using namespace std::chrono;
	high_resolution_clock::time_point t1 = high_resolution_clock::now();

	rasterShader = Shader("Vertex.glsl", "Fragment.glsl").programId;
	computeShader = Shader("ComputeShader.glsl").programId;

	glUseProgram(computeShader);
	int* workGroupSize = new int[3];
	glGetProgramiv(computeShader, GL_COMPUTE_WORK_GROUP_SIZE, workGroupSize);
	workGroupSizeX = workGroupSize[0];
	workGroupSizeY = workGroupSize[1];

	uniforms.eye = glGetUniformLocation(computeShader, "eye");
	uniforms.ray00 = glGetUniformLocation(computeShader, "ray00");
	uniforms.ray01 = glGetUniformLocation(computeShader, "ray01");
	uniforms.ray10 = glGetUniformLocation(computeShader, "ray10");
	uniforms.ray11 = glGetUniformLocation(computeShader, "ray11");
	uniforms.time = glGetUniformLocation(computeShader, "globalTime");
	uniforms.frameCount = glGetUniformLocation(computeShader, "frameCount");
	uniforms.transposeInverseViewMatrix = glGetUniformLocation(computeShader, "transposeInverseViewMatrix");

	listenerFlags.load = false;
	globalFrameCount = 0;

	high_resolution_clock::time_point t2 = high_resolution_clock::now();
	auto duration = duration_cast<milliseconds>(t2 - t1).count();

	std::cout << std::endl << std::endl;
	std::cout << "Shader Loaded in " << duration << " milliseconds" << std::endl;
}

void shaderFileListener() {
	using std::chrono::milliseconds;
	using std::chrono::system_clock;
	using std::experimental::filesystem::last_write_time;
	using std::this_thread::sleep_for;

	system_clock::time_point now = system_clock::now();
	while (!listenerFlags.exit) {
		for (const char* str: shaderFiles) {
			if (last_write_time(str) > now) {
				listenerFlags.load = true;

				printf("Loading Shader");
				while (listenerFlags.load != false) {
					printf(".");
					sleep_for(milliseconds(700));
				}

				break;
			}
		}
		
		now = system_clock::now();
		sleep_for(milliseconds(100));
	}
}

void initShaders() {
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	
	shaderFileListenerThread = std::thread(shaderFileListener);
	loadShaders();
	resizeComputeTexture();

    glfwSetTime(0.0);
	checkForOpenGLErrors();
}

void renderToTexture() {
	GLuint null = 0;
	int worksizeX = nextPowerOfTwo(windowWidth);
	int worksizeY = nextPowerOfTwo(windowHeight);

	glUseProgram(computeShader);
	uniforms.setFrameCount(globalFrameCount);
	uniforms.setTime(globalTime);
	uniforms.setRays(eyePos);
	uniforms.setViewMatrix(viewMatrix);

	glBindImageTexture(0, tex, 0, false, 0, GL_WRITE_ONLY, GL_RGBA32F);
	glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, rayCount);
	glBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(int), &null);
	glDispatchCompute(worksizeX / workGroupSizeX, worksizeY / workGroupSizeY, 1);
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
	checkForOpenGLErrors();
}

void renderToScreen() {
	glUseProgram(rasterShader);
	glBindTexture(GL_TEXTURE_2D, tex);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	checkForOpenGLErrors();
}

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action == GLFW_RELEASE) {
        return;
    }
	switch (key) {
	case GLFW_KEY_ESCAPE:
		glfwSetWindowShouldClose(window, true);
		return;
	case GLFW_KEY_LEFT:
		eyePos = glm::rotateY(eyePos, -0.003f);
		break;
	case GLFW_KEY_RIGHT:
		eyePos = glm::rotateY(eyePos, 0.003f);
		break;
	case GLFW_KEY_UP:
		eyePos = glm::rotateX(eyePos, -0.003f);
		break;
	case GLFW_KEY_DOWN:
		eyePos = glm::rotateX(eyePos, 0.003f);
		break;
	case GLFW_KEY_R:
		spinning = !spinning;
		return;
	default:
		return;
	}

	spinning = false;
	globalFrameCount = 0;
}

void cursorCallback(GLFWwindow* window, double x, double y) {
	int left  = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT);
	int right = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT);

	if (right != lastMousePosition.z) {
		lookAt = glm::vec3(0.0, 0.5, 0.0);
		globalFrameCount = 0;
	}

	if (left != GLFW_PRESS && right != GLFW_PRESS ||
		left == GLFW_PRESS && right == GLFW_PRESS) {
		lastMousePosition.x = (float)x;
		lastMousePosition.y = (float)y;
		lastMousePosition.z = (right == GLFW_PRESS);

		return;
	}

	spinning = false;

	int dx = (int)(lastMousePosition.x - x);
	int dy = (int)(lastMousePosition.y - y);

	if (right == GLFW_PRESS) {
		glm::vec3 newLookAt = lookAt - glm::vec3(eyePos.x, 0, eyePos.z);
		newLookAt = glm::rotateY(newLookAt, 0.003f * dx);
		newLookAt += glm::vec3(eyePos.x, -eyePos.y, 0);
		newLookAt = glm::rotateX(newLookAt, 0.003f * dy);
		newLookAt += glm::vec3(0, eyePos.y, eyePos.z);
		lookAt = newLookAt;
	}
	else {
		eyePos = glm::rotateY(glm::rotateX(eyePos, 0.003f * dy), 0.003f * dx);
	}

	lastMousePosition.x = (float)x;
	lastMousePosition.y = (float)y;
	lastMousePosition.z = (right == GLFW_PRESS);
	globalFrameCount = 0;
}

void scrollCallback(GLFWwindow* window, double x, double y) {
	int ctrl = glfwGetKey(window, GLFW_KEY_LEFT_CONTROL);

	if (ctrl == GLFW_PRESS) {
		fov += y * 0.01;
	}
	else {
		eyePos += eyePos * (float)y * 0.01f;
	}
	globalFrameCount = 0;
}

void windowSizeCallback(GLFWwindow* window, int width, int height) {
	glViewport(0, 0, width, height);

	if (windowWidth != width || windowHeight != height) {
		windowWidth = width;
		windowHeight = height;
		resizeComputeTexture();
		globalFrameCount = 0;
	}

    checkForOpenGLErrors();
}

void errorCallback(int error, const char* description) {
	fputs(description, stderr);
}

int main() {
	glEnable(GL_DEBUG_OUTPUT);
	glfwSetErrorCallback(errorCallback);
	glfwInit();

	GLFWwindow* window = glfwCreateWindow(windowWidth, windowHeight, "Compute Shader Pathtracing", NULL, NULL);
	if (window == NULL) {
		printf("Failed to create GLFW window!\n");
		return -1;
	}
	glfwMakeContextCurrent(window);

	if (GLEW_OK != glewInit()) {
		printf("Failed to initialize GLEW!.\n");
		return -1;
	}

	printf("Renderer: %s\n", glGetString(GL_RENDERER));
	printf("OpenGL version supported %s\n", glGetString(GL_VERSION));
#ifdef GL_SHADING_LANGUAGE_VERSION
	printf("Supported GLSL version is %s.\n", (char *)glGetString(GL_SHADING_LANGUAGE_VERSION));
#endif
    printf("Using GLEW version %s.\n", glewGetString(GLEW_VERSION));
	printf("------------------------------\n");
    printf("Press ESCAPE to exit.\n");
	
	glfwSetFramebufferSizeCallback(window, windowSizeCallback);
	glfwSetKeyCallback(window, keyCallback);
	glfwSetCursorPosCallback(window, cursorCallback);
	glfwSetScrollCallback(window, scrollCallback);
	windowSizeCallback(window, windowWidth, windowHeight);

	initShaders();
	createBufferObjects();

	double previousTime = glfwGetTime();
	int frameCount = 0;
	std::cout.imbue(std::locale(""));
	
	while (!glfwWindowShouldClose(window)) {
		if (listenerFlags.load) {
			loadShaders();
		}

		if (spinning) {
			eyePos = glm::rotateY(eyePos, 0.001f);
			globalFrameCount = 0;
		}

		createMatrices();
		renderToTexture();
		renderToScreen();

		double currentTime = glfwGetTime();
		frameCount++;
		globalFrameCount++;
		globalTime = 0.001f * globalFrameCount;

		if (currentTime - previousTime >= 1.0) {
			GLuint* rayData = (GLuint*)glMapBufferRange(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(int), GL_MAP_READ_BIT);
			if (rayData) {
				glUnmapBuffer(GL_ATOMIC_COUNTER_BUFFER);
				std::cout << "Rays Traced: " << *rayData << std::endl;
				printf("Anti-Aliased Samples per Pixel: %d\n", globalFrameCount);
				checkForOpenGLErrors();
			}

			printf("Framerate: %d\n", frameCount);
			frameCount = 0;
			previousTime = currentTime;
		}

		glfwSwapBuffers(window);
		glfwWaitEventsTimeout(1.0/300.0);
	}

	listenerFlags.exit = true;
	shaderFileListenerThread.join();
	glfwTerminate();
	return 0;
}
