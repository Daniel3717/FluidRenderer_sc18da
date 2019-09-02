//Standard headers
#include <stdio.h>
#include <stdlib.h>

//GLEW
#include <GL/glew.h>

//GLFW for window and keyboard
#include <GLFW/glfw3.h>

//GLM for maths
#include <glm/glm.hpp>

//For loading shaders
#include <common/shader.hpp>

//using namespace glm;
//I'll try not using this so I can know for sure what comes from glm

//Include matrix transform functions
#include <glm/gtc/matrix_transform.hpp>

//For file reading
#include <fstream>

//For debug output
#include <iostream>
//For cout-ing strings
#include <string>

//For std::vector
#include <vector>

//For camera movement
#include <common/controls.hpp>

#include <sstream>

//For reading cubemap images
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

const int IMAGE_SIZE = 1024;

//WIDTH and HEIGHT have been blocked to have the same size for some easier computations in shaders
const int WIDTH = IMAGE_SIZE;
const int HEIGHT = IMAGE_SIZE;

const float SPHERE_RADIUS = 0.2f;

const int MAX_FRAMES = 790;

std::vector<GLfloat> mPointsPositions[MAX_FRAMES];

std::vector<GLfloat> mPointsColours[MAX_FRAMES];

const glm::vec3 LIGHT_POSITION_WORLD = glm::vec3(0.0f, 10.0f, 0.0f);

GLFWwindow* mWindow;

const int SMOOTHING_ITERATIONS = 2;

void loadPoints()
{
	for (int frame = 0; frame < MAX_FRAMES; frame++)
	{
		mPointsPositions[frame].clear();
		mPointsColours[frame].clear();

		std::string lFilename = "frames/frame";
		lFilename += std::to_string(frame +1); 
		//std::cout << lFilename << '\n';
		std::ifstream lPointsIn(lFilename, std::ios::in);
		int lNrPoints;
		lPointsIn >> lNrPoints;

		for (int i = 0; i < lNrPoints; i++)
		{
			//std::cout << i << ' ';

			float lX, lY, lZ;
			lPointsIn >> lX >> lY >> lZ;
			//std::cout << lX << ' ' << lY << ' ' << lZ << '\n';
			mPointsPositions[frame].push_back(lX);
			mPointsPositions[frame].push_back(lY);
			mPointsPositions[frame].push_back(lZ);

			mPointsColours[frame].push_back(0);
			mPointsColours[frame].push_back(0);
			mPointsColours[frame].push_back(0);
		}

		lPointsIn.close();
	}
}




//First render (for depth and such) functions and variables ----->

GLuint mFRProgramID, mFRMVPID, mFRInvPID, mFRMVID, mFRInvMVPID, mFRCamPosID, mFRSphereRadiusID, mFRViewLightPosID, mFRNearFarID, mFRImageSizeID;

GLuint mFRFramebufferName[2], mFRRenderToTexture[2], mFRDepthRenderbuffer[2];
//Index 0 corresponds to near-side First Render, index 1 corresponds to far-side First Render

void setupCommonFirstRender()
{
	//Load shaders
	mFRProgramID = LoadShaders("firstRender.vertexshader", "firstRender.fragmentshader");

	//Get handle of the MVP uniform in the shaders
	mFRMVPID = glGetUniformLocation(mFRProgramID, "MVP");
	mFRInvPID = glGetUniformLocation(mFRProgramID, "invP");
	mFRInvMVPID = glGetUniformLocation(mFRProgramID, "invMVP");
	mFRMVID = glGetUniformLocation(mFRProgramID, "MV");
	mFRCamPosID = glGetUniformLocation(mFRProgramID, "cameraPos");
	mFRSphereRadiusID = glGetUniformLocation(mFRProgramID, "sphereRadius");
	mFRViewLightPosID = glGetUniformLocation(mFRProgramID, "viewLightPos");
	mFRNearFarID = glGetUniformLocation(mFRProgramID, "nearFar");
	mFRImageSizeID = glGetUniformLocation(mFRProgramID, "imageSize");

	//Enable point size
	glEnable(GL_PROGRAM_POINT_SIZE);
}


//Index 0 corresponds to near-side First Render, index 1 corresponds to far-side First Render
bool setupFirstRender(int pSideIndex)
{
	//Creating a render target
	// The framebuffer, which regroups 0, 1, or more textures, and 0 or 1 depth buffer.
	mFRFramebufferName[pSideIndex] = 0;
	glGenFramebuffers(1, &mFRFramebufferName[pSideIndex]);
	glBindFramebuffer(GL_FRAMEBUFFER, mFRFramebufferName[pSideIndex]);

	// The texture we're going to render to
	glGenTextures(1, &mFRRenderToTexture[pSideIndex]);

	// "Bind" the newly created texture : all future texture functions will modify this texture
	glBindTexture(GL_TEXTURE_2D, mFRRenderToTexture[pSideIndex]);

	// Give an empty image to OpenGL ( the last "0" )
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, WIDTH, HEIGHT, 0, GL_RGBA, GL_FLOAT, 0);

	// Poor filtering. Needed ! (dunno why but that's what they said)
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	// The depth buffer
	glGenRenderbuffers(1, &mFRDepthRenderbuffer[pSideIndex]);
	glBindRenderbuffer(GL_RENDERBUFFER, mFRDepthRenderbuffer[pSideIndex]);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, WIDTH, HEIGHT);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, mFRDepthRenderbuffer[pSideIndex]);

	// Set "mFRRenderToTexture" as our colour attachement #0
	glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, mFRRenderToTexture[pSideIndex], 0);

	// Set the list of draw buffers.
	GLenum lFRDrawBuffers[1] = { GL_COLOR_ATTACHMENT0 };
	glDrawBuffers(1, lFRDrawBuffers); // "1" is the size of lFRDrawBuffers
	
	// Always check that our framebuffer is ok
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		return false;
	}

	return true;
}

void drawFirstRender(int pFrame, int pSideIndex)
{
	//Enable depth-testing
	glEnable(GL_DEPTH_TEST);

	//Choosing which side to render, adjusting clear value
	if (pSideIndex == 0)
	{
		glClearDepth(1.0f);
		glDepthFunc(GL_LESS);
	}
	else
	{
		glClearDepth(0.0f);
		glDepthFunc(GL_GREATER);
	}

	//Disable alpha-blending (will send 0 value for coords where there's no water)
	glDisable(GL_BLEND);

	// Render to our framebuffer (i.e. the texture)
	glBindFramebuffer(GL_FRAMEBUFFER, mFRFramebufferName[pSideIndex]);
	glViewport(0, 0, WIDTH, HEIGHT); // Render on the whole framebuffer, complete from the lower left corner to the upper right

	//clear screen (color attachment and depth attachment)
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	//use the shaders I just loaded
	glUseProgram(mFRProgramID);

	//---- Setting up vertex positions ----
	//Get something for the vertex positions buffer handle
	GLuint lVertexPositionsBuffer;
	//Generate 1 buffer, get a handle
	glGenBuffers(1, &lVertexPositionsBuffer);
	//Bind openGL stuff
	glBindBuffer(GL_ARRAY_BUFFER, lVertexPositionsBuffer);
	//Pass data to buffer
	glBufferData(GL_ARRAY_BUFFER, mPointsPositions[pFrame].size() * sizeof(GLfloat), &mPointsPositions[pFrame][0], GL_STATIC_DRAW);
	//---- End of setting up vertex positions ----

	//---- Setting up vertex colours ----
	//Similar to above
	GLuint lVertexColoursBuffer;
	glGenBuffers(1, &lVertexColoursBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, lVertexColoursBuffer);
	glBufferData(GL_ARRAY_BUFFER, mPointsColours[pFrame].size() * sizeof(GLfloat), &mPointsColours[pFrame][0], GL_STATIC_DRAW);
	//---- End of setting up vertex colours ----

	//Pass vertex positions
	{
		//Enable a buffer at position 0
		glEnableVertexAttribArray(0);
		//Bind buffer
		glBindBuffer(GL_ARRAY_BUFFER, lVertexPositionsBuffer);
		//Declare where to find vertices positions from this buffer
		glVertexAttribPointer(
			0,        //vertex position is at position 0 in shader
			3,        //3D positions
			GL_FLOAT, //floats for positions
			GL_FALSE, //not normalized
			0,        //stride of 0 through vertex buffer
			(void*)0  //start at index 0 in vertex buffer
		);
	}

	//Pass vertex colours
	{
		//Enable a buffer at position 0
		glEnableVertexAttribArray(1);
		//Bind buffer
		glBindBuffer(GL_ARRAY_BUFFER, lVertexColoursBuffer);
		//Declare where to find vertices colours from this buffer
		glVertexAttribPointer(
			1,        //vertex position is at position 0 in shader
			3,        //RGB colours
			GL_FLOAT, //floats for colours
			GL_FALSE, //not normalized
			0,        //stride of 0 through buffer
			(void*)0  //start at index 0 in buffer
		);
	}


	//Compute MVP matrices
	glm::mat4 lProjectionMatrix = getProjectionMatrix();
	glm::mat4 lViewMatrix = getViewMatrix();
	glm::mat4 lModelMatrix = glm::mat4(1.0);
	glm::mat4 lMVP = lProjectionMatrix * lViewMatrix * lModelMatrix;
	glm::mat4 lMV = lViewMatrix * lModelMatrix;

	//Get camera position
	glm::vec3 lCameraPos = getPosition();

	//Compute inverse projection matrix
	glm::mat4 lInvProjection = glm::inverse(lProjectionMatrix);

	//Compute inverse MVP
	glm::mat4 lInvMVP = glm::inverse(lMVP);

	//Compute light position in view coords
	glm::vec4 lViewLightPos4 = lMV * glm::vec4(LIGHT_POSITION_WORLD, 1.0f);
	glm::vec3 lViewLightPos = glm::vec3(0.0f, 0.0f, 0.0f);
	lViewLightPos.x = lViewLightPos4.x / lViewLightPos4.w;
	lViewLightPos.y = lViewLightPos4.y / lViewLightPos4.w;
	lViewLightPos.z = lViewLightPos4.z / lViewLightPos4.w;

	//Send MVP matrix (doing it here since we may have different models or they may move)
	glUniformMatrix4fv(mFRMVPID, 1, GL_FALSE, &lMVP[0][0]);

	//Send MV matrix
	glUniformMatrix4fv(mFRMVID, 1, GL_FALSE, &lMV[0][0]);

	//Send cameraPos
	glUniform3fv(mFRCamPosID, 1, &lCameraPos[0]);

	//Send inverse projection matrix
	glUniformMatrix4fv(mFRInvPID, 1, GL_FALSE, &lInvProjection[0][0]);

	//Send inverse MVP matrix
	glUniformMatrix4fv(mFRInvMVPID, 1, GL_FALSE, &lInvMVP[0][0]);

	//Send sphere radius
	glUniform1f(mFRSphereRadiusID, SPHERE_RADIUS);

	//Send lViewLightPos
	glUniform3fv(mFRViewLightPosID, 1, &lViewLightPos[0]);

	//Send nearFar
	glUniform1i(mFRNearFarID, pSideIndex);

	//Send imageSize
	glUniform1i(mFRImageSizeID, IMAGE_SIZE);

	//Actual draw command
	glDrawArrays(GL_POINTS, 0, mPointsPositions[pFrame].size()/3);//Starting from 0, drawing all vertices (the /3 is because we need point count), drawing points

	glDisableVertexAttribArray(0);
	glDisableVertexAttribArray(1);

}
//<----- End of first render functions and variables




//Smoothing render functions and variables

GLuint mSRProgramID, mSRVertexBuffer, mSRTexID;

std::vector<GLuint> mSRFramebufferName[2], mSRRenderToTexture[2], mSRDepthRenderbuffer[2];

void initializeSmoothingRenderVariables()
{
	for (int i = 0; i <= 1; i++)
	{
		mSRFramebufferName[i].clear();
		mSRFramebufferName[i].resize(SMOOTHING_ITERATIONS);

		mSRRenderToTexture[i].clear();
		mSRRenderToTexture[i].resize(SMOOTHING_ITERATIONS);

		mSRDepthRenderbuffer[i].clear();
		mSRDepthRenderbuffer[i].resize(SMOOTHING_ITERATIONS);
	}
}

void setupCommonSmoothingRender()
{
	// The fullscreen quad's FBO
	static const GLfloat lSRVertexBufferData[] = {
		-1.0f, -1.0f, 0.0f,
		1.0f, -1.0f, 0.0f,
		-1.0f,  1.0f, 0.0f,
		-1.0f,  1.0f, 0.0f,
		1.0f, -1.0f, 0.0f,
		1.0f,  1.0f, 0.0f,
	};

	glGenBuffers(1, &mSRVertexBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, mSRVertexBuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(lSRVertexBufferData), lSRVertexBufferData, GL_STATIC_DRAW);

	//Load shaders
	mSRProgramID = LoadShaders("smoothingQuad.vertexshader", "smoothingQuad.fragmentshader");

	//Get uniform IDs
	mSRTexID = glGetUniformLocation(mSRProgramID, "renderedTexture");
}

bool setupSmoothingRender(int pIteration, int pSideIndex)
{
	//Creating a render target
	// The framebuffer, which regroups 0, 1, or more textures, and 0 or 1 depth buffer.
	mSRFramebufferName[pSideIndex][pIteration] = 0;
	glGenFramebuffers(1, &mSRFramebufferName[pSideIndex][pIteration]);
	glBindFramebuffer(GL_FRAMEBUFFER, mSRFramebufferName[pSideIndex][pIteration]);

	// The texture we're going to render to
	glGenTextures(1, &mSRRenderToTexture[pSideIndex][pIteration]);

	// "Bind" the newly created texture : all future texture functions will modify this texture
	glBindTexture(GL_TEXTURE_2D, mSRRenderToTexture[pSideIndex][pIteration]);

	// Give an empty image to OpenGL ( the last "0" )
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, WIDTH, HEIGHT, 0, GL_RGBA, GL_FLOAT, 0);

	// Poor filtering. Needed ! (dunno why but that's what they said)
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	// The depth buffer
	glGenRenderbuffers(1, &mSRDepthRenderbuffer[pSideIndex][pIteration]);
	glBindRenderbuffer(GL_RENDERBUFFER, mSRDepthRenderbuffer[pSideIndex][pIteration]);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, WIDTH, HEIGHT);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, mSRDepthRenderbuffer[pSideIndex][pIteration]);

	// Set "mSRRenderToTexture" as our colour attachement #0
	glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, mSRRenderToTexture[pSideIndex][pIteration], 0);

	// Set the list of draw buffers.
	GLenum lSRDrawBuffers[1] = { GL_COLOR_ATTACHMENT0 };
	glDrawBuffers(1, lSRDrawBuffers); // "1" is the size of lSRDrawBuffers

	// Always check that our framebuffer is ok
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		return false;
	}
	return true;
}

void drawSmoothingRender(int pIteration, int pSideIndex)
{
	//Disable depth testing
	glDisable(GL_DEPTH_TEST);

	//Disable alpha blending
	glDisable(GL_BLEND);

	// Render to our framebuffer (i.e. the texture)
	glBindFramebuffer(GL_FRAMEBUFFER, mSRFramebufferName[pSideIndex][pIteration]);
	glViewport(0, 0, WIDTH, HEIGHT); // Render on the whole framebuffer, complete from the lower left corner to the upper right

	// Clear the screen
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	//use the shaders I just loaded
	glUseProgram(mSRProgramID);

	// Bind our texture in Texture Unit 0
	glActiveTexture(GL_TEXTURE0);
	if (pIteration == 0)
		glBindTexture(GL_TEXTURE_2D, mFRRenderToTexture[pSideIndex]);
	else
		glBindTexture(GL_TEXTURE_2D, mSRRenderToTexture[pSideIndex][pIteration - 1]);
	// Set our sampler to use the corresponding texture

	//Send uniforms
	//Send texture id
	glUniform1i(mSRTexID, 0);

	// 1rst attribute buffer : vertices
	glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, mSRVertexBuffer);
	glVertexAttribPointer(
		0,                  // attribute 0. No particular reason for 0, but must match the layout in the shader.
		3,                  // 3D positions
		GL_FLOAT,           // type
		GL_FALSE,           // normalized?
		0,                  // stride
		(void*)0            // array buffer offset
	);

	// Draw the triangles !
	glDrawArrays(GL_TRIANGLES, 0, 6); // 2*3=6 indices starting at 0 for 2 triangles

	glDisableVertexAttribArray(0);
}

//<----- End of smoothing render functions and variables

//Cubemap functions and variables ----->

GLuint mCShaderID, mCVertexBuffer, mCProjectionID, mCViewID, mCTexID;
unsigned int mCTexture;

GLuint mCFramebufferName, mCRenderToTexture, mCDepthRenderbuffer;

//Taken from https://learnopengl.com/Advanced-OpenGL/Cubemaps
unsigned int loadCubemap(std::vector<std::string> faces)
{
	unsigned int textureID;
	glGenTextures(1, &textureID);
	glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

	int width, height, nrChannels;
	for (unsigned int i = 0; i < faces.size(); i++)
	{
		unsigned char *data = stbi_load(faces[i].c_str(), &width, &height, &nrChannels, 0);
		if (data)
		{
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
				0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data
			);
			stbi_image_free(data);
		}
		else
		{
			std::cout << "Cubemap texture failed to load at path: " << faces[i] << std::endl;
			stbi_image_free(data);
		}
	}
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	return textureID;
}

//Parts adapted from https://learnopengl.com/Advanced-OpenGL/Cubemaps
bool setupCubemap()
{

	std::vector<std::string> faces
	{
		"right.jpg",
		"left.jpg",
		"top.jpg",
		"bottom.jpg",
		"front.jpg",
		"back.jpg"
	};
	mCTexture = loadCubemap(faces);
	mCShaderID = LoadShaders("cubemap.vertexshader", "cubemap.fragmentshader");

	//Get handle of the MVP uniform in the shaders
	mCProjectionID = glGetUniformLocation(mCShaderID, "projection");
	mCViewID = glGetUniformLocation(mCShaderID, "view");
	mCTexID = glGetUniformLocation(mCShaderID, "skybox");

	static const GLfloat skyboxVertices[] = {
		// positions          
		-10.0f,  10.0f, -10.0f,
		-10.0f, -10.0f, -10.0f,
		10.0f, -10.0f, -10.0f,
		10.0f, -10.0f, -10.0f,
		10.0f,  10.0f, -10.0f,
		-10.0f,  10.0f, -10.0f,

		-10.0f, -10.0f,  10.0f,
		-10.0f, -10.0f, -10.0f,
		-10.0f,  10.0f, -10.0f,
		-10.0f,  10.0f, -10.0f,
		-10.0f,  10.0f,  10.0f,
		-10.0f, -10.0f,  10.0f,

		10.0f, -10.0f, -10.0f,
		10.0f, -10.0f,  10.0f,
		10.0f,  10.0f,  10.0f,
		10.0f,  10.0f,  10.0f,
		10.0f,  10.0f, -10.0f,
		10.0f, -10.0f, -10.0f,

		-10.0f, -10.0f,  10.0f,
		-10.0f,  10.0f,  10.0f,
		10.0f,  10.0f,  10.0f,
		10.0f,  10.0f,  10.0f,
		10.0f, -10.0f,  10.0f,
		-10.0f, -10.0f,  10.0f,

		-10.0f,  10.0f, -10.0f,
		10.0f,  10.0f, -10.0f,
		10.0f,  10.0f,  10.0f,
		10.0f,  10.0f,  10.0f,
		-10.0f,  10.0f,  10.0f,
		-10.0f,  10.0f, -10.0f,

		-10.0f, -10.0f, -10.0f,
		-10.0f, -10.0f,  10.0f,
		10.0f, -10.0f, -10.0f,
		10.0f, -10.0f, -10.0f,
		-10.0f, -10.0f,  10.0f,
		10.0f, -10.0f,  10.0f
	};

	glGenBuffers(1, &mCVertexBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, mCVertexBuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), skyboxVertices, GL_STATIC_DRAW);

	//Creating a render target
	// The framebuffer, which regroups 0, 1, or more textures, and 0 or 1 depth buffer.
	mCFramebufferName = 0;
	glGenFramebuffers(1, &mCFramebufferName);
	glBindFramebuffer(GL_FRAMEBUFFER, mCFramebufferName);

	// The texture we're going to render to
	glGenTextures(1, &mCRenderToTexture);

	// "Bind" the newly created texture : all future texture functions will modify this texture
	glBindTexture(GL_TEXTURE_2D, mCRenderToTexture);

	// Give an empty image to OpenGL ( the last "0" )
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, WIDTH, HEIGHT, 0, GL_RGBA, GL_FLOAT, 0);

	// Poor filtering. Needed ! (dunno why but that's what they said)
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	// The depth buffer
	glGenRenderbuffers(1, &mCDepthRenderbuffer);
	glBindRenderbuffer(GL_RENDERBUFFER, mCDepthRenderbuffer);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, WIDTH, HEIGHT);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, mCDepthRenderbuffer);

	// Set "mFRRenderToTexture" as our colour attachement #0
	glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, mCRenderToTexture, 0);

	// Set the list of draw buffers.
	GLenum lCDrawBuffers[1] = { GL_COLOR_ATTACHMENT0 };
	glDrawBuffers(1, lCDrawBuffers); // "1" is the size of lFRDrawBuffers

	// Always check that our framebuffer is ok
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		return false;
	}

	return true;
}

//Parts adapted from https://learnopengl.com/Advanced-OpenGL/Cubemaps
void drawCubemap()
{
	//Disable depth testing
	glDisable(GL_DEPTH_TEST);

	//Disable alpha blending
	glDisable(GL_BLEND);

	// Render to our framebuffer (i.e. the texture)
	glBindFramebuffer(GL_FRAMEBUFFER, mCFramebufferName);
	glViewport(0, 0, WIDTH, HEIGHT); // Render on the whole framebuffer, complete from the lower left corner to the upper right

	// Clear the screen
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Use the shaders provided
	glUseProgram(mCShaderID);

	//Setup texture
	// Bind our texture in Texture Unit 3
	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_CUBE_MAP, mCTexture);
	// Set our "skybox" sampler to use Texture Unit 3
	glUniform1i(mCTexID, 3);

	//Setup uniforms
	glm::mat4 lProjectionMatrix = getProjectionMatrix();
	glm::mat4 lViewMatrix = getViewMatrix();
	
	//Eliminating translations
	lViewMatrix = glm::mat4(glm::mat3(lViewMatrix));
	
	//Send projection and view
	glUniformMatrix4fv(mCProjectionID, 1, GL_FALSE, &lProjectionMatrix[0][0]);
	glUniformMatrix4fv(mCViewID, 1, GL_FALSE, &lViewMatrix[0][0]);

	// 1rst attribute buffer : vertices
	glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, mCVertexBuffer);
	glVertexAttribPointer(
		0,                  // attribute 0. No particular reason for 0, but must match the layout in the shader.
		3,                  // size
		GL_FLOAT,           // type
		GL_FALSE,           // normalized?
		0,                  // stride
		(void*)0            // array buffer offset
	);

	//Disable depth mask
	glDepthMask(GL_FALSE);

	// Draw the triangles !
	glDrawArrays(GL_TRIANGLES, 0, 36);//Starting from 0, drawing all vertices, drawing triangles

	//Enable depth mask
	glDepthMask(GL_TRUE);

	glDisableVertexAttribArray(0);
}

//<----- End of cubemap functions and variables


//Quad render functions and variables() ----->

GLuint mQRProgramID, mQRTimeID, mQRViewLightPosID, mQRVertexBuffer;

GLuint mQRTexNearID, mQRTexFarID, mQRTexSkyID;

void setupQuadRender()
{
	// The fullscreen quad's FBO
	static const GLfloat lQRVertexBufferData[] = {
		-1.0f, -1.0f, 0.0f,
		1.0f, -1.0f, 0.0f,
		-1.0f,  1.0f, 0.0f,
		-1.0f,  1.0f, 0.0f,
		1.0f, -1.0f, 0.0f,
		1.0f,  1.0f, 0.0f,
	};

	glGenBuffers(1, &mQRVertexBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, mQRVertexBuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(lQRVertexBufferData), lQRVertexBufferData, GL_STATIC_DRAW);

	// Create and compile our GLSL program from the shaders
	mQRProgramID = LoadShaders("quad.vertexshader", "quad.fragmentshader");
	mQRTexNearID = glGetUniformLocation(mQRProgramID, "renderedTextureNear");
	mQRTexFarID = glGetUniformLocation(mQRProgramID, "renderedTextureFar");
	mQRTexSkyID = glGetUniformLocation(mQRProgramID, "renderedTextureSky");
	mQRTimeID = glGetUniformLocation(mQRProgramID, "time");
	mQRViewLightPosID = glGetUniformLocation(mQRProgramID, "viewLightPos");
}

void drawQuadRender()
{
	//Disable depth testing
	glDisable(GL_DEPTH_TEST);

	//Disable alpha blending
	glDisable(GL_BLEND);

	/*
	//Obsolete, still keeping it in case it is needed, now skybox is done as a texture uniform
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	*/

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	// Render on the whole framebuffer, complete from the lower left corner to the upper right
	glViewport(0, 0, WIDTH, HEIGHT);

	// Clear the screen (skybox is in texture uniform)
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	

	// Use our shader
	glUseProgram(mQRProgramID);

	// Bind our smoothed near texture in Texture Unit 0
	glActiveTexture(GL_TEXTURE0);
	if (SMOOTHING_ITERATIONS>0)
		glBindTexture(GL_TEXTURE_2D, mSRRenderToTexture[0][SMOOTHING_ITERATIONS - 1]);
	else
		glBindTexture(GL_TEXTURE_2D, mFRRenderToTexture[0]);


	// Bind our smoothed far texture in Texture Unit 1
	glActiveTexture(GL_TEXTURE1);
	if (SMOOTHING_ITERATIONS>0)
		glBindTexture(GL_TEXTURE_2D, mSRRenderToTexture[1][SMOOTHING_ITERATIONS - 1]);
	else
		glBindTexture(GL_TEXTURE_2D, mFRRenderToTexture[1]);


	// Bind our skybox texture in Texture Unit 2
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, mCRenderToTexture);


	//Solve uniforms
	//Compute MVP matrices
	glm::mat4 lViewMatrix = getViewMatrix();
	glm::mat4 lModelMatrix = glm::mat4(1.0);
	glm::mat4 lMV = lViewMatrix * lModelMatrix;

	//Compute light position in view coords
	glm::vec4 lViewLightPos4 = lMV * glm::vec4(LIGHT_POSITION_WORLD, 1.0f);
	glm::vec3 lViewLightPos = glm::vec3(0.0f, 0.0f, 0.0f);
	lViewLightPos.x = lViewLightPos4.x / lViewLightPos4.w;
	lViewLightPos.y = lViewLightPos4.y / lViewLightPos4.w;
	lViewLightPos.z = lViewLightPos4.z / lViewLightPos4.w;

	//Send lViewLightPos
	glUniform3fv(mQRViewLightPosID, 1, &lViewLightPos[0]);

	//Send other uniforms
	glUniform1i(mQRTexNearID, 0); //Set near sampler to use texture 0
	glUniform1i(mQRTexFarID, 1); //Set far sampler to use texture 1
	glUniform1i(mQRTexSkyID, 2); //Set sky sampelr to use texture 2

	glUniform1f(mQRTimeID, (float)(glfwGetTime()*10.0f));

	// 1rst attribute buffer : vertices
	glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, mQRVertexBuffer);
	glVertexAttribPointer(
		0,                  // attribute 0. No particular reason for 0, but must match the layout in the shader.
		3,                  // 3D positions
		GL_FLOAT,           // type
		GL_FALSE,           // normalized?
		0,                  // stride
		(void*)0            // array buffer offset
	);

	// Draw the triangles !
	glDrawArrays(GL_TRIANGLES, 0, 6); // 2*3=6 indices starting at 0 for 2 triangles

	glDisableVertexAttribArray(0);
}

//Must keep same uniform names
void reloadQuadShaders()
{
	//Delete shaders
	glDeleteProgram(mQRProgramID);

	//Load shaders
	mQRProgramID = LoadShaders("quad.vertexshader", "quad.fragmentshader");
}

//<----- End of quad render functions and variables


//Setting up OpenGL ----->
bool setupWindow()
{
	//Init GLFW
	glewExperimental = true; //Needed for something called core profile apparently
	if (!glfwInit())
	{
		fprintf(stderr, "Failed to initialize GLFW\n");
		return false;
	}
	//Create openGL window
	glfwWindowHint(GLFW_SAMPLES, 4);//antialiasing, 4x
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3); //using openGL 3.x
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3); //using openGL x.3
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); //apparently needed for Mac
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); //apparently, this is so we don't get the old openGL, though I thought we did that in version major/minor?

	//create the window
	mWindow = glfwCreateWindow(WIDTH, HEIGHT, "Fluid rendering", NULL, NULL);

	if (mWindow == NULL)
	{
		fprintf(stderr, "Failed to open GLFW window\n");
		glfwTerminate();
		return false;
	}

	glfwMakeContextCurrent(mWindow); //Moar initializing (oh wait, this is for GLEW?)
	glewExperimental = true; //Moar core profile stuff
	if (glewInit() != GLEW_OK)
	{
		fprintf(stderr, "Failed to initialize GLEW\n");
		//wait but what about glfwTerminate?
		return false;
	}

	//Make a way to keep stuff open until we don't want it open anymore
	glfwSetInputMode(mWindow, GLFW_STICKY_KEYS, GL_TRUE); //Will follow with an exit-on-esc loop

	return true;
}

GLuint mVertexArray;
void setupVertexArray()
{
	//Create vertex array, mandatory
	glGenVertexArrays(1, &mVertexArray);
	glBindVertexArray(mVertexArray);
}
//<----- End of setting up OpenGL


//Fps tracker code----->
const int FRAMES_FOR_FPS = 30;
double mFps[FRAMES_FOR_FPS];
int mFpsIndex;
double mLastTime, mNowTime;

void initFpsTracker()
{
	//Setting up fps tracker
	double lFps[FRAMES_FOR_FPS];
	for (int i = 0; i < FRAMES_FOR_FPS; i++)
		lFps[i] = 0;
	mFpsIndex = 0;

	mLastTime = glfwGetTime();
	mNowTime = mLastTime;
}

void processFpsTracker()
{
	//Fps tracker stuff
	mNowTime = glfwGetTime();
	if (mLastTime != mNowTime) //avoid division by 0
	{
		double lLastFps = 1.0f / (mNowTime - mLastTime);

		mFps[mFpsIndex] = lLastFps;
		mFpsIndex = (mFpsIndex + 1) % FRAMES_FOR_FPS;
		int lFpsExist = 0;
		double lFpsAvg = 0;
		for (int i = 0; i<FRAMES_FOR_FPS; i++)
			if (mFps[i] != 0)
			{
				lFpsExist++;
				lFpsAvg += mFps[i];
			}
		if (lFpsExist != 0) //avoid division by 0
			lFpsAvg /= lFpsExist;

		std::ostringstream lFpsStringStream;
		lFpsStringStream << lFpsAvg;
		std::string lFpsString = lFpsStringStream.str();

		glfwSetWindowTitle(mWindow, lFpsString.c_str());
	}
	mLastTime = mNowTime;
}
//<----- End of fps tracker

int main()
{
	if (!setupWindow())
	{
		return -1;
	}

	setupVertexArray();
	//Doing those things outside loop since they need to be done only once

	//Load points in
	loadPoints();


	//Set clear color to black
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

	//Setups
	setupCommonFirstRender();
	if (!setupFirstRender(0))
	{
		return -1;
	}
	if (!setupFirstRender(1))
	{
		return -1;
	}

	initializeSmoothingRenderVariables();
	setupCommonSmoothingRender();
	for (int i = 0; i < SMOOTHING_ITERATIONS; i++)
	{
		if (!setupSmoothingRender(i, 0))
		{
			return -1;
		}
		if (!setupSmoothingRender(i, 1))
		{
			return -1;
		}
	}

	setupQuadRender();
	setupCubemap();

	int lFrame = 0;
	//End of only-once stuff to be done

	initFpsTracker();

	do {
		processFpsTracker();

		//Reloading shaders for quad rendering, note that MVPs should stay the same
		reloadQuadShaders();

		//Reading inputs
		computeMatricesFromInputs(mWindow);

		//Drawing to texture
		drawFirstRender(lFrame, 0);
		drawFirstRender(lFrame, 1);
		//End of drawing to texture

		//Smoothing
		for (int i = 0; i < SMOOTHING_ITERATIONS; i++)
		{
			drawSmoothingRender(i, 0);
			drawSmoothingRender(i, 1);
		}
		//End of smoothing

		//Drawing cubemap
		drawCubemap();
		//End of drawing cubemap

		//Drawing to quad
		drawQuadRender();
		//End of drawing to quad

		//Swap buffers (so by default we get the back/front buffers, this is easier than Vulkan)
		glfwSwapBuffers(mWindow);

		glfwPollEvents(); //What's this? Lemme try disabling it --- Results: If this is not here the window is unresponsive (can't move it, doesn't detect key presses)
	
		lFrame = (lFrame + 1) % MAX_FRAMES;
	} while (glfwGetKey(mWindow, GLFW_KEY_ESCAPE) != GLFW_PRESS && glfwWindowShouldClose(mWindow) == 0);

	return 0; //Just doing this for good measure, since we are in an int main
}
