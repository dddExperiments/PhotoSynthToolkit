/*
	Copyright (c) 2010 ASTRE Henri (http://www.visual-experiments.com)

	Permission is hereby granted, free of charge, to any person obtaining a copy
	of this software and associated documentation files (the "Software"), to deal
	in the Software without restriction, including without limitation the rights
	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
	copies of the Software, and to permit persons to whom the Software is
	furnished to do so, subject to the following conditions:

	The above copyright notice and this permission notice shall be included in
	all copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
	THE SOFTWARE.
*/

#include "OgreAppLogic.h"
#include "OgreApp.h"
#include <Ogre.h>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/convenience.hpp>
#include <boost/algorithm/string.hpp> 
#include <OgreTextureUnitState.h>

#include "DynamicLines.h"
#include "StatsFrameListener.h"
#include "PlyImporter.h"

using namespace Ogre;

namespace bf = boost::filesystem;

CameraPlane::CameraPlane(Ogre::Vector3 c, Ogre::Vector3 d, Ogre::Vector3 e, Ogre::Vector3 f)
{
	this->c = c;
	this->d = d;
	this->e = e;
	this->f = f;
}

OgreAppLogic::OgreAppLogic() : mApplication(0)
{
	// ogre
	mSceneMgr		= 0;
	mViewport		= 0;
	mCamera         = 0;
	mCameraNode     = 0;
	mObjectNode     = 0;
	mCameraIndex    = 0;
	mStatsFrameListener = 0;
	mAnimState = 0;
	mTimeUntilNextToggle = 0;
	mDistanceFromCamera = 499.0f;
	mOISListener.mParent = this;
	mDensePointCloud = NULL;
	mDensePointCloudFilePath = "";
	mBillboardSize = 0.01f;
	mBillboardInterval = 0.0005f;
	mFovSize = 40.0f;
	mFovInterval = 5.0f;
	mIsSparsePointCloudVisible = true;
	mIsCameraPlaneVisible      = false;
	mIsCameraActivated         = true;
	mThumbsAvailable           = false;
}

OgreAppLogic::~OgreAppLogic()
{}

// preAppInit
bool OgreAppLogic::preInit(const Ogre::StringVector &commandArgs)
{
	mProjectPath = commandArgs[0];
	std::stringstream bundlerFilePath;
	std::stringstream pictureFilePath;
	std::stringstream densePointCloudFilePath;
	std::stringstream firstThumbFilePath;

	std::string guidFilePath     = PhotoSynth::Parser::createFilePath(mProjectPath, PhotoSynth::Parser::guidFilename);
	std::string soapFilePath     = PhotoSynth::Parser::createFilePath(mProjectPath, PhotoSynth::Parser::soapFilename);
	std::string jsonFilePath     = PhotoSynth::Parser::createFilePath(mProjectPath, PhotoSynth::Parser::jsonFilename);
	std::string picsListFilePath = PhotoSynth::Parser::createFilePath(mProjectPath, "list.txt");
	parsePictureList(picsListFilePath);

	densePointCloudFilePath << mProjectPath << "pmvs\\models\\mesh.ply";

	if (bf::exists(densePointCloudFilePath.str()))
		mDensePointCloudFilePath = densePointCloudFilePath.str();

	mPhotoSynthParser = new PhotoSynth::Parser;	
	std::string guid = mPhotoSynthParser->getGuid(guidFilePath);
	mPhotoSynthParser->parseSoap(soapFilePath);
	mPhotoSynthParser->parseJson(jsonFilePath, guid);
	mPhotoSynthParser->parseBinFiles(mProjectPath);

	 firstThumbFilePath << mProjectPath << "thumbs\\00000000.jpg";
	
	if (bf::exists(firstThumbFilePath.str()))
		mThumbsAvailable = true;

	return true;
}

// postAppInit
bool OgreAppLogic::init(void)
{
	if (mThumbsAvailable)
			Ogre::ResourceGroupManager::getSingleton().addResourceLocation(PhotoSynth::Parser::createFilePath(mProjectPath, "thumbs"), "FileSystem", Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);

	createSceneManager();
	createViewport();
	createScene();
	createCamera();

	createAllCameraPlane();
	prepareTriangleIntersection();

	mStatsFrameListener = new StatsFrameListener(mApplication->getRenderWindow());
	mApplication->getOgreRoot()->addFrameListener(mStatsFrameListener);
	mStatsFrameListener->showDebugOverlay(true);

	mApplication->getKeyboard()->setEventCallback(&mOISListener);
	mApplication->getMouse()->setEventCallback(&mOISListener);

	return true;
}

bool OgreAppLogic::preUpdate(Ogre::Real deltaTime)
{
	return true;
}

bool OgreAppLogic::update(Ogre::Real deltaTime)
{
	if (mAnimState)
		mAnimState->addTime(deltaTime);

	bool result = processInputs(deltaTime);
	return result;
}

void OgreAppLogic::shutdown(void)
{
	mApplication->getOgreRoot()->removeFrameListener(mStatsFrameListener);
	delete mStatsFrameListener;
	mStatsFrameListener = 0;
	
	if(mSceneMgr)
		mApplication->getOgreRoot()->destroySceneManager(mSceneMgr);
	mSceneMgr = 0;
}

void OgreAppLogic::postShutdown(void)
{

}

//--------------------------------- Init --------------------------------

void OgreAppLogic::createSceneManager(void)
{
	mSceneMgr = mApplication->getOgreRoot()->createSceneManager(ST_GENERIC, "SceneManager");
}

void OgreAppLogic::createViewport(void)
{
	mViewport = mApplication->getRenderWindow()->addViewport(0);
}

void OgreAppLogic::createCamera(void)
{
	mCamera = mSceneMgr->createCamera("camera");
	mCamera->setNearClipDistance(0.005f);
	mCamera->setFarClipDistance(500);
	mCamera->setFOVy(Degree(45)); //FOVy camera Ogre = 20°
	mCamera->setAspectRatio((float) mViewport->getActualWidth() / (float) mViewport->getActualHeight());	
	mViewport->setCamera(mCamera);
	mViewport->setBackgroundColour(Ogre::ColourValue::White);

	mCameraNode = mSceneMgr->getRootSceneNode()->createChildSceneNode();
	mCameraNode2 = mCameraNode->createChildSceneNode();
	mCameraNode2->attachObject(mCamera);
}

void OgreAppLogic::createScene(void)
{	
	Ogre::Matrix3 mx;
	mx.FromAxisAngle(Ogre::Vector3::UNIT_X, Ogre::Degree(-90));

	SceneNode* root = mSceneMgr->getRootSceneNode()->createChildSceneNode();	
	SceneNode* pointCloud = root->createChildSceneNode();
	pointCloud->setOrientation(mx);
	
	const RenderSystemCapabilities* caps = Ogre::Root::getSingletonPtr()->getRenderSystem()->getCapabilities();
	bool useGeometryShader = caps->hasCapability(RSC_GEOMETRY_PROGRAM);

	const PhotoSynth::CoordSystem& coord = mPhotoSynthParser->getCoordSystem(0);
	std::vector<PhotoSynth::Vertex> vertices;
	for (unsigned int i=0; i<coord.nbBinFile; ++i)
		vertices.insert(vertices.end(), coord.pointClouds[i].vertices.begin(), coord.pointClouds[i].vertices.end());

	mSparsePointCloud = new GPUBillboardSet("sparsePointCloud", vertices, useGeometryShader);
	pointCloud->attachObject(mSparsePointCloud);	
	if (mDensePointCloudFilePath != "")
	{		
		mDensePointCloud = new GPUBillboardSet("densePointCloud", PlyImporter::importPly(mDensePointCloudFilePath), useGeometryShader);
		pointCloud->attachObject(mDensePointCloud);
		mIsSparsePointCloudVisible = false;
		mSparsePointCloud->setVisible(false);
	}
}

//--------------------------------- update --------------------------------

bool OgreAppLogic::processInputs(Ogre::Real deltaTime)
{
	const Degree ROT_SCALE = Degree(40.0f);
	const Real MOVE_SCALE = 2.0;
	Vector3 translateVector(Vector3::ZERO);
	Degree rotX(0);
	Degree rotY(0);
	OIS::Keyboard *keyboard = mApplication->getKeyboard();
	OIS::Mouse *mouse = mApplication->getMouse();

	if(keyboard->isKeyDown(OIS::KC_ESCAPE))
	{
		return false;
	}

	//////// moves  //////

	// keyboard moves
	if(keyboard->isKeyDown(OIS::KC_A) || keyboard->isKeyDown(OIS::KC_LEFT))
		translateVector.x = -MOVE_SCALE;	// Move camera left

	if(keyboard->isKeyDown(OIS::KC_D) || keyboard->isKeyDown(OIS::KC_RIGHT))
		translateVector.x = +MOVE_SCALE;	// Move camera RIGHT

	if(keyboard->isKeyDown(OIS::KC_UP) || keyboard->isKeyDown(OIS::KC_W) )
		translateVector.z = -MOVE_SCALE;	// Move camera forward

	if(keyboard->isKeyDown(OIS::KC_DOWN) || keyboard->isKeyDown(OIS::KC_S) )
		translateVector.z = +MOVE_SCALE;	// Move camera backward

	if(keyboard->isKeyDown(OIS::KC_PGUP))
		translateVector.y = +MOVE_SCALE;	// Move camera up

	if(keyboard->isKeyDown(OIS::KC_PGDOWN))
		translateVector.y = -MOVE_SCALE;	// Move camera down

	/*if(keyboard->isKeyDown(OIS::KC_RIGHT))
		rotX -= ROT_SCALE;					// Turn camera right

	if(keyboard->isKeyDown(OIS::KC_LEFT))
		rotX += ROT_SCALE;					// Turn camera left*/

	rotX *= deltaTime;
	rotY *= deltaTime;
	translateVector *= deltaTime;

	// mouse moves
	const OIS::MouseState &ms = mouse->getMouseState();
	if (ms.buttonDown(OIS::MB_Right))
	{
		translateVector.x += ms.X.rel * 0.005f * MOVE_SCALE;	// Move camera horizontaly
		translateVector.y -= ms.Y.rel * 0.005f * MOVE_SCALE;	// Move camera verticaly
	}
	else
	{
		rotX += Degree(-ms.X.rel * 0.0025f * ROT_SCALE);		// Rotate camera horizontaly
		rotY += Degree(-ms.Y.rel * 0.0025f * ROT_SCALE);		// Rotate camera verticaly
	}

	mCameraNode->translate(mCameraNode->getOrientation()*mCameraNode2->getOrientation()*translateVector);
	if (mIsCameraActivated)
	{
		mCameraNode->yaw(rotX);
		mCameraNode2->pitch(rotY);
		//mCameraNode->pitch(rotY);		
	}

	if (keyboard->isKeyDown(OIS::KC_ADD) && mTimeUntilNextToggle <= 0)
	{
		nextCamera();
		mTimeUntilNextToggle = 0.2f;
	}
	else if (keyboard->isKeyDown(OIS::KC_SUBTRACT) && mTimeUntilNextToggle <= 0)
	{
		previousCamera();
		mTimeUntilNextToggle = 0.2f;
	}
	else if (keyboard->isKeyDown(OIS::KC_O) && mTimeUntilNextToggle <= 0)
	{
		increaseBillboardSize();
		mTimeUntilNextToggle = 0.1f;
	}
	else if (keyboard->isKeyDown(OIS::KC_P) && mTimeUntilNextToggle <= 0)
	{
		decreaseBillboardSize();
		mTimeUntilNextToggle = 0.1f;
	}

	else if (keyboard->isKeyDown(OIS::KC_H) && mTimeUntilNextToggle <= 0)
	{
		increaseFovSize();
		mTimeUntilNextToggle = 0.1f;
	}
	else if (keyboard->isKeyDown(OIS::KC_J) && mTimeUntilNextToggle <= 0)
	{
		decreaseFovSize();
		mTimeUntilNextToggle = 0.1f;
	}
	else if (keyboard->isKeyDown(OIS::KC_RETURN) && mTimeUntilNextToggle <= 0)
	{
		toggleSparseDensePointCloud();
		mTimeUntilNextToggle = 0.2f;
	}
	else if (keyboard->isKeyDown(OIS::KC_C) && mTimeUntilNextToggle <= 0)
	{
		toggleCameraPlaneVisibility();
		mTimeUntilNextToggle = 0.2f;
	}
	else if (keyboard->isKeyDown(OIS::KC_SPACE) && mTimeUntilNextToggle <= 0)
	{
		toggleCameraMode();
		mTimeUntilNextToggle = 0.2f;
	}

	if (mTimeUntilNextToggle >= 0)
		mTimeUntilNextToggle -= deltaTime;

	return true;
}

bool OgreAppLogic::OISListener::mouseMoved(const OIS::MouseEvent &arg)
{
	if (arg.state.Z.abs > 0)
	{
		mParent->increaseFovSize();
	}
	else if (arg.state.Z.abs < 0)
	{
		mParent->decreaseFovSize();
	}
	return true;
}

bool OgreAppLogic::OISListener::mousePressed(const OIS::MouseEvent &arg, OIS::MouseButtonID id)
{
	
	if (id == 0)
	{
		float x = arg.state.X.abs / (float) arg.state.width;
		float y = arg.state.Y.abs / (float) arg.state.height;
		mParent->onMouseClick(x, y);
	}
	return true;
}

bool OgreAppLogic::OISListener::mouseReleased(const OIS::MouseEvent &arg, OIS::MouseButtonID id)
{
	return true;
}

bool OgreAppLogic::OISListener::keyPressed(const OIS::KeyEvent &arg)
{
	return true;
}

bool OgreAppLogic::OISListener::keyReleased(const OIS::KeyEvent &arg)
{
	return true;
}

void OgreAppLogic::nextCamera()
{
	mCameraIndex++;
	if (mCameraIndex >= (int)mPhotoSynthParser->getNbCamera(0))
		mCameraIndex = 0;

	setCamera(mCameraIndex);
}

void OgreAppLogic::previousCamera()
{
	mCameraIndex--;
	if (mCameraIndex < 0)
		mCameraIndex = mPhotoSynthParser->getNbCamera(0)-1;

	setCamera(mCameraIndex);
}

void OgreAppLogic::setCamera(unsigned int index)
{
	const PhotoSynth::Camera& cam = mPhotoSynthParser->getCamera(0, index);

	Ogre::Matrix3 mx;
	mx.FromAxisAngle(Ogre::Vector3::UNIT_X, Ogre::Degree(180));

	Ogre::Matrix3 mx2;
	mx2.FromAxisAngle(Ogre::Vector3::UNIT_X, Ogre::Degree(-90));

	Ogre::Matrix3 rot;
	cam.orientation.ToRotationMatrix(rot);
	rot = mx2 * rot * mx;

	//old method with roll
	//mCameraNode->setOrientation(rot);

	//new method without roll
	Ogre::Radian yaw, pitch, roll;
	rot.ToEulerAnglesYXZ(yaw, pitch, roll);

	Ogre::Matrix3 yawMatrix, pitchMatrix, rollMatrix;	
	yawMatrix.FromAxisAngle(Ogre::Vector3::UNIT_Y, yaw);
	pitchMatrix.FromAxisAngle(Ogre::Vector3::UNIT_X, pitch);
	//rollMatrix.FromAxisAngle(Ogre::Vector3::UNIT_Z, roll);

	rot = yawMatrix * pitchMatrix;// * rollMatrix;

	mCameraNode->setPosition(mx2*cam.position);
	mCameraNode->setOrientation(yawMatrix);
	mCameraNode2->setOrientation(pitchMatrix);
}

void OgreAppLogic::increaseBillboardSize()
{
	mBillboardSize += mBillboardInterval;

	setBillboardSize(mBillboardSize);
}

void OgreAppLogic::decreaseBillboardSize()
{
	mBillboardSize -= mBillboardInterval;
	if (mBillboardSize < 0)
		mBillboardSize = mBillboardInterval;

	setBillboardSize(mBillboardSize);
}

void OgreAppLogic::setBillboardSize(Ogre::Real size)
{
	mSparsePointCloud->setBillboardSize(size*Ogre::Vector2::UNIT_SCALE);
	if (mDensePointCloud)
		mDensePointCloud->setBillboardSize(size*Ogre::Vector2::UNIT_SCALE);
}

void OgreAppLogic::increaseFovSize()
{
	mFovSize += mFovInterval;
	if (mFovSize > 180.0)
		mFovSize = 180.0;

	setFovSize(mBillboardSize);
}

void OgreAppLogic::decreaseFovSize()
{
	mFovSize -= mFovInterval;
	if (mFovSize < 0.0)
		mFovSize = mFovInterval;

	setFovSize(mFovSize);
}

void OgreAppLogic::setFovSize(Ogre::Real size)
{
	mCamera->setFOVy(Ogre::Degree(mFovSize));
}

void OgreAppLogic::toggleSparseDensePointCloud()
{
	mIsSparsePointCloudVisible = !mIsSparsePointCloudVisible;
	if (mDensePointCloud)
	{		
		if (mIsSparsePointCloudVisible)
		{
			mSparsePointCloud->setVisible(true);
			mDensePointCloud->setVisible(false);
		}
		else
		{
			mSparsePointCloud->setVisible(false);
			mDensePointCloud->setVisible(true);
		}
	}
	else
	{
		mSparsePointCloud->setVisible(mIsSparsePointCloudVisible);
	}
}

void OgreAppLogic::toggleCameraPlaneVisibility()
{
	mIsCameraPlaneVisible = !mIsCameraPlaneVisible;
}

void OgreAppLogic::toggleCameraMode()
{
	mIsCameraActivated = !mIsCameraActivated;
}

void OgreAppLogic::parsePictureList(const std::string& picListFilePath)
{
	std::ifstream input(picListFilePath.c_str());
	if (input.is_open())
	{
		while(!input.eof())
		{
			std::string line;
			std::getline(input, line);
			if (line != "")
			{
				std::string filename, path;
				Ogre::StringUtil::splitFilename(line, filename, path);
				mPictureFilenames.push_back(filename);
			}
		}
	}
	input.close();
}

void OgreAppLogic::createAllCameraPlane()
{
	Ogre::Matrix3 mx;
	mx.FromAxisAngle(Ogre::Vector3::UNIT_X, Ogre::Degree(-90));

	Ogre::SceneNode* root = mSceneMgr->getRootSceneNode()->createChildSceneNode("cameraPlanes");
	root->setOrientation(mx);

	for (unsigned int i=0; i<mPhotoSynthParser->getNbCamera(0); ++i)
		createCameraPlane(i);
}

void OgreAppLogic::createCameraPlane(unsigned int index)
{
	Ogre::SceneNode* root = mSceneMgr->getSceneNode("cameraPlanes");
	std::stringstream nodeName;
	nodeName << "cameraPlane" << index;
	
	// Create parent node
	Ogre::SceneNode* currentNode = root->createChildSceneNode(nodeName.str());

	// Get camera position and orientation
	const PhotoSynth::Camera& cam = mPhotoSynthParser->getCamera(0, index);

	Ogre::Matrix3 mx;
	mx.FromAxisAngle(Ogre::Vector3::UNIT_X, Ogre::Degree(180));

	Ogre::Matrix3 rot;
	cam.orientation.ToRotationMatrix(rot);
	rot = rot * mx;

	// Update parent node with camera pose
	currentNode->setPosition(cam.position);
	currentNode->setOrientation(rot);

	// Create material
	std::string textureFilename;
	{
		char buf[10];
		sprintf(buf, "%08d", cam.index);

		std::stringstream tmp;
		tmp << buf << ".jpg";
		textureFilename = tmp.str();
	}

	//load image
	Ogre::TexturePtr tex = Ogre::TextureManager::getSingletonPtr()->load(textureFilename, Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);

	// Adapt plane to camera info
	float width       = (float) tex->getWidth();
	float height      = (float) tex->getHeight();
	float focalLength = cam.focal*std::max(width, height);

	Ogre::MaterialPtr material = Ogre::MaterialManager::getSingleton().create(nodeName.str(), Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
	material->getTechnique(0)->getPass(0)->setLightingEnabled(false);
	//material->getTechnique(0)->getPass(0)->setDepthWriteEnabled(true);
	material->getTechnique(0)->getPass(0)->setDepthWriteEnabled(false);
	material->getTechnique(0)->getPass(0)->setCullingMode(Ogre::CULL_NONE);
	Ogre::TextureUnitState* texUnit = material->getTechnique(0)->getPass(0)->createTextureUnitState(tex->getName());	
	texUnit->setTextureAddressingMode(Ogre::TextureUnitState::TAM_CLAMP);
	material->getTechnique(0)->getPass(0)->setVertexProgram("RadialUndistort_vp");
	material->getTechnique(0)->getPass(0)->setFragmentProgram("RadialUndistort_fp");
	material->getTechnique(0)->getPass(0)->getFragmentProgramParameters()->setNamedConstant("distort1", (float) cam.distort1);
	material->getTechnique(0)->getPass(0)->getFragmentProgramParameters()->setNamedConstant("distort2", (float) cam.distort2);
	material->getTechnique(0)->getPass(0)->getFragmentProgramParameters()->setNamedConstant("focal",    (float) focalLength);

	// Create plane
	Plane p(Vector3::UNIT_Z, 0.0);
	MeshManager::getSingleton().createPlane("VerticalPlane", ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME, p , 1, 1, 1, 1, true, 1, 1, 1, Vector3::UNIT_Y);
	Entity* planeEntity = mSceneMgr->createEntity(nodeName.str(), "VerticalPlane"); 
	planeEntity->setMaterialName(nodeName.str());
	planeEntity->setRenderQueueGroup(RENDER_QUEUE_2);

	// Create a node for the plane, inserts it in the scene
	Ogre::SceneNode* planeNode = currentNode->createChildSceneNode();
	planeNode->attachObject(planeEntity);

	// Update position    
	float distanceFromCamera = 0.2f;
	planeNode->setPosition(0, 0, -distanceFromCamera);

	//compute fov from image size and camera focal
	Ogre::Radian fovy = 2.0f * Ogre::Math::ATan(height / (2.0f*focalLength));

	//update plane dimension with texture dimension
	float videoAspectRatio = width / height;
	float planeHeight = 2 * distanceFromCamera * Ogre::Math::Tan(fovy*0.5);
	float planeWidth  = planeHeight * videoAspectRatio;
	planeNode->setScale(planeWidth, planeHeight, 1.0f);

	DynamicLines* lines = new DynamicLines(Ogre::RenderOperation::OT_LINE_LIST);
	lines->setMaterial("BlackDynamicLines");

	lines->addPoint(Ogre::Vector3::ZERO);
	lines->addPoint(Ogre::Vector3(-planeWidth/2, planeHeight/2, -distanceFromCamera));
	lines->addPoint(Ogre::Vector3::ZERO);
	lines->addPoint(Ogre::Vector3(planeWidth/2, planeHeight/2, -distanceFromCamera));
	lines->addPoint(Ogre::Vector3::ZERO);
	lines->addPoint(Ogre::Vector3(-planeWidth/2, -planeHeight/2, -distanceFromCamera));
	lines->addPoint(Ogre::Vector3::ZERO);
	lines->addPoint(Ogre::Vector3(planeWidth/2, -planeHeight/2, -distanceFromCamera));

	lines->addPoint(Ogre::Vector3(-planeWidth/2,  planeHeight/2, -distanceFromCamera));
	lines->addPoint(Ogre::Vector3( planeWidth/2,  planeHeight/2, -distanceFromCamera));
	lines->addPoint(Ogre::Vector3( planeWidth/2,  planeHeight/2, -distanceFromCamera));
	lines->addPoint(Ogre::Vector3( planeWidth/2, -planeHeight/2, -distanceFromCamera));
	lines->addPoint(Ogre::Vector3( planeWidth/2, -planeHeight/2, -distanceFromCamera));
	lines->addPoint(Ogre::Vector3(-planeWidth/2, -planeHeight/2, -distanceFromCamera));
	lines->addPoint(Ogre::Vector3(-planeWidth/2, -planeHeight/2, -distanceFromCamera));
	lines->addPoint(Ogre::Vector3(-planeWidth/2,  planeHeight/2, -distanceFromCamera));

	lines->update();
	currentNode->attachObject(lines);
}

void OgreAppLogic::prepareTriangleIntersection()
{
	Ogre::Matrix3 mx;
	mx.FromAxisAngle(Ogre::Vector3::UNIT_X, Ogre::Degree(180));

	float distanceFromCamera = 0.2f;

	DynamicLines* lines = new DynamicLines(Ogre::RenderOperation::OT_LINE_LIST);
	lines->setMaterial("RedDynamicLines");
	for (unsigned int i=0; i<mPhotoSynthParser->getNbCamera(0); ++i)
	{
		const PhotoSynth::Camera& cam = mPhotoSynthParser->getCamera(0, i);
		const PhotoSynth::Thumb& thumb = mPhotoSynthParser->getJsonInfo().thumbs[cam.index];
		float width       = (float) thumb.width;
		float height      = (float) thumb.height;
		float focalLength = cam.focal*std::max(width, height);		
		float videoAspectRatio = width / height;
		
		Ogre::Radian fovy = 2.0f * Ogre::Math::ATan(height / (2.0f*focalLength));
		float planeHeight = 2 * distanceFromCamera * Ogre::Math::Tan(fovy*0.5);
		float planeWidth  = planeHeight * videoAspectRatio;

		Ogre::Matrix3 rot;
		cam.orientation.ToRotationMatrix(rot);
		rot = rot * mx;

		Ogre::Matrix3 mx2;
		mx2.FromAxisAngle(Ogre::Vector3::UNIT_X, Ogre::Degree(-90));

		lines->addPoint(mx2*cam.position+mx2*rot*Ogre::Vector3(-planeWidth/2,  planeHeight/2, -distanceFromCamera)); //C
		lines->addPoint(mx2*cam.position+mx2*rot*Ogre::Vector3( planeWidth/2,  planeHeight/2, -distanceFromCamera)); //D
		lines->addPoint(mx2*cam.position+mx2*rot*Ogre::Vector3( planeWidth/2,  planeHeight/2, -distanceFromCamera)); //D
		lines->addPoint(mx2*cam.position+mx2*rot*Ogre::Vector3( planeWidth/2, -planeHeight/2, -distanceFromCamera)); //E
		lines->addPoint(mx2*cam.position+mx2*rot*Ogre::Vector3( planeWidth/2, -planeHeight/2, -distanceFromCamera)); //E
		lines->addPoint(mx2*cam.position+mx2*rot*Ogre::Vector3(-planeWidth/2, -planeHeight/2, -distanceFromCamera)); //F
		lines->addPoint(mx2*cam.position+mx2*rot*Ogre::Vector3(-planeWidth/2, -planeHeight/2, -distanceFromCamera)); //F
		lines->addPoint(mx2*cam.position+mx2*rot*Ogre::Vector3(-planeWidth/2,  planeHeight/2, -distanceFromCamera)); //C

		Ogre::Vector3 C(mx2*cam.position+mx2*rot*Ogre::Vector3(-planeWidth/2,  planeHeight/2, -distanceFromCamera));
		Ogre::Vector3 D(mx2*cam.position+mx2*rot*Ogre::Vector3( planeWidth/2,  planeHeight/2, -distanceFromCamera));
		Ogre::Vector3 E(mx2*cam.position+mx2*rot*Ogre::Vector3( planeWidth/2, -planeHeight/2, -distanceFromCamera));
		Ogre::Vector3 F(mx2*cam.position+mx2*rot*Ogre::Vector3(-planeWidth/2, -planeHeight/2, -distanceFromCamera));		

		mCameraPlanes.push_back(CameraPlane(C, D, E, F));
	}
	lines->update();
	//mSceneMgr->getRootSceneNode()->attachObject(lines);
}

void OgreAppLogic::onMouseClick(float x, float y)
{
	//Create camera ray using 2d mouse click coord
	Ogre::Ray ray;
	mCamera->getCameraToViewportRay(x, y, &ray);

	//Compute ray/triangle with all pictures (2 triangles per picture)
	unsigned int nbPlanes = mPhotoSynthParser->getNbCamera(0);
	std::vector<std::pair<bool, float>> results(nbPlanes);
	for (unsigned int i=0; i<mPhotoSynthParser->getNbCamera(0); ++i)
	{
		const CameraPlane& plane = mCameraPlanes[i];
		std::pair<bool, float> resultCDE = Ogre::Math::intersects(ray, plane.c, plane.d, plane.e, true, true);
		std::pair<bool, float> resultCEF = Ogre::Math::intersects(ray, plane.c, plane.e, plane.f, true, true);
		if (resultCDE.first)
			results[i] = resultCDE;
		else if (resultCEF.first)
			results[i] = resultCEF;
		else
			results[i].first = false;
	}

	//Find the closest picture intersecting with the camera ray
	float distance = FLT_MAX;
	int index      = -1;
	for (unsigned int i=0; i<results.size(); ++i)
	{
		if (results[i].first && results[i].second < distance)
		{
			distance = results[i].second;
			index = i;
		}
	}

	//If a picture is found apply the camera pose
	if (index != -1)
	{
		setCamera(index);
		mCameraIndex = index;
	}
}