#pragma once

#include <vector>
#include <map>
#include <string>
#include <glm/glm.hpp>

#include <Visualisation/GLWindow.hpp>
#include <Visualisation/openGL/Mesh.hpp>
#include <Visualisation/openGL/Shader.hpp>

class GLFWwindow;
class TrackedObject;

enum class MeshNames
{
	unknown,
	robot,
	tag,
	arena,
	brio,
	skybox,
	axis,
	trackercube,
	toptracker,
	solarpanel
};

struct GLObject
{
	MeshNames type;
	glm::mat4 location;
	std::string metadata;

	GLObject(MeshNames InType = MeshNames::unknown, glm::mat4 InLoc = glm::mat4(1), std::string InMetadata = "")
		:type(InType), location(InLoc), metadata(InMetadata)
	{

	}

	GLObject(MeshNames InType, double x, double y, double z, std::string InMetadata = "");
};


class BoardGL : public GLWindow
{
private:
	GLuint VertexArrayID;
	Shader ShaderProgram;

	bool MeshesLoaded = false, TagsLoaded= false, TagLoadWarning=false;
	std::map<MeshNames, Mesh> Meshes;
	std::vector<Texture> TagTextures;

	glm::mat4 GetVPMatrix(glm::vec3 forward, glm::vec3 up) const;
public:

	BoardGL(std::string name = "Cyclops");

	virtual ~BoardGL();

	bool LookingAround = false; //Is left button pressed ?
	double lastCursorX, lastCursorY;
	float FoV = 75.f, mouseSpeed = 0.002f;
	float horizontalAngle = 0.f, verticalAngle = -M_PI_2;
	glm::vec3 cameraPosition = glm::vec3(0,0,2);

	double lastTime;

	glm::vec3 GetDirection() const;

	glm::vec3 GetRightVector() const;

	void HandleInputs(); //Move camera and stuff

	void LoadModels();
	void LoadTags();

	bool Tick(std::vector<GLObject> data); //Run display loop for these objects, returns false if exit was asked.

	void runTest();

	virtual void WindowSizeCallback(int width, int height) override;

};
