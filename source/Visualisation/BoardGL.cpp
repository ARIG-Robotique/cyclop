#include "Visualisation/BoardGL.hpp"

#include <iostream>
#include <fstream>
#include <tuple>
#include <optional>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>	

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#include <assimp/Importer.hpp>

#include "GlobalConf.hpp"
#include "Misc/metadata.hpp"
#include "Visualisation/openGL/Mesh.hpp"

using namespace std;


static string shaderfolder = "../source/Visualisation/openGL/";

GLObject::GLObject(MeshNames InType, double x, double y, double z, std::string InMetadata)
{
	type = InType;
	location = glm::translate(glm::mat4(1), {x,y,z});
	metadata = InMetadata;
}

void BoardGL::WindowSizeCallback(int width, int height)
{
	glViewport(0, 0, width, height);
}

glm::mat4 BoardGL::GetVPMatrix(glm::vec3 forward, glm::vec3 up)
{
	int winwidth, winheight;
	glfwGetWindowSize(Window, &winwidth, &winheight);

	glm::mat4 CameraMatrix = glm::lookAt(
		cameraPosition,
		cameraPosition+forward,
		up
	);

	glm::mat4 projectionMatrix = glm::perspective(
		glm::radians(FoV),						// The vertical Field of View, in radians
		(float) winwidth / (float)winheight,	//Aspect ratio
		0.01f,									// Near clipping plane.
		200.0f									// Far clipping plane.
	);

	return projectionMatrix * CameraMatrix;
}

glm::vec3 BoardGL::GetDirection()
{
	// Direction : Spherical coordinates to Cartesian coordinates conversion
	double sinh, cosh;
	sincos(horizontalAngle, &sinh, &cosh);
	double sinv, cosv;
	sincos(verticalAngle, &sinv, &cosv);
	glm::vec3 direction(
		cosv * sinh,
		cosv * cosh,
		sinv
	);
	return direction;
}


glm::vec3 BoardGL::GetRightVector()
{
	// Right vector
	double sinh, cosh;
	sincos(horizontalAngle, &sinh, &cosh);
	glm::vec3 right = glm::vec3(
		cosh,
		-sinh,
		0
	);
	return right;
}




void BoardGL::HandleInputs()
{
	double currentTime = glfwGetTime();
	float deltaTime = float(currentTime - lastTime);
	int winwidth, winheight;
	glfwGetWindowSize(Window, &winwidth, &winheight);

	if (glfwGetMouseButton(Window, GLFW_MOUSE_BUTTON_LEFT))
	{
		// Get mouse position
		double xpos, ypos;
		if (LookingAround)
		{
			glfwGetCursorPos(Window, &xpos, &ypos);
		}
		else
		{
			LookingAround = true;
			xpos = winwidth/2.0;
			ypos = winheight/2.0;
			glfwSetInputMode(Window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
		}
		
		glfwSetCursorPos(Window, winwidth/2.0, winheight/2.0);
		// Compute new orientation
		double xdiff = winwidth/2.0 - xpos, ydiff = winheight/2.0 - ypos;
		horizontalAngle -= mouseSpeed * xdiff;
		verticalAngle 	+= mouseSpeed * ydiff;
		//cout << "X: " << xdiff << " Y :" << ydiff << " h: " << horizontalAngle << " v: " << verticalAngle << endl;
		verticalAngle = clamp<float>(verticalAngle, -M_PI_2, M_PI_2);
	}
	else
	{
		LookingAround = false;
		glfwSetInputMode(Window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	}
	

	

	glm::vec3 direction = GetDirection();
	glm::vec3 right = GetRightVector();
	glm::vec3 up = glm::cross(right, direction);

	float speed = 1;
	// Move forward
	if (glfwGetKey(Window, GLFW_KEY_UP) == GLFW_PRESS){
		cameraPosition += direction * deltaTime * speed;
	}
	// Move backward
	if (glfwGetKey(Window, GLFW_KEY_DOWN) == GLFW_PRESS){
		cameraPosition -= direction * deltaTime * speed;
	}
	// Strafe right
	if (glfwGetKey(Window, GLFW_KEY_RIGHT) == GLFW_PRESS){
		cameraPosition += right * deltaTime * speed;
	}
	// Strafe left
	if (glfwGetKey(Window, GLFW_KEY_LEFT) == GLFW_PRESS){
		cameraPosition -= right * deltaTime * speed;
	}
	// Move up
	if (glfwGetKey(Window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS){
		cameraPosition += up * deltaTime * speed;
	}
	// Move down
	if (glfwGetKey(Window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS){
		cameraPosition -= up * deltaTime * speed;
	}

	//cout << "pos: " << cameraPosition.x << " " << cameraPosition.y << " " << cameraPosition.z << endl;

	lastTime = currentTime;
}

void BoardGL::LoadModels()
{
	if (MeshesLoaded)
	{
		return;
	}
	string assetpath = "../assets/";
	static const map<MeshNames, tuple<string, optional<string>>> meshpathes = 
	{
		{MeshNames::arena, {"board.obj", "boardtex.png"}},
		{MeshNames::robot, {"robot.obj", nullopt}},
		{MeshNames::robot_tray, {"robot/tray.obj", nullopt}},
		{MeshNames::robot_claw, {"robot/claws.obj", nullopt}},
		{MeshNames::axis, {"axis.obj", nullopt}},
		{MeshNames::brio, {"BRIO.obj", nullopt}},
		{MeshNames::skybox, {"skybox.obj", nullopt}},
		{MeshNames::tag, {"tag.obj", nullopt}},
		{MeshNames::trackercube, {"trackercube.obj", nullopt}},
		{MeshNames::toptracker, {"toptracker.obj", nullopt}},

		{MeshNames::browncake,	{"cakes/cake.obj", "cakes/cakebrown.png"}},
		{MeshNames::yellowcake,	{"cakes/cake.obj", "cakes/cakeyellow.png"}},
		{MeshNames::pinkcake,	{"cakes/cake.obj", "cakes/cakepink.png"}},
		{MeshNames::cherry,	{"cherry.obj", nullopt}}
	};
	//cout << "Loading meshes" << endl;

	for (auto iterator : meshpathes)
	{
		Meshes[iterator.first] = Mesh();
		Mesh &thismesh = Meshes[iterator.first];
		auto &second = iterator.second;
		auto &meshpath = get<0>(second);
		auto &textpath = get<1>(second);
		if (textpath.has_value())
		{
			thismesh.LoadFromFile(assetpath + meshpath, assetpath + textpath.value());
		}
		else
		{
			thismesh.LoadFromFile(assetpath + meshpath);
		}
		thismesh.BindMesh();
	}
	MeshesLoaded = true;
}
	

void BoardGL::LoadTags()
{
	if (TagsLoaded)
	{
		return;
	}
	glfwMakeContextCurrent(Window);

	TagTextures.resize(100);
	auto& det = GetArucoDetector();
	auto& dict = det.getDictionary();
	for (int i = 0; i < 100; i++)
	{
		cv::Mat texture;
		cv::aruco::generateImageMarker(dict, i, 60, texture, 1);
		cv::cvtColor(texture, TagTextures[i].Texture, cv::COLOR_GRAY2BGR);
		//TagTextures[i].Texture = texture;
		TagTextures[i].valid = true;
		TagTextures[i].Bind();
	}
	TagsLoaded = true;
	glfwMakeContextCurrent(NULL);
}

void BoardGL::Start(string name)
{
	GLCreateWindow(1280, 720, name);
	// Create and compile our GLSL program from the shaders
	ShaderProgram.LoadShader(shaderfolder + "vertexshader.vs", shaderfolder + "fragmentshader.fs");

	glGenVertexArrays(1, &VertexArrayID);
	glBindVertexArray(VertexArrayID);
	LoadModels();
	
	glfwMakeContextCurrent(NULL);

	//cout << "OpenGL init done!" << endl;
}

bool BoardGL::Tick(std::vector<GLObject> data)
{
	glfwMakeContextCurrent(Window);
	glfwPollEvents();
	HandleInputs();
	glm::vec3 direction = GetDirection();
	glm::vec3 right = GetRightVector();

	// Up vector : perpendicular to both direction and right
	glm::vec3 up = glm::cross(right, direction);

	glm::mat4 VPMatrix = GetVPMatrix(direction, up);

	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glUseProgram(ShaderProgram.ProgramID);

	GLuint MatrixID = glGetUniformLocation(ShaderProgram.ProgramID, "MVP"); //projection matrix handle
	GLuint TextureSamplerID  = glGetUniformLocation(ShaderProgram.ProgramID, "TextureSampler"); //texture sampler handle
	glUniform1i(TextureSamplerID, 0); //set texture sampler to use texture 0
	GLuint ParameterID = glGetUniformLocation(ShaderProgram.ProgramID, "Parameters");
	GLuint ScaleID = glGetUniformLocation(ShaderProgram.ProgramID, "scale");


	glUniformMatrix4fv(MatrixID, 1, GL_FALSE, &VPMatrix[0][0]);
	glUniform1f(ScaleID, 1);
	Meshes[MeshNames::axis].Draw(ParameterID);
	Meshes[MeshNames::skybox].Draw(ParameterID);

	

	for (int i = 0; i < data.size(); i++)
	{
		auto &odata = data[i];

		glm::mat4 MVPMatrix = VPMatrix * odata.location;

		glUniformMatrix4fv(MatrixID, 1, GL_FALSE, &MVPMatrix[0][0]);
		glUniform1f(ScaleID, 1);
		switch (odata.type)
		{
		case MeshNames::unknown:
			break;
		case MeshNames::tag :
			{
				if (!TagsLoaded)
				{
					cerr << "WARNING Tried to display tags but tags aren't loaded" << endl;
					break;
				}
				int number; float scale;
				GetTag(odata.metadata, scale, number); 
				glUniform1f(ScaleID, scale);
				TagTextures[number].Draw();
				Meshes[MeshNames::tag].Draw(ParameterID, true);
			}
			break;
		case MeshNames::brio : //Add an axis to the camera
			Meshes[MeshNames::axis].Draw(ParameterID);
		default:
			Meshes[odata.type].Draw(ParameterID);
			break;
		}
	}

	glfwSwapBuffers(Window);

	bool IsDone = glfwGetKey(Window, GLFW_KEY_ESCAPE ) != GLFW_PRESS && glfwWindowShouldClose(Window) == 0;

	glfwMakeContextCurrent(NULL);

	return IsDone;
}

void BoardGL::runTest()
{
	Start();

	int winwidth, winheight;
	glfwGetWindowSize(Window, &winwidth, &winheight);
	

	lastTime = glfwGetTime();

	do{
		HandleInputs();

		glm::vec3 direction = GetDirection();
		glm::vec3 right = GetRightVector();

		// Up vector : perpendicular to both direction and right
		glm::vec3 up = glm::cross(right, direction);

		// Clear the screen. It's not mentioned before Tutorial 02, but it can cause flickering, so it's there nonetheless.
		glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glUseProgram(ShaderProgram.ProgramID);



		glm::mat4 MVPmatrix = GetVPMatrix(direction, up) /** model*/;

		// Get a handle for our "MVP" uniform
		// Only during the initialisation
		GLuint MatrixID = glGetUniformLocation(ShaderProgram.ProgramID, "MVP");
		
		// Send our transformation to the currently bound shader, in the "MVP" uniform
		// This is done in the main loop since each model will have a different MVP matrix (At least for the M part)
		glUniformMatrix4fv(MatrixID, 1, GL_FALSE, &MVPmatrix[0][0]);

		Meshes[MeshNames::robot].Draw();

		// Swap buffers
		glfwSwapBuffers(Window);
		glfwPollEvents();

	} // Check if the ESC key was pressed or the Window was closed
	while( glfwGetKey(Window, GLFW_KEY_ESCAPE ) != GLFW_PRESS &&
		glfwWindowShouldClose(Window) == 0 );
}