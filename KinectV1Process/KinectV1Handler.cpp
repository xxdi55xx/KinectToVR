#include "stdafx.h"
#include "KinectV1Handler.h"
#include <boost/exception/diagnostic_information.hpp> 
#include <boost/exception_ptr.hpp> 
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/transform2.hpp>
#include <glm/mat4x4.hpp>
#include <glm/detail/type_vec3.hpp>
#include <glm/detail/type_vec4.hpp>
#include <glm/detail/type_vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp> 
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include "glew.h"
#include <SFML/Graphics/CircleShape.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <KinectSettings.h>
#include <VRHelper.h>
#include <sfLine.h>
#include <iostream>
#include <KinectJoint.h>
#include <Eigen/Geometry>
#include <math.h>
#include "VectorMath.h"

 void KinectV1Handler::initOpenGL() {
    LOG(INFO) << "Attempted to initialise OpenGL";
    int width = 0, height = 0;
    if (kVersion == KinectVersion::Version1) {
        width = KinectSettings::kinectWidth;
        height = KinectSettings::kinectHeight;
    }
    else if (kVersion == KinectVersion::Version2) {
        width = KinectSettings::kinectV2Width;
        height = KinectSettings::kinectV2Height;
    }   // REMOVE THIS INTO KINECT V2 IMPL
        // Initialize textures
    glGenTextures(1, &kinectTextureId);
    glBindTexture(GL_TEXTURE_2D, kinectTextureId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height,
        0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, (GLvoid*)kinectImageData.get());
    glBindTexture(GL_TEXTURE_2D, 0);

    // OpenGL setup
    glClearColor(1, 0, 0, 0);
    glClearDepth(1.0f);
    glEnable(GL_TEXTURE_2D);

    // Camera setup
    glViewport(0, 0, SFMLsettings::m_window_width, SFMLsettings::m_window_height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, SFMLsettings::m_window_width, SFMLsettings::m_window_height, 0, 1, -1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}


 HRESULT KinectV1Handler::getStatusResult()
 {
     if (kinectSensor)
         return kinectSensor->NuiStatus();
     else
         return E_NUI_NOTCONNECTED;
 }

 std::string KinectV1Handler::statusResultString(HRESULT stat) {
    switch (stat) {
    case S_OK: return "S_OK";
    case S_NUI_INITIALIZING:	return "S_NUI_INITIALIZING The device is connected, but still initializing.";
    case E_NUI_NOTCONNECTED:	return "E_NUI_NOTCONNECTED The device is not connected.";
    case E_NUI_NOTGENUINE:	return "E_NUI_NOTGENUINE The device is not a valid Kinect.";
    case E_NUI_NOTSUPPORTED:	return "E_NUI_NOTSUPPORTED The device is an unsupported model.";
    case E_NUI_INSUFFICIENTBANDWIDTH:	return "E_NUI_INSUFFICIENTBANDWIDTH The device is connected to a hub without the necessary bandwidth requirements.";
    case E_NUI_NOTPOWERED:	return "E_NUI_NOTPOWERED The device is connected, but unpowered.";
    case E_NUI_NOTREADY:	return "E_NUI_NOTREADY There was some other unspecified error.";
    default: return "Uh Oh undefined kinect error! " + std::to_string(stat);
    }
}

 void KinectV1Handler::initialise() {
    try {
        kVersion = KinectVersion::Version1;
        kinectImageData
            = std::make_unique<GLubyte[]>(KinectSettings::kinectWidth * KinectSettings::kinectHeight * 4);  // BGRA
        initialised = initKinect();
        LOG_IF(initialised, INFO) << "Kinect initialised successfully!";
        if (!initialised) throw FailedKinectInitialisation;
    }
    catch (std::exception&  e) {
        LOG(ERROR) << "Failed to initialise Kinect " << e.what() << std::endl;
    }
}

 void KinectV1Handler::update() {
    if (isInitialised()) {
        HRESULT kinectStatus = kinectSensor->NuiStatus();
        if (kinectStatus == S_OK) {
            getKinectRGBData();
            updateSkeletalData();
        }
    }
}

 void KinectV1Handler::drawKinectData(sf::RenderWindow &drawingWindow) {
    if (isInitialised()) {
        if (KinectSettings::isKinectDrawn) {
            drawKinectImageData(drawingWindow);
        }
        if (KinectSettings::isSkeletonDrawn) {
            drawTrackedSkeletons(drawingWindow);
        }
    }
};
 void KinectV1Handler::drawKinectImageData(sf::RenderWindow &drawingWindow) {

    glBindTexture(GL_TEXTURE_2D, kinectTextureId);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, SFMLsettings::m_window_width, SFMLsettings::m_window_height, GL_BGRA_EXT, GL_UNSIGNED_BYTE, (GLvoid*)kinectImageData.get());
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(0, 0, 0);
    glTexCoord2f(1.0f, 0.0f);
    glVertex3f(SFMLsettings::m_window_width, 0, 0);
    glTexCoord2f(1.0f, 1.0f);
    glVertex3f(SFMLsettings::m_window_width, SFMLsettings::m_window_height, 0.0f);
    glTexCoord2f(0.0f, 1.0f);
    glVertex3f(0, SFMLsettings::m_window_height, 0.0f);

    glEnd();
};
 void KinectV1Handler::drawTrackedSkeletons(sf::RenderWindow &drawingWindow) {
    for (int i = 0; i < NUI_SKELETON_POSITION_COUNT; ++i) {
        screenSkelePoints[i] = sf::Vector2f(0.0f, 0.0f);
    }
    for (int i = 0; i < NUI_SKELETON_COUNT; ++i) {
        NUI_SKELETON_TRACKING_STATE trackingState = skeletonFrame.SkeletonData[i].eTrackingState;

        if (NUI_SKELETON_TRACKED == trackingState)
        {
            if (KinectSettings::isSkeletonDrawn) {
                drawingWindow.pushGLStates();
                drawingWindow.resetGLStates();

                DrawSkeleton(skeletonFrame.SkeletonData[i], drawingWindow);

                drawingWindow.popGLStates();
            }

        }
        else if (NUI_SKELETON_POSITION_ONLY == trackingState) {
            //ONLY CENTER POINT TO DRAW
            if (KinectSettings::isSkeletonDrawn) {
                sf::CircleShape circle(KinectSettings::g_JointThickness, 30);
                circle.setRadius(KinectSettings::g_JointThickness);
                circle.setPosition(SkeletonToScreen(skeletonFrame.SkeletonData[i].Position, SFMLsettings::m_window_width, SFMLsettings::m_window_height));
                circle.setFillColor(sf::Color::Yellow);

                drawingWindow.pushGLStates();
                drawingWindow.resetGLStates();

                drawingWindow.draw(circle);

                drawingWindow.popGLStates();
            }
        }
    }
};

//Consider moving this tracking stuff into a seperate class
 void KinectV1Handler::zeroAllTracking(vr::IVRSystem* &m_sys) { // Holdover from previous implementation
    for (int i = 0; i < NUI_SKELETON_COUNT; ++i) {
        NUI_SKELETON_TRACKING_STATE trackingState = skeletonFrame.SkeletonData[i].eTrackingState;

        if (NUI_SKELETON_TRACKED == trackingState)
        {
            //KinectSettings::hmdZero = getHMDPosition(m_sys);

            setKinectToVRMultiplier(i);
            zeroed = true;
            break;

        }
    }
}

 void KinectV1Handler::updateTrackersWithSkeletonPosition(
    std::vector<KVR::KinectTrackedDevice> & trackers)
{
    for (KVR::KinectTrackedDevice & device : trackers) {
        if (device.isSensor()) {
            device.update(KinectSettings::kinectRepPosition, { 0,0,0 }, KinectSettings::kinectRepRotation);
        }
        else {
            vr::HmdVector3d_t jointPosition{ 0,0,0 };
            vr::HmdQuaternion_t jointRotation{ 0,0,0,0 };
            if (getFilteredJoint(device, jointPosition, jointRotation)) {
                

                device.update(trackedPositionVROffset, jointPosition, jointRotation);
            } 
        }
    }
}
 

bool KinectV1Handler::getFilteredJoint(KVR::KinectTrackedDevice device, vr::HmdVector3d_t& position, vr::HmdQuaternion_t &rotation) {
    for (int i = 0; i < NUI_SKELETON_COUNT; ++i) {
        NUI_SKELETON_TRACKING_STATE trackingState = skeletonFrame.SkeletonData[i].eTrackingState;

        if (trackingState == NUI_SKELETON_TRACKED)
        {
            // If we can't find either of these joints, exit
            if (jointsUntracked(device.joint0, device.joint1, skeletonFrame.SkeletonData[i]))
            {
                return false;
            }

            // Don't track if both points are inferred
            bool ignoreInferredJoints = true;
            if (ignoreInferredJoints && jointsInferred(device.joint0, device.joint1, skeletonFrame.SkeletonData[i]))
            {
                return false;
            }
            
            {
                float jointX = jointPositions[convertJoint(device.joint0)].x;
                float jointY = jointPositions[convertJoint(device.joint0)].y;
                float jointZ = jointPositions[convertJoint(device.joint0)].z;
                position = vr::HmdVector3d_t{ jointX,jointY,jointZ };

                //Rotation - Need to seperate into function
                Vector4 kRotation = { 0,0,0,1 };
                switch (device.rotationFilterOption) {
                case KVR::JointRotationFilterOption::Unfiltered:
                    kRotation = boneOrientations[convertJoint(device.joint0)].absoluteRotation.rotationQuaternion;
                    break;
                case KVR::JointRotationFilterOption::Filtered:
                    kRotation = rotFilter.GetFilteredJoints()[convertJoint(device.joint0)];
                    break;
                case KVR::JointRotationFilterOption::HeadLook: {        // Ew
                    auto q = KinectSettings::hmdRotation;
                    //Isolate Yaw
                    float yaw = atan2(2 * q.w*q.y + 2 * q.x*q.z, +q.w*q.w + q.x*q.x - q.z*q.z - q.y*q.y);

                    auto kq = vrmath::quaternionFromRotationY(yaw);
                    kRotation.w = kq.w;
                    kRotation.x = kq.x;
                    kRotation.y = kq.y;
                    kRotation.z = kq.z;
                }
                                                         break;
                default:
                    LOG(ERROR) << "JOINT ROTATION OPTION UNDEFINED IN DEVICE " << device.deviceId;
                    break;
                }
                rotation.w = kRotation.w;
                rotation.x = kRotation.x;
                rotation.y = kRotation.y;
                rotation.z = kRotation.z;


                return true;
            }
        }
    }
    return false;
}
NUI_SKELETON_POSITION_INDEX KinectV1Handler::convertJoint(KVR::KinectJoint joint)
{
    using namespace KVR;
    //Unfortunately I believe this is required because there are mismatches between v1 and v2 joint IDs
    //Might consider investigating to see if there's a way to shorten this
    switch (joint.joint) {
    case KinectJointType::SpineBase:
        return NUI_SKELETON_POSITION_HIP_CENTER;
    case KinectJointType::SpineMid:
        return NUI_SKELETON_POSITION_SPINE;

    case KinectJointType::Head:
        return NUI_SKELETON_POSITION_HEAD;
    case KinectJointType::ShoulderLeft:
        return NUI_SKELETON_POSITION_SHOULDER_LEFT;
    case KinectJointType::ShoulderRight:
        return NUI_SKELETON_POSITION_SHOULDER_RIGHT;
    case KinectJointType::SpineShoulder:
        return NUI_SKELETON_POSITION_SHOULDER_CENTER;

    case KinectJointType::ElbowLeft:
        return NUI_SKELETON_POSITION_ELBOW_LEFT;
    case KinectJointType::WristLeft:
        return NUI_SKELETON_POSITION_WRIST_LEFT;
    case KinectJointType::HandLeft:
        return NUI_SKELETON_POSITION_HAND_LEFT;

    case KinectJointType::ElbowRight:
        return NUI_SKELETON_POSITION_ELBOW_RIGHT;
    case KinectJointType::WristRight:
        return NUI_SKELETON_POSITION_WRIST_RIGHT;
    case KinectJointType::HandRight:
        return NUI_SKELETON_POSITION_HAND_RIGHT;

    case KinectJointType::HipLeft:
        return NUI_SKELETON_POSITION_HIP_LEFT;
    case KinectJointType::HipRight:
        return NUI_SKELETON_POSITION_HIP_RIGHT;

    case KinectJointType::KneeLeft:
        return NUI_SKELETON_POSITION_KNEE_LEFT;
    case KinectJointType::KneeRight:
        return NUI_SKELETON_POSITION_KNEE_RIGHT;

    case KinectJointType::AnkleLeft:
        return NUI_SKELETON_POSITION_ANKLE_LEFT;
    case KinectJointType::AnkleRight:
        return NUI_SKELETON_POSITION_ANKLE_RIGHT;

    case KinectJointType::FootLeft:
        return NUI_SKELETON_POSITION_FOOT_LEFT;
    case KinectJointType::FootRight:
        return NUI_SKELETON_POSITION_FOOT_RIGHT;

        /*BELOW DO NOT HAVE A 1:1 V1 REPRESENTATION*/
        //refer to the skeleton images from Microsoft for diffs between v1 and 2

    case KinectJointType::Neck:
        return NUI_SKELETON_POSITION_SHOULDER_CENTER;
    case KinectJointType::HandTipLeft:
        return NUI_SKELETON_POSITION_HAND_LEFT;
    case KinectJointType::HandTipRight:
        return NUI_SKELETON_POSITION_HAND_RIGHT;
    case KinectJointType::ThumbLeft:
        return NUI_SKELETON_POSITION_HAND_LEFT;
    case KinectJointType::ThumbRight:
        return NUI_SKELETON_POSITION_HAND_RIGHT;

    default:
        LOG(ERROR) << "INVALID KinectJointType!!!";
        return NUI_SKELETON_POSITION_WRIST_LEFT;
        break;

    }
}
bool KinectV1Handler::initKinect() {
    //Get a working Kinect Sensor
    int numSensors = 0;
    if (NuiGetSensorCount(&numSensors) < 0 || numSensors < 1) {
        LOG(ERROR) << "No Kinect Sensors found!";
        return false;
    }
    if (NuiCreateSensorByIndex(0, &kinectSensor) < 0) {
        LOG(ERROR) << "Sensor found, but could not create an instance of it!";
        return false;
    }
    //Initialise Sensor
    HRESULT hr = kinectSensor->NuiInitialize(NUI_INITIALIZE_FLAG_USES_DEPTH_AND_PLAYER_INDEX
        | NUI_INITIALIZE_FLAG_USES_SKELETON);
    LOG_IF(FAILED(hr), ERROR) << "Kinect sensor failed to initialise!";
    else LOG(INFO) << "Kinect sensor opened successfully.";
    /*
    kinectSensor->NuiImageStreamOpen(
        NUI_IMAGE_TYPE_COLOR,               //Depth Camera or RGB Camera?
        NUI_IMAGE_RESOLUTION_640x480,       //Image Resolution
        0,                                  //Image stream flags, e.g. near mode
        2,                                  //Number of frames to buffer
        NULL,                               //Event handle
        &kinectRGBStream);
    
    */
    kinectSensor->NuiImageStreamOpen(
        NUI_IMAGE_TYPE_DEPTH_AND_PLAYER_INDEX,               //Depth Camera or RGB Camera?
        NUI_IMAGE_RESOLUTION_320x240,       //Image Resolution
        0,                                  //Image stream flags, e.g. near mode
        2,                                  //Number of frames to buffer
        NULL,                               //Event handle
        &kinectDepthStream);
    kinectSensor->NuiSkeletonTrackingEnable(
        NULL,
        NUI_SKELETON_TRACKING_FLAG_ENABLE_IN_NEAR_RANGE
    );
    
    return kinectSensor;
}
void KinectV1Handler::getKinectRGBData() {
    NUI_IMAGE_FRAME imageFrame{};
    NUI_LOCKED_RECT LockedRect{};
    if (acquireKinectFrame(imageFrame, kinectRGBStream, kinectSensor)) {
        return;
    }
    INuiFrameTexture* texture = lockKinectPixelData(imageFrame, LockedRect);
    copyKinectPixelData(LockedRect, kinectImageData.get());
    unlockKinectPixelData(texture);

    releaseKinectFrame(imageFrame, kinectRGBStream, kinectSensor);
}
    bool KinectV1Handler::acquireKinectFrame(NUI_IMAGE_FRAME &imageFrame, HANDLE & rgbStream, INuiSensor* &sensor)
    {
        return (sensor->NuiImageStreamGetNextFrame(rgbStream, 1, &imageFrame) < 0);
    }
    INuiFrameTexture* KinectV1Handler::lockKinectPixelData(NUI_IMAGE_FRAME &imageFrame, NUI_LOCKED_RECT &LockedRect)
    {
        INuiFrameTexture* texture = imageFrame.pFrameTexture;
        texture->LockRect(0, &LockedRect, NULL, 0);
        return imageFrame.pFrameTexture;
    }
    void KinectV1Handler::copyKinectPixelData(NUI_LOCKED_RECT &LockedRect, GLubyte* dest)
    {
        int bytesInFrameRow = LockedRect.Pitch;
        if (bytesInFrameRow != 0) {
            const BYTE* curr = (const BYTE*)LockedRect.pBits;
            const BYTE* dataEnd = curr + (KinectSettings::kinectWidth*KinectSettings::kinectHeight) * 4;

            while (curr < dataEnd) {
                *dest++ = *curr++;
            }
        }
    }
    void KinectV1Handler::unlockKinectPixelData(INuiFrameTexture* texture)
    {
        texture->UnlockRect(0);
    }
    void KinectV1Handler::releaseKinectFrame(NUI_IMAGE_FRAME &imageFrame, HANDLE& rgbStream, INuiSensor* &sensor)
    {
        sensor->NuiImageStreamReleaseFrame(rgbStream, &imageFrame);
    }

    static bool flip = false;
    void KinectV1Handler::updateSkeletalData() {
        if (kinectSensor->NuiSkeletonGetNextFrame(0, &skeletonFrame) >= 0) {
            NUI_TRANSFORM_SMOOTH_PARAMETERS params;
            
            params.fCorrection = .25f;
            params.fJitterRadius = .4f;
            params.fMaxDeviationRadius = .25f;
            params.fPrediction = .25f;
            params.fSmoothing = .25f;
            
            /*
            params.fSmoothing = .25f;
            params.fCorrection = .25f;
            params.fMaxDeviationRadius = .05f;
            params.fJitterRadius = 0.03f;
            params.fPrediction = .25f;
            */
            kinectSensor->NuiTransformSmooth(&skeletonFrame, &params);   //Smooths jittery tracking
            NUI_SKELETON_DATA data;

            for (int i = 0; i < NUI_SKELETON_COUNT; ++i) {
                NUI_SKELETON_TRACKING_STATE trackingState = skeletonFrame.SkeletonData[i].eTrackingState;
                data = skeletonFrame.SkeletonData[i];

                if (NUI_SKELETON_TRACKED == trackingState)
                {
                    for (int j = 0; j < NUI_SKELETON_POSITION_COUNT; ++j) {
                        jointPositions[j] = skeletonFrame.SkeletonData[i].SkeletonPositions[j];
                    }
                    NuiSkeletonCalculateBoneOrientations(&skeletonFrame.SkeletonData[i], boneOrientations);
                    rotFilter.update(boneOrientations);
                    break;
                }
            }
            
            KinectSettings::mposes[1].v[0] = jointPositions[convertJoint(KVR::KinectJointType::HandLeft)].x;
            KinectSettings::mposes[1].v[1] = jointPositions[convertJoint(KVR::KinectJointType::HandLeft)].y;
            KinectSettings::mposes[1].v[2] = jointPositions[convertJoint(KVR::KinectJointType::HandLeft)].z;
            
            KinectSettings::mposes[0].v[0] = jointPositions[convertJoint(KVR::KinectJointType::Head)].x;
            KinectSettings::mposes[0].v[1] = jointPositions[convertJoint(KVR::KinectJointType::Head)].y;
            KinectSettings::mposes[0].v[2] = jointPositions[convertJoint(KVR::KinectJointType::Head)].z;
            
            vr::HmdVector3d_t trotation[3] = { {0,0,0},{0,0,0},{0,0,0} };

#pragma region PipeSeup

            HANDLE pipeTracker = CreateFile(TEXT("\\\\.\\pipe\\LogPipeTracker"), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
            HANDLE pipeSan = CreateFile(TEXT("\\\\.\\pipe\\LogPipeSan"), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
            HANDLE pipeLeft = CreateFile(TEXT("\\\\.\\pipe\\LogPipeNi"), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
            HANDLE pipeRight = CreateFile(TEXT("\\\\.\\pipe\\LogPipeIchi"), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
            HANDLE pipeLeftRot = CreateFile(TEXT("\\\\.\\pipe\\LogPipeNiRot"), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
            HANDLE pipeRightRot = CreateFile(TEXT("\\\\.\\pipe\\LogPipeIchiRot"), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);

#pragma endregion

            DWORD numWritten;

#pragma region TrackerIPC

			float yaw = KinectSettings::hmdYaw * 180 / M_PI;
			float facing = yaw - KinectSettings::tryaw;

			if (facing < 25 && facing > -25)flip = false;
			if (facing < -155 && facing > -205)flip = true;

            std::string TrackerS = [&]()->std::string {
                std::stringstream S;

                using PointSet = Eigen::Matrix<float, 3, Eigen::Dynamic>;

#pragma region Rotation_Hips

                glm::quat hipsrot =
                    glm::quat(
                        boneOrientations[convertJoint(KVR::KinectJointType::SpineBase)].absoluteRotation.rotationQuaternion.w,
                        boneOrientations[convertJoint(KVR::KinectJointType::AnkleLeft)].absoluteRotation.rotationQuaternion.x,
                        boneOrientations[convertJoint(KVR::KinectJointType::AnkleLeft)].absoluteRotation.rotationQuaternion.y,
                        boneOrientations[convertJoint(KVR::KinectJointType::AnkleLeft)].absoluteRotation.rotationQuaternion.z);

                trotation[2] = vr::HmdVector3d_t{ /*double(glm::eulerAngles(hipsrot).x * 180 / M_PI) + 180.f*/ 0.f,
                    double(glm::eulerAngles(hipsrot).y * 180 / M_PI),
                    double(glm::eulerAngles(hipsrot).z * 180 / M_PI) };

#pragma endregion
#pragma region Rotation_Ankles

				glm::vec3 ankle[2] = { glm::vec3(jointPositions[convertJoint(KVR::KinectJointType::AnkleLeft)].x, jointPositions[convertJoint(KVR::KinectJointType::AnkleLeft)].y, jointPositions[convertJoint(KVR::KinectJointType::AnkleLeft)].z),
				    glm::vec3(jointPositions[convertJoint(KVR::KinectJointType::AnkleRight)].x, jointPositions[convertJoint(KVR::KinectJointType::AnkleRight)].y, jointPositions[convertJoint(KVR::KinectJointType::AnkleRight)].z) };

                glm::vec3 foot[2] = { glm::vec3(jointPositions[convertJoint(KVR::KinectJointType::FootLeft)].x, jointPositions[convertJoint(KVR::KinectJointType::FootLeft)].y, jointPositions[convertJoint(KVR::KinectJointType::FootLeft)].z),
                    glm::vec3(jointPositions[convertJoint(KVR::KinectJointType::FootRight)].x, jointPositions[convertJoint(KVR::KinectJointType::FootRight)].y, jointPositions[convertJoint(KVR::KinectJointType::FootRight)].z) };

                glm::quat ankleRot[2] = { glm::lookAt(ankle[0], foot[0], glm::vec3(0.0f, 1.0f, 0.0f)), glm::lookAt(ankle[1], foot[1], glm::vec3(0.0f, 1.0f, 0.0f)) };
                glm::vec3 ankleRotRad[2] = { glm::eulerAngles(ankleRot[0]), glm::eulerAngles(ankleRot[1]) };

				
				glm::quat footrot[2] = { 
                    glm::quat(
					    boneOrientations[convertJoint(KVR::KinectJointType::AnkleLeft)].absoluteRotation.rotationQuaternion.w,
					    boneOrientations[convertJoint(KVR::KinectJointType::AnkleLeft)].absoluteRotation.rotationQuaternion.x,
					    boneOrientations[convertJoint(KVR::KinectJointType::AnkleLeft)].absoluteRotation.rotationQuaternion.y,
					    boneOrientations[convertJoint(KVR::KinectJointType::AnkleLeft)].absoluteRotation.rotationQuaternion.z),
					glm::quat(
						boneOrientations[convertJoint(KVR::KinectJointType::AnkleRight)].absoluteRotation.rotationQuaternion.w,
					    boneOrientations[convertJoint(KVR::KinectJointType::AnkleRight)].absoluteRotation.rotationQuaternion.x,
					    boneOrientations[convertJoint(KVR::KinectJointType::AnkleRight)].absoluteRotation.rotationQuaternion.y,
					    boneOrientations[convertJoint(KVR::KinectJointType::AnkleRight)].absoluteRotation.rotationQuaternion.z) };

                if (!flip) {
                    trotation[0] = vr::HmdVector3d_t{ double(glm::eulerAngles(footrot[0]).x * 180 / M_PI) + 180.f,
                        -double(ankleRotRad[0].y * 180.f / M_PI),
                        double(glm::eulerAngles(footrot[0]).z * 180 / M_PI) };
                    trotation[1] = vr::HmdVector3d_t{ double(glm::eulerAngles(footrot[1]).x * 180 / M_PI) + 180.f,
                        -double(ankleRotRad[1].y * 180.f / M_PI),
                        double(glm::eulerAngles(footrot[1]).z * 180 / M_PI) };
                }
                else {
                    trotation[1] = vr::HmdVector3d_t{ -double(glm::eulerAngles(footrot[0]).x * 180 / M_PI) + 180.f,
                        double(ankleRotRad[0].y * 180.f / M_PI),
                        -double(glm::eulerAngles(footrot[0]).z * 180 / M_PI) };
                    trotation[0] = vr::HmdVector3d_t{ -double(glm::eulerAngles(footrot[1]).x * 180 / M_PI) + 180.f,
                        double(ankleRotRad[1].y * 180.f / M_PI),
                        -double(glm::eulerAngles(footrot[1]).z * 180 / M_PI) };
                }

#pragma endregion

                if (KinectSettings::rtcalibrated) {
                    Eigen::Vector3f Hf, Mf, Hp;
                    if (!flip) {
                        Hf(0) = jointPositions[convertJoint(KVR::KinectJointType::AnkleLeft)].x;
                        Hf(1) = jointPositions[convertJoint(KVR::KinectJointType::AnkleLeft)].y;
                        Hf(2) = jointPositions[convertJoint(KVR::KinectJointType::AnkleLeft)].z;

                        Mf(0) = jointPositions[convertJoint(KVR::KinectJointType::AnkleRight)].x;
                        Mf(1) = jointPositions[convertJoint(KVR::KinectJointType::AnkleRight)].y;
                        Mf(2) = jointPositions[convertJoint(KVR::KinectJointType::AnkleRight)].z;
                    }
                    else {
                        trotation[0].v[1] += 180;
                        trotation[1].v[1] += 180;
                        trotation[2].v[1] += 180;

                        Mf(0) = jointPositions[convertJoint(KVR::KinectJointType::AnkleLeft)].x;
                        Mf(1) = jointPositions[convertJoint(KVR::KinectJointType::AnkleLeft)].y;
                        Mf(2) = jointPositions[convertJoint(KVR::KinectJointType::AnkleLeft)].z;

                        Hf(0) = jointPositions[convertJoint(KVR::KinectJointType::AnkleRight)].x;
                        Hf(1) = jointPositions[convertJoint(KVR::KinectJointType::AnkleRight)].y;
                        Hf(2) = jointPositions[convertJoint(KVR::KinectJointType::AnkleRight)].z;
                    }

                    Hp(0) = jointPositions[convertJoint(KVR::KinectJointType::SpineBase)].x;
                    Hp(1) = jointPositions[convertJoint(KVR::KinectJointType::SpineBase)].y;
                    Hp(2) = jointPositions[convertJoint(KVR::KinectJointType::SpineBase)].z;

                    PointSet Hf2 = (KinectSettings::R_matT * Hf).colwise() + KinectSettings::T_matT;
                    PointSet Mf2 = (KinectSettings::R_matT * Mf).colwise() + KinectSettings::T_matT;
                    PointSet Hp2 = (KinectSettings::R_matT * Hp).colwise() + KinectSettings::T_matT;

                    S << "HX" << 10000 * (Hf2(0) + KinectSettings::moffsets[0][1].v[0] + KinectSettings::troffsets.v[0]) <<
                        "/HY" << 10000 * (Hf2(1) + KinectSettings::moffsets[0][1].v[1] + KinectSettings::troffsets.v[1]) <<
                        "/HZ" << 10000 * (Hf2(2) + KinectSettings::moffsets[0][1].v[2] + KinectSettings::troffsets.v[2]) <<
                        "/MX" << 10000 * (Mf2(0) + KinectSettings::moffsets[0][0].v[0] + KinectSettings::troffsets.v[0]) <<
                        "/MY" << 10000 * (Mf2(1) + KinectSettings::moffsets[0][0].v[1] + KinectSettings::troffsets.v[1]) <<
                        "/MZ" << 10000 * (Mf2(2) + KinectSettings::moffsets[0][0].v[2] + KinectSettings::troffsets.v[2]) <<
                        "/PX" << 10000 * (Hp2(0) + KinectSettings::moffsets[0][2].v[0] + KinectSettings::troffsets.v[0]) <<
                        "/PY" << 10000 * (Hp2(1) + KinectSettings::moffsets[0][2].v[1] + KinectSettings::troffsets.v[1]) <<
                        "/PZ" << 10000 * (Hp2(2) + KinectSettings::moffsets[0][2].v[2] + KinectSettings::troffsets.v[2]) <<
                        "/HRX" << 10000 * (trotation[0].v[0] + KinectSettings::moffsets[1][1].v[0]) * M_PI / 180 <<
                        "/HRY" << 10000 * (trotation[0].v[1] + KinectSettings::tryaw + KinectSettings::moffsets[1][1].v[1]) * M_PI / 180 <<
                        "/HRZ" << 10000 * (trotation[0].v[2] + KinectSettings::moffsets[1][1].v[2]) * M_PI / 180 <<
                        "/MRX" << 10000 * (trotation[1].v[0] + KinectSettings::moffsets[1][0].v[0]) * M_PI / 180 <<
                        "/MRY" << 10000 * (trotation[1].v[1] + KinectSettings::tryaw + KinectSettings::moffsets[1][0].v[1]) * M_PI / 180 <<
                        "/MRZ" << 10000 * (trotation[1].v[2] + KinectSettings::moffsets[1][0].v[2]) * M_PI / 180 <<
                        "/PRX" << 10000 * (trotation[2].v[0] + KinectSettings::moffsets[1][2].v[0]) * M_PI / 180 <<
                        "/PRY" << 10000 * (trotation[2].v[1] + KinectSettings::tryaw + KinectSettings::moffsets[1][2].v[1]) * M_PI / 180 <<
                        "/PRZ" << 10000 * (trotation[2].v[2] + KinectSettings::moffsets[1][2].v[2]) * M_PI / 180 <<
                        "/WRW" << 10000 * (0) << "/"; //DEPRECATED: GLM_ROTATE SCREWED UP WITH > 99
                }
                else {
                    if (!flip) {
                        S << "HX" << 10000 * (jointPositions[convertJoint(KVR::KinectJointType::AnkleLeft)].x + KinectSettings::moffsets[0][1].v[0] + KinectSettings::troffsets.v[0]) <<
                            "/HY" << 10000 * (jointPositions[convertJoint(KVR::KinectJointType::AnkleLeft)].y + KinectSettings::moffsets[0][1].v[1] + KinectSettings::troffsets.v[1]) <<
                            "/HZ" << 10000 * (jointPositions[convertJoint(KVR::KinectJointType::AnkleLeft)].z + KinectSettings::moffsets[0][1].v[2] + KinectSettings::troffsets.v[2]) <<
                            "/MX" << 10000 * (jointPositions[convertJoint(KVR::KinectJointType::AnkleRight)].x + KinectSettings::moffsets[0][0].v[0] + KinectSettings::troffsets.v[0]) <<
                            "/MY" << 10000 * (jointPositions[convertJoint(KVR::KinectJointType::AnkleRight)].y + KinectSettings::moffsets[0][0].v[1] + KinectSettings::troffsets.v[1]) <<
                            "/MZ" << 10000 * (jointPositions[convertJoint(KVR::KinectJointType::AnkleRight)].z + KinectSettings::moffsets[0][0].v[2] + KinectSettings::troffsets.v[2]) <<
                            "/PX" << 10000 * (jointPositions[convertJoint(KVR::KinectJointType::SpineBase)].x + KinectSettings::moffsets[0][2].v[0] + KinectSettings::troffsets.v[0]) <<
                            "/PY" << 10000 * (jointPositions[convertJoint(KVR::KinectJointType::SpineBase)].y + KinectSettings::moffsets[0][2].v[1] + KinectSettings::troffsets.v[1]) <<
                            "/PZ" << 10000 * (jointPositions[convertJoint(KVR::KinectJointType::SpineBase)].z + KinectSettings::moffsets[0][2].v[2] + KinectSettings::troffsets.v[2]) <<
                            "/HRX" << 10000 * (trotation[0].v[0] + KinectSettings::moffsets[1][1].v[0]) * M_PI / 180 <<
                            "/HRY" << 10000 * (trotation[0].v[1] + KinectSettings::moffsets[1][1].v[1]) * M_PI / 180 <<
                            "/HRZ" << 10000 * (trotation[0].v[2] + KinectSettings::moffsets[1][1].v[2]) * M_PI / 180 <<
                            "/MRX" << 10000 * (trotation[1].v[0] + KinectSettings::moffsets[1][0].v[0]) * M_PI / 180 <<
                            "/MRY" << 10000 * (trotation[1].v[1] + KinectSettings::moffsets[1][0].v[1]) * M_PI / 180 <<
                            "/MRZ" << 10000 * (trotation[1].v[2] + KinectSettings::moffsets[1][0].v[2]) * M_PI / 180 <<
                            "/PRX" << 10000 * (trotation[2].v[0] + KinectSettings::moffsets[1][2].v[0]) * M_PI / 180 <<
                            "/PRY" << 10000 * (trotation[2].v[1] + KinectSettings::moffsets[1][2].v[1]) * M_PI / 180 <<
                            "/PRZ" << 10000 * (trotation[2].v[2] + KinectSettings::moffsets[1][2].v[2]) * M_PI / 180 <<
                            "/WRW" << 10000 * (0) << "/"; //DEPRECATED: GLM_ROTATE SCREWED UP WITH > 99
                    }
                    else {
                        trotation[0].v[1] += 180;
                        trotation[1].v[1] += 180;
                        trotation[2].v[1] += 180;

                        S << "HX" << 10000 * (jointPositions[convertJoint(KVR::KinectJointType::AnkleRight)].x + KinectSettings::moffsets[0][1].v[0] + KinectSettings::troffsets.v[0]) <<
                            "/HY" << 10000 * (jointPositions[convertJoint(KVR::KinectJointType::AnkleRight)].y + KinectSettings::moffsets[0][1].v[1] + KinectSettings::troffsets.v[1]) <<
                            "/HZ" << 10000 * (jointPositions[convertJoint(KVR::KinectJointType::AnkleRight)].z + KinectSettings::moffsets[0][1].v[2] + KinectSettings::troffsets.v[2]) <<
                            "/MX" << 10000 * (jointPositions[convertJoint(KVR::KinectJointType::AnkleLeft)].x + KinectSettings::moffsets[0][0].v[0] + KinectSettings::troffsets.v[0]) <<
                            "/MY" << 10000 * (jointPositions[convertJoint(KVR::KinectJointType::AnkleLeft)].y + KinectSettings::moffsets[0][0].v[1] + KinectSettings::troffsets.v[1]) <<
                            "/MZ" << 10000 * (jointPositions[convertJoint(KVR::KinectJointType::AnkleLeft)].z + KinectSettings::moffsets[0][0].v[2] + KinectSettings::troffsets.v[2]) <<
                            "/PX" << 10000 * (jointPositions[convertJoint(KVR::KinectJointType::SpineBase)].x + KinectSettings::moffsets[0][2].v[0] + KinectSettings::troffsets.v[0]) <<
                            "/PY" << 10000 * (jointPositions[convertJoint(KVR::KinectJointType::SpineBase)].y + KinectSettings::moffsets[0][2].v[1] + KinectSettings::troffsets.v[1]) <<
                            "/PZ" << 10000 * (jointPositions[convertJoint(KVR::KinectJointType::SpineBase)].z + KinectSettings::moffsets[0][2].v[2] + KinectSettings::troffsets.v[2]) <<
                            "/HRX" << 10000 * (trotation[0].v[0] + KinectSettings::moffsets[1][1].v[0]) * M_PI / 180 <<
                            "/HRY" << 10000 * (trotation[0].v[1] + KinectSettings::moffsets[1][1].v[1]) * M_PI / 180 <<
                            "/HRZ" << 10000 * (trotation[0].v[2] + KinectSettings::moffsets[1][1].v[2]) * M_PI / 180 <<
                            "/MRX" << 10000 * (trotation[1].v[0] + KinectSettings::moffsets[1][0].v[0]) * M_PI / 180 <<
                            "/MRY" << 10000 * (trotation[1].v[1] + KinectSettings::moffsets[1][0].v[1]) * M_PI / 180 <<
                            "/MRZ" << 10000 * (trotation[1].v[2] + KinectSettings::moffsets[1][0].v[2]) * M_PI / 180 <<
                            "/PRX" << 10000 * (trotation[2].v[0] + KinectSettings::moffsets[1][2].v[0]) * M_PI / 180 <<
                            "/PRY" << 10000 * (trotation[2].v[1] + KinectSettings::moffsets[1][2].v[1]) * M_PI / 180 <<
                            "/PRZ" << 10000 * (trotation[2].v[2] + KinectSettings::moffsets[1][2].v[2]) * M_PI / 180 <<
                            "/WRW" << 10000 * (0) << "/"; //DEPRECATED: GLM_ROTATE SCREWED UP WITH > 99
                    }
                }

                return S.str();
            }();

#pragma endregion

			std::string HedoS = [&]()->std::string {
				std::stringstream S;

                Eigen::AngleAxisd rollAngle(0.f, Eigen::Vector3d::UnitZ());
                Eigen::AngleAxisd yawAngle(KinectSettings::hroffset * M_PI / 180, Eigen::Vector3d::UnitY());
                Eigen::AngleAxisd pitchAngle(0.f, Eigen::Vector3d::UnitX());

                Eigen::Quaternion<double> q = rollAngle * yawAngle * pitchAngle;

                Eigen::Vector3d in(jointPositions[convertJoint(KVR::KinectJointType::Head)].x,
                    jointPositions[convertJoint(KVR::KinectJointType::Head)].y,
                    jointPositions[convertJoint(KVR::KinectJointType::Head)].z);

                Eigen::Vector3d out = q * in;


				S << "X" << 10000 * (out(0) + KinectSettings::hoffsets.v[0]) <<
					"/Y" << 10000 * (out(1) + KinectSettings::hoffsets.v[1]) <<
					"/Z" << 10000 * (out(2) + KinectSettings::hoffsets.v[2]) << "/";

				return S.str();
			}();

			std::string RightS = [&]()->std::string {
				std::stringstream S;

				if (KinectSettings::rtconcalib) {
					using PointSet = Eigen::Matrix<float, 3, Eigen::Dynamic>;

					Eigen::Vector3f Hf, Ef;
					if (!flip) {
						Hf(0) = jointPositions[convertJoint(KVR::KinectJointType::HandRight)].x;
						Hf(1) = jointPositions[convertJoint(KVR::KinectJointType::HandRight)].y;
						Hf(2) = jointPositions[convertJoint(KVR::KinectJointType::HandRight)].z;
						Ef(0) = jointPositions[convertJoint(KVR::KinectJointType::ElbowRight)].x;
						Ef(1) = jointPositions[convertJoint(KVR::KinectJointType::ElbowRight)].y;
						Ef(2) = jointPositions[convertJoint(KVR::KinectJointType::ElbowRight)].z;
					}
					else {
						trotation[0].v[1] = 180;
						trotation[1].v[1] = 180;
						trotation[2].v[1] = 180;

						Hf(0) = jointPositions[convertJoint(KVR::KinectJointType::HandLeft)].x;
						Hf(1) = jointPositions[convertJoint(KVR::KinectJointType::HandLeft)].y;
						Hf(2) = jointPositions[convertJoint(KVR::KinectJointType::HandLeft)].z;
						Ef(0) = jointPositions[convertJoint(KVR::KinectJointType::ElbowLeft)].x;
						Ef(1) = jointPositions[convertJoint(KVR::KinectJointType::ElbowLeft)].y;
						Ef(2) = jointPositions[convertJoint(KVR::KinectJointType::ElbowLeft)].z;
					}

					PointSet Hf2 = (KinectSettings::R_matT * Hf).colwise() + KinectSettings::T_matT;
					PointSet Mf2 = (KinectSettings::R_matT * Ef).colwise() + KinectSettings::T_matT;

					S << "X" << 10000 * (Hf(0) + KinectSettings::hoffsets.v[0] + KinectSettings::mauoffset.v[0]) <<
						"/Y" << 10000 * (Hf(1) + KinectSettings::hoffsets.v[1] + KinectSettings::mauoffset.v[1]) <<
						"/Z" << 10000 * (Hf(2) + KinectSettings::hoffsets.v[2] + KinectSettings::mauoffset.v[2]) <<
						"/EX" << 10000 * (Ef(0) + KinectSettings::hoffsets.v[0] + KinectSettings::mauoffset.v[0]) <<
						"/EY" << 10000 * (Ef(1) + KinectSettings::hoffsets.v[1] + KinectSettings::mauoffset.v[1]) <<
						"/EZ" << 10000 * (Ef(2) + KinectSettings::hoffsets.v[2] + KinectSettings::mauoffset.v[2]) << "/";
				}
                else {
                    if (!flip) {
                        S << "X" << 10000 * (jointPositions[convertJoint(KVR::KinectJointType::HandRight)].x + KinectSettings::hoffsets.v[0] + KinectSettings::mauoffset.v[0]) <<
                            "/Y" << 10000 * (jointPositions[convertJoint(KVR::KinectJointType::HandRight)].y + KinectSettings::hoffsets.v[1] + KinectSettings::mauoffset.v[1]) <<
                            "/Z" << 10000 * (jointPositions[convertJoint(KVR::KinectJointType::HandRight)].z + KinectSettings::hoffsets.v[2] + KinectSettings::mauoffset.v[2]) <<
                            "/EX" << 10000 * (jointPositions[convertJoint(KVR::KinectJointType::ElbowRight)].x + KinectSettings::hoffsets.v[0] + KinectSettings::mauoffset.v[0]) <<
                            "/EY" << 10000 * (jointPositions[convertJoint(KVR::KinectJointType::ElbowRight)].y + KinectSettings::hoffsets.v[1] + KinectSettings::mauoffset.v[1]) <<
                            "/EZ" << 10000 * (jointPositions[convertJoint(KVR::KinectJointType::ElbowRight)].z + KinectSettings::hoffsets.v[2] + KinectSettings::mauoffset.v[2]) << "/";
                    }
                    else {
                        S << "X" << 10000 * (jointPositions[convertJoint(KVR::KinectJointType::HandLeft)].x + KinectSettings::hoffsets.v[0] + KinectSettings::mauoffset.v[0]) <<
                            "/Y" << 10000 * (jointPositions[convertJoint(KVR::KinectJointType::HandLeft)].y + KinectSettings::hoffsets.v[1] + KinectSettings::mauoffset.v[1]) <<
                            "/Z" << 10000 * (jointPositions[convertJoint(KVR::KinectJointType::HandLeft)].z + KinectSettings::hoffsets.v[2] + KinectSettings::mauoffset.v[2]) <<
                            "/EX" << 10000 * (jointPositions[convertJoint(KVR::KinectJointType::ElbowLeft)].x + KinectSettings::hoffsets.v[0] + KinectSettings::mauoffset.v[0]) <<
                            "/EY" << 10000 * (jointPositions[convertJoint(KVR::KinectJointType::ElbowLeft)].y + KinectSettings::hoffsets.v[1] + KinectSettings::mauoffset.v[1]) <<
                            "/EZ" << 10000 * (jointPositions[convertJoint(KVR::KinectJointType::ElbowLeft)].z + KinectSettings::hoffsets.v[2] + KinectSettings::mauoffset.v[2]) << "/";
                    }
                }

				return S.str();
			}();

            std::string LeftS = [&]()->std::string {
                std::stringstream S;

                if (KinectSettings::rtconcalib) {
                    using PointSet = Eigen::Matrix<float, 3, Eigen::Dynamic>;

                    Eigen::Vector3f Hf, Ef;
                    if (flip) {
                        Hf(0) = jointPositions[convertJoint(KVR::KinectJointType::HandRight)].x;
                        Hf(1) = jointPositions[convertJoint(KVR::KinectJointType::HandRight)].y;
                        Hf(2) = jointPositions[convertJoint(KVR::KinectJointType::HandRight)].z;
                        Ef(0) = jointPositions[convertJoint(KVR::KinectJointType::ElbowRight)].x;
                        Ef(1) = jointPositions[convertJoint(KVR::KinectJointType::ElbowRight)].y;
                        Ef(2) = jointPositions[convertJoint(KVR::KinectJointType::ElbowRight)].z;
                    }
                    else {
                        trotation[0].v[1] = 180;
                        trotation[1].v[1] = 180;
                        trotation[2].v[1] = 180;

                        Hf(0) = jointPositions[convertJoint(KVR::KinectJointType::HandLeft)].x;
                        Hf(1) = jointPositions[convertJoint(KVR::KinectJointType::HandLeft)].y;
                        Hf(2) = jointPositions[convertJoint(KVR::KinectJointType::HandLeft)].z;
                        Ef(0) = jointPositions[convertJoint(KVR::KinectJointType::ElbowLeft)].x;
                        Ef(1) = jointPositions[convertJoint(KVR::KinectJointType::ElbowLeft)].y;
                        Ef(2) = jointPositions[convertJoint(KVR::KinectJointType::ElbowLeft)].z;
                    }

                    PointSet Hf2 = (KinectSettings::R_matT * Hf).colwise() + KinectSettings::T_matT;
                    PointSet Mf2 = (KinectSettings::R_matT * Ef).colwise() + KinectSettings::T_matT;

                    S << "X" << 10000 * (Hf(0) + KinectSettings::hoffsets.v[0] + KinectSettings::hauoffset.v[0]) <<
                        "/Y" << 10000 * (Hf(1) + KinectSettings::hoffsets.v[1] + KinectSettings::hauoffset.v[1]) <<
                        "/Z" << 10000 * (Hf(2) + KinectSettings::hoffsets.v[2] + KinectSettings::hauoffset.v[2]) <<
                        "/EX" << 10000 * (Ef(0) + KinectSettings::hoffsets.v[0] + KinectSettings::hauoffset.v[0]) <<
                        "/EY" << 10000 * (Ef(1) + KinectSettings::hoffsets.v[1] + KinectSettings::hauoffset.v[1]) <<
                        "/EZ" << 10000 * (Ef(2) + KinectSettings::hoffsets.v[2] + KinectSettings::hauoffset.v[2]) << "/";
                }
                else {
                    if (flip) {
                        /*Eigen::AngleAxisd rollAngle(0.f, Eigen::Vector3d::UnitZ());
                        Eigen::AngleAxisd yawAngle(KinectSettings::hroffset * M_PI / 180, Eigen::Vector3d::UnitY());
                        Eigen::AngleAxisd pitchAngle(0.f, Eigen::Vector3d::UnitX());
                        Eigen::Quaternion<double> q = rollAngle * yawAngle * pitchAngle;
                        Eigen::Vector3d hin(jointPositions[convertJoint(KVR::KinectJointType::HandRight)].x,
                            jointPositions[convertJoint(KVR::KinectJointType::HandRight)].y,
                            jointPositions[convertJoint(KVR::KinectJointType::HandRight)].z);

                        Eigen::Vector3d hout = q * hin;

                        Eigen::Vector3d ein(jointPositions[convertJoint(KVR::KinectJointType::ElbowRight)].x,
                            jointPositions[convertJoint(KVR::KinectJointType::ElbowRight)].y,
                            jointPositions[convertJoint(KVR::KinectJointType::ElbowRight)].z);

                        Eigen::Vector3d eout = q * ein;*/

                        S << "X" << 10000 * (jointPositions[convertJoint(KVR::KinectJointType::HandRight)].x + KinectSettings::hoffsets.v[0] + KinectSettings::hauoffset.v[0]) <<
                            "/Y" << 10000 * (jointPositions[convertJoint(KVR::KinectJointType::HandRight)].y + KinectSettings::hoffsets.v[1] + KinectSettings::hauoffset.v[1]) <<
                            "/Z" << 10000 * (jointPositions[convertJoint(KVR::KinectJointType::HandRight)].z + KinectSettings::hoffsets.v[2] + KinectSettings::hauoffset.v[2]) <<
                            "/EX" << 10000 * (jointPositions[convertJoint(KVR::KinectJointType::ElbowRight)].x + KinectSettings::hoffsets.v[0] + KinectSettings::hauoffset.v[0]) <<
                            "/EY" << 10000 * (jointPositions[convertJoint(KVR::KinectJointType::ElbowRight)].y + KinectSettings::hoffsets.v[1] + KinectSettings::hauoffset.v[1]) <<
                            "/EZ" << 10000 * (jointPositions[convertJoint(KVR::KinectJointType::ElbowRight)].z + KinectSettings::hoffsets.v[2] + KinectSettings::hauoffset.v[2]) << "/";
                    }
                    else {
                        /*Eigen::AngleAxisd rollAngle(0.f, Eigen::Vector3d::UnitZ());
                        Eigen::AngleAxisd yawAngle(KinectSettings::hroffset* M_PI / 180, Eigen::Vector3d::UnitY());
                        Eigen::AngleAxisd pitchAngle(0.f, Eigen::Vector3d::UnitX());
                        Eigen::Quaternion<double> q = rollAngle * yawAngle * pitchAngle;
                        Eigen::Vector3d hin(jointPositions[convertJoint(KVR::KinectJointType::HandLeft)].x,
                            jointPositions[convertJoint(KVR::KinectJointType::HandLeft)].y,
                            jointPositions[convertJoint(KVR::KinectJointType::HandLeft)].z);

                        Eigen::Vector3d hout = q * hin;

                        Eigen::Vector3d ein(jointPositions[convertJoint(KVR::KinectJointType::ElbowLeft)].x,
                            jointPositions[convertJoint(KVR::KinectJointType::ElbowLeft)].y,
                            jointPositions[convertJoint(KVR::KinectJointType::ElbowLeft)].z);

                        Eigen::Vector3d eout = q * ein;*/

                        S << "X" << 10000 * (jointPositions[convertJoint(KVR::KinectJointType::HandLeft)].x + KinectSettings::hoffsets.v[0] + KinectSettings::hauoffset.v[0]) <<
                            "/Y" << 10000 * (jointPositions[convertJoint(KVR::KinectJointType::HandLeft)].y + KinectSettings::hoffsets.v[1] + KinectSettings::hauoffset.v[1]) <<
                            "/Z" << 10000 * (jointPositions[convertJoint(KVR::KinectJointType::HandLeft)].z + KinectSettings::hoffsets.v[2] + KinectSettings::hauoffset.v[2]) <<
                            "/EX" << 10000 * (jointPositions[convertJoint(KVR::KinectJointType::ElbowLeft)].x + KinectSettings::hoffsets.v[0] + KinectSettings::hauoffset.v[0]) <<
                            "/EY" << 10000 * (jointPositions[convertJoint(KVR::KinectJointType::ElbowLeft)].y + KinectSettings::hoffsets.v[1] + KinectSettings::hauoffset.v[1]) <<
                            "/EZ" << 10000 * (jointPositions[convertJoint(KVR::KinectJointType::ElbowLeft)].z + KinectSettings::hoffsets.v[2] + KinectSettings::hauoffset.v[2]) << "/";
                    }
                }

                return S.str();
            }();

            std::string RotS = [&]()->std::string {
                std::stringstream S;

                if (KinectSettings::rtconcalib) {

                    Eigen::Vector3f Hf, Ef;
                    if (flip) {
                        S << "X" << 0.f <<
                            "/Y" << 10000 * KinectSettings::tryaw * 180 / M_PI <<
                            "/Z" << 10000 * 0.f << "/";
                    }
                    else {
                        S << "X" << 0.f <<
                            "/Y" << 10000 * ((KinectSettings::tryaw * 180 / M_PI) + 180.f) <<
                            "/Z" << 0.f << "/";
                    }
                }
                else {
                    if (flip) {
                        S << "X" << 0.f <<
                            "/Y" << 0.f <<
                            "/Z" << 0.f << "/";
                    }
                    else {
                        S << "X" << 0.f <<
                            "/Y" << 180.f <<
                            "/Z" << 0.f << "/";
                    }
                }

                return S.str();
            }();

            char TrackerD[1024];
            char HedoD[1024];
            char RightD[1024];
            char LeftD[1024];
            char RotD[1024];

            strcpy_s(TrackerD, TrackerS.c_str());
            strcpy_s(HedoD, HedoS.c_str());
            strcpy_s(RightD, RightS.c_str());
            strcpy_s(LeftD, LeftS.c_str());
            strcpy_s(RotD, RotS.c_str());

            WriteFile(pipeTracker, TrackerD, sizeof(TrackerD), &numWritten, NULL);
            WriteFile(pipeSan, HedoD, sizeof(HedoD), &numWritten, NULL);
            WriteFile(pipeRight, RightD, sizeof(RightD), &numWritten, NULL);
            WriteFile(pipeLeft, LeftD, sizeof(LeftD), &numWritten, NULL);
            WriteFile(pipeLeftRot, RotD, sizeof(RotD), &numWritten, NULL);
            WriteFile(pipeRightRot, RotD, sizeof(RotD), &numWritten, NULL);

            CloseHandle(pipeTracker);
            CloseHandle(pipeSan);
            CloseHandle(pipeRight);
            CloseHandle(pipeLeft);
            CloseHandle(pipeLeftRot);
            CloseHandle(pipeRightRot);

            //std::cout << 
            
            //DEBUG
            /*
            for (int i = 0; i < 20; i++) {
                Vector4 orientation = boneOrientations[i].absoluteRotation.rotationQuaternion;
                std::cerr << "Joint " << i << ": " << orientation.w << ", " << orientation.x << ", " << orientation.y << ", " << orientation.z << '\n';
            }*/
            //Vector4 orientation = boneOrientations[convertJoint(KinectJointType::AnkleLeft)].absoluteRotation.rotationQuaternion;
            //using namespace SFMLsettings;

            //debugDisplayTextStream << "AnkleLeftRot " << ": " << orientation.w << ", " << orientation.x << ", " << orientation.y << ", " << orientation.z << '\n';
            //orientation = boneOrientations[convertJoint(KinectJointType::FootLeft)].absoluteRotation.rotationQuaternion;
            //debugDisplayTextStream << "FootLeftRot " << ": " << orientation.w << ", " << orientation.x << ", " << orientation.y << ", " << orientation.z << '\n';
            //orientation = boneOrientations[convertJoint(KinectJointType::AnkleRight)].absoluteRotation.rotationQuaternion;
            //debugDisplayTextStream << "AnkleRightRot " << ": " << orientation.w << ", " << orientation.x << ", " << orientation.y << ", " << orientation.z << '\n';
            //orientation = boneOrientations[convertJoint(KinectJointType::FootRight)].absoluteRotation.rotationQuaternion;
            //debugDisplayTextStream << "FootLeftRot " << ": " << orientation.w << ", " << orientation.x << ", " << orientation.y << ", " << orientation.z << '\n';
        }
        
        return;
    };
    void KinectV1Handler::DrawSkeleton(const NUI_SKELETON_DATA & skel, sf::RenderWindow &window) {
        for (int i = 0; i < NUI_SKELETON_POSITION_COUNT; ++i) {
            screenSkelePoints[i] = SkeletonToScreen(jointPositions[i], SFMLsettings::m_window_width, SFMLsettings::m_window_height);
            //std::cerr << "m_points[" << i << "] = " << screenSkelePoints[i].x << ", " << screenSkelePoints[i].y << '\n';
            // Same with the other cerr, without this, the skeleton flickers
        }
        // Render Torso
        DrawBone(skel, NUI_SKELETON_POSITION_HEAD, NUI_SKELETON_POSITION_SHOULDER_CENTER, window);
        DrawBone(skel, NUI_SKELETON_POSITION_SHOULDER_CENTER, NUI_SKELETON_POSITION_SHOULDER_LEFT, window);
        DrawBone(skel, NUI_SKELETON_POSITION_SHOULDER_CENTER, NUI_SKELETON_POSITION_SHOULDER_RIGHT, window);
        DrawBone(skel, NUI_SKELETON_POSITION_SHOULDER_CENTER, NUI_SKELETON_POSITION_SPINE, window);
        DrawBone(skel, NUI_SKELETON_POSITION_SPINE, NUI_SKELETON_POSITION_HIP_CENTER, window);
        DrawBone(skel, NUI_SKELETON_POSITION_HIP_CENTER, NUI_SKELETON_POSITION_HIP_LEFT, window);
        DrawBone(skel, NUI_SKELETON_POSITION_HIP_CENTER, NUI_SKELETON_POSITION_HIP_RIGHT, window);

        // Left Arm
        DrawBone(skel, NUI_SKELETON_POSITION_SHOULDER_LEFT, NUI_SKELETON_POSITION_ELBOW_LEFT, window);
        DrawBone(skel, NUI_SKELETON_POSITION_ELBOW_LEFT, NUI_SKELETON_POSITION_WRIST_LEFT, window);
        DrawBone(skel, NUI_SKELETON_POSITION_WRIST_LEFT, NUI_SKELETON_POSITION_HAND_LEFT, window);

        // Right Arm
        DrawBone(skel, NUI_SKELETON_POSITION_SHOULDER_RIGHT, NUI_SKELETON_POSITION_ELBOW_RIGHT, window);
        DrawBone(skel, NUI_SKELETON_POSITION_ELBOW_RIGHT, NUI_SKELETON_POSITION_WRIST_RIGHT, window);
        DrawBone(skel, NUI_SKELETON_POSITION_WRIST_RIGHT, NUI_SKELETON_POSITION_HAND_RIGHT, window);

        // Left Leg
        DrawBone(skel, NUI_SKELETON_POSITION_HIP_LEFT, NUI_SKELETON_POSITION_KNEE_LEFT, window);
        DrawBone(skel, NUI_SKELETON_POSITION_KNEE_LEFT, NUI_SKELETON_POSITION_ANKLE_LEFT, window);
        DrawBone(skel, NUI_SKELETON_POSITION_ANKLE_LEFT, NUI_SKELETON_POSITION_FOOT_LEFT, window);

        // Right Leg
        DrawBone(skel, NUI_SKELETON_POSITION_HIP_RIGHT, NUI_SKELETON_POSITION_KNEE_RIGHT, window);
        DrawBone(skel, NUI_SKELETON_POSITION_KNEE_RIGHT, NUI_SKELETON_POSITION_ANKLE_RIGHT, window);
        DrawBone(skel, NUI_SKELETON_POSITION_ANKLE_RIGHT, NUI_SKELETON_POSITION_FOOT_RIGHT, window);


        // Draw the joints in a different color
        for (int i = 0; i < NUI_SKELETON_POSITION_COUNT; ++i)
        {
            sf::CircleShape circle{};
            circle.setRadius(KinectSettings::g_JointThickness);
            circle.setPosition(screenSkelePoints[i]);

            if (skel.eSkeletonPositionTrackingState[i] == NUI_SKELETON_POSITION_INFERRED)
            {
                circle.setFillColor(sf::Color::Red);
                window.draw(circle);
            }
            else if (skel.eSkeletonPositionTrackingState[i] == NUI_SKELETON_POSITION_TRACKED)
            {
                circle.setFillColor(sf::Color::Yellow);
                window.draw(circle);
            }
        }

    }
    sf::Vector2f KinectV1Handler::SkeletonToScreen(Vector4 skeletonPoint, int _width, int _height) {
        LONG x = 0, y = 0;
        USHORT depth = 0;

        // Calculate the skeleton's position on the screen
        // NuiTransformSkeletonToDepthImage returns coordinates in NUI_IMAGE_RESOLUTION_320x240 space
        NuiTransformSkeletonToDepthImage(skeletonPoint, &x, &y, &depth);

        float screenPointX = x * _width / 320.f;
        float screenPointY = y * _height / 240.f;
        //std::cerr << "x = " << x << " ScreenX = " << screenPointX << " y = " << y << " ScreenY = " << screenPointY << '\n';

        // The skeleton constantly flickers and drops out without the cerr command...
        return sf::Vector2f(screenPointX, screenPointY);
    }
    void KinectV1Handler::DrawBone(const NUI_SKELETON_DATA & skel, NUI_SKELETON_POSITION_INDEX joint0,
        NUI_SKELETON_POSITION_INDEX joint1, sf::RenderWindow &window)
    {
        NUI_SKELETON_POSITION_TRACKING_STATE joint0State = skel.eSkeletonPositionTrackingState[joint0];
        NUI_SKELETON_POSITION_TRACKING_STATE joint1State = skel.eSkeletonPositionTrackingState[joint1];

        // If we can't find either of these joints, exit
        if (joint0State == NUI_SKELETON_POSITION_NOT_TRACKED || joint1State == NUI_SKELETON_POSITION_NOT_TRACKED)
        {
            return;
        }

        // Don't draw if both points are inferred
        if (joint0State == NUI_SKELETON_POSITION_INFERRED && joint1State == NUI_SKELETON_POSITION_INFERRED)
        {
            return;
        }
        // Assume all bones are inferred unless BOTH joints are tracked
        if (joint0State == NUI_SKELETON_POSITION_TRACKED && joint1State == NUI_SKELETON_POSITION_TRACKED)
        {
            DrawLine(screenSkelePoints[joint0], screenSkelePoints[joint1], sf::Color::Green, KinectSettings::g_TrackedBoneThickness, window);
        }
        else
        {
            DrawLine(screenSkelePoints[joint0], screenSkelePoints[joint1], sf::Color::Red, KinectSettings::g_InferredBoneThickness, window);
        }
    }
    void KinectV1Handler::DrawLine(sf::Vector2f start, sf::Vector2f end, sf::Color colour, float lineThickness, sf::RenderWindow &window) {
        sfLine line(start, end);
        line.setColor(colour);
        line.setThickness(lineThickness);
        window.draw(line);
        //std::cerr << "Line drawn at: " << start.x << ", " << start.y << " to " << end.x << ", " << end.y << "\n";
    }
    Vector4 KinectV1Handler::zeroKinectPosition(int trackedSkeletonIndex) {
        return jointPositions[NUI_SKELETON_POSITION_HEAD];
    }
    void KinectV1Handler::setKinectToVRMultiplier(int skeletonIndex) {
        /*
        KinectSettings::kinectToVRScale = KinectSettings::hmdZero.v[1]
            / (jointPositions[NUI_SKELETON_POSITION_HEAD].y
                +
                -jointPositions[NUI_SKELETON_POSITION_FOOT_LEFT].y);
        //std::cerr << "HMD zero: " << KinectSettings::hmdZero.v[1] << '\n';
        std::cerr << "head pos: " << jointPositions[NUI_SKELETON_POSITION_HEAD].y << '\n';
        std::cerr << "foot pos: " << jointPositions[NUI_SKELETON_POSITION_FOOT_LEFT].y << '\n';
        */
    }


    bool KinectV1Handler::jointsUntracked(KVR::KinectJoint joint0, KVR::KinectJoint joint1, NUI_SKELETON_DATA data) {
        NUI_SKELETON_POSITION_TRACKING_STATE joint0State = data.eSkeletonPositionTrackingState[convertJoint(joint0)];
        NUI_SKELETON_POSITION_TRACKING_STATE joint1State = data.eSkeletonPositionTrackingState[convertJoint(joint1)];

        // If we can't find either of these joints, exit
        return ((joint0State == NUI_SKELETON_POSITION_NOT_TRACKED
            || joint1State == NUI_SKELETON_POSITION_NOT_TRACKED)
            && KinectSettings::ignoreInferredPositions);
    }
    bool KinectV1Handler::jointsInferred(KVR::KinectJoint joint0, KVR::KinectJoint joint1, NUI_SKELETON_DATA data) {
        NUI_SKELETON_POSITION_TRACKING_STATE joint0State = data.eSkeletonPositionTrackingState[convertJoint(joint0)];
        NUI_SKELETON_POSITION_TRACKING_STATE joint1State = data.eSkeletonPositionTrackingState[convertJoint(joint1)];

        // If we can't find either of these joints, exit
        return (joint0State == NUI_SKELETON_POSITION_INFERRED
            && joint1State == NUI_SKELETON_POSITION_INFERRED
            && KinectSettings::ignoreInferredPositions);
    }
