#pragma once

#include <string>
#include <vector>

#include <opencv2/core.hpp>

#include <GL/glew.h>
#include <glm/glm.hpp>

struct Texture
{
private:
	GLuint TextureID;

	cv::Size LastSize;

public:
	cv::Mat Texture;

	bool valid = false;

	void LoadFromFile(std::string path);

	void Bind(); //Send the texutre to the GPU

	void Refresh(); //Update the GPU texture with the one inside here

	void Draw(); //Set the texture as the currently active texture

	GLuint GetTextureID() 
	{
		return TextureID;
	}
};
