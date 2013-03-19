//
//  Head.cpp
//  interface
//
//  Created by Philip Rosedale on 9/11/12.
//  Copyright (c) 2012 Physical, Inc.. All rights reserved.
//

#include <iostream>
#include <glm/glm.hpp>
#include "Head.h"
#include <vector>
#include <lodepng.h>
#include <fstream>
#include <sstream>

using namespace std;

float skinColor[] = {1.0f, 0.84f, 0.66f};
float browColor[] = {210.0f/255.0f, 105.0f/255.0f, 30.0f/255.0f};
float mouthColor[] = {1, 0, 0};

float BrowRollAngle[5] = {0, 15, 30, -30, -15};
float BrowPitchAngle[3] = {-70, -60, -50};
float eyeColor[3] = {1,1,1};

float MouthWidthChoices[3] = {0.5f, 0.77f, 0.3f};

float browWidth = 0.8f;
float browThickness = 0.16f;

const float DECAY = 0.1f;

char iris_texture_file[] = "interface.app/Contents/Resources/images/green_eye.png";

vector<unsigned char> iris_texture;
unsigned int iris_texture_width = 512;
unsigned int iris_texture_height = 256;

GLUquadric *sphere = gluNewQuadric();

Head::Head()
{
    position.x = position.y = position.z = 0;
    PupilSize = 0.10f;
    interPupilDistance = 0.6f;
    interBrowDistance = 0.75f;
    NominalPupilSize = 0.10f;
    Yaw = 0.0f;
    EyebrowPitch[0] = EyebrowPitch[1] = -30;
    EyebrowRoll[0] = 20;
    EyebrowRoll[1] = -20;
    MouthPitch = 0;
    MouthYaw = 0;
    MouthWidth = 1.0;
    MouthHeight = 0.2f;
    EyeballPitch[0] = EyeballPitch[1] = 0;
    EyeballScaleX = 1.2f;  EyeballScaleY = 1.5f; EyeballScaleZ = 1.0f;
    EyeballYaw[0] = EyeballYaw[1] = 0;
    PitchTarget = YawTarget = 0; 
    NoiseEnvelope = 1.0;
    PupilConverge = 10.0;
    leanForward = 0.0;
    leanSideways = 0.0;
    eyeContact = 1;
    eyeContactTarget = LEFT_EYE;
    scale = 1.0;
    renderYaw = 0.0;
    renderPitch = 0.0;
    audioAttack = 0.0;
    loudness = 0.0;
    averageLoudness = 0.0;
    lastLoudness = 0.0;
    browAudioLift = 0.0;
    noise = 0;
    
    hand = new Hand(glm::vec3(skinColor[0], skinColor[1], skinColor[2]));

    if (iris_texture.size() == 0) {
        unsigned error = lodepng::decode(iris_texture, iris_texture_width, iris_texture_height, iris_texture_file);
        if (error != 0) {
            std::cout << "error " << error << ": " << lodepng_error_text(error) << std::endl;
        }
    }
}

Head::Head(const Head &otherHead) {
    position = otherHead.position;
    PupilSize = otherHead.PupilSize;
    interPupilDistance = otherHead.interPupilDistance;
    interBrowDistance = otherHead.interBrowDistance;
    NominalPupilSize = otherHead.NominalPupilSize;
    Yaw = otherHead.Yaw;
    EyebrowPitch[0] = otherHead.EyebrowPitch[0];
    EyebrowPitch[1] = otherHead.EyebrowPitch[1];
    EyebrowRoll[0] = otherHead.EyebrowRoll[0];
    EyebrowRoll[1] = otherHead.EyebrowRoll[1];
    MouthPitch = otherHead.MouthPitch;
    MouthYaw = otherHead.MouthYaw;
    MouthWidth = otherHead.MouthWidth;
    MouthHeight = otherHead.MouthHeight;
    EyeballPitch[0] = otherHead.EyeballPitch[0];
    EyeballPitch[1] = otherHead.EyeballPitch[1];
    EyeballScaleX = otherHead.EyeballScaleX;
    EyeballScaleY = otherHead.EyeballScaleY;
    EyeballScaleZ = otherHead.EyeballScaleZ;
    EyeballYaw[0] = otherHead.EyeballYaw[0];
    EyeballYaw[1] = otherHead.EyeballYaw[1];
    PitchTarget = otherHead.PitchTarget;
    YawTarget = otherHead.YawTarget;
    NoiseEnvelope = otherHead.NoiseEnvelope;
    PupilConverge = otherHead.PupilConverge;
    leanForward = otherHead.leanForward;
    leanSideways = otherHead.leanSideways;
    eyeContact = otherHead.eyeContact;
    eyeContactTarget = otherHead.eyeContactTarget;
    scale = otherHead.scale;
    renderYaw = otherHead.renderYaw;
    renderPitch = otherHead.renderPitch;
    audioAttack = otherHead.audioAttack;
    loudness = otherHead.loudness;
    averageLoudness = otherHead.averageLoudness;
    lastLoudness = otherHead.lastLoudness;
    browAudioLift = otherHead.browAudioLift;
    noise = otherHead.noise;
    
    Hand newHand = Hand(*otherHead.hand);
    hand = &newHand;
}

Head::~Head() {
    if (sphere) {
        gluDeleteQuadric(sphere);
    }
}

Head* Head::clone() const {
    return new Head(*this);
}

void Head::reset()
{
    Pitch = Yaw = Roll = 0;
    leanForward = leanSideways = 0;
}

void Head::UpdatePos(float frametime, SerialInterface * serialInterface, int head_mirror, glm::vec3 * gravity)
//  Using serial data, update avatar/render position and angles
{
    
    const float PITCH_ACCEL_COUPLING = 0.5;
    const float ROLL_ACCEL_COUPLING = -1.0;
    float measured_pitch_rate = static_cast<float>(serialInterface->getRelativeValue(PITCH_RATE));
    YawRate = static_cast<float>(serialInterface->getRelativeValue(YAW_RATE));
    float measured_lateral_accel = serialInterface->getRelativeValue(ACCEL_X) -
                                ROLL_ACCEL_COUPLING*serialInterface->getRelativeValue(ROLL_RATE);
    float measured_fwd_accel = serialInterface->getRelativeValue(ACCEL_Z) -
                                PITCH_ACCEL_COUPLING*serialInterface->getRelativeValue(PITCH_RATE);
    float measured_roll_rate = static_cast<float>(serialInterface->getRelativeValue(ROLL_RATE));
    
    //std::cout << "Pitch Rate: " << serialInterface->getRelativeValue(PITCH_RATE) <<
    //    " fwd_accel: " << serialInterface->getRelativeValue(ACCEL_Z) << "\n";
    //std::cout << "Roll Rate: " << serialInterface->getRelativeValue(ROLL_RATE) <<
    //" ACCEL_X: " << serialInterface->getRelativeValue(ACCEL_X) << "\n";
    //std::cout << "Pitch: " << Pitch << "\n";
    
    //  Update avatar head position based on measured gyro rates
    const float HEAD_ROTATION_SCALE = 0.70f;
    const float HEAD_ROLL_SCALE = 0.40f;
    const float HEAD_LEAN_SCALE = 0.01f;
    const float MAX_PITCH = 45;
    const float MIN_PITCH = -45;
    const float MAX_YAW = 85;
    const float MIN_YAW = -85;

    if ((Pitch < MAX_PITCH) && (Pitch > MIN_PITCH))
        addPitch(measured_pitch_rate * -HEAD_ROTATION_SCALE * frametime);
    addRoll(-measured_roll_rate * HEAD_ROLL_SCALE * frametime);

    if (head_mirror) {
        if ((Yaw < MAX_YAW) && (Yaw > MIN_YAW))
            addYaw(-YawRate * HEAD_ROTATION_SCALE * frametime);
        addLean(-measured_lateral_accel * frametime * HEAD_LEAN_SCALE, -measured_fwd_accel*frametime * HEAD_LEAN_SCALE);
    } else {
        if ((Yaw < MAX_YAW) && (Yaw > MIN_YAW))
            addYaw(YawRate * -HEAD_ROTATION_SCALE * frametime);
        addLean(measured_lateral_accel * frametime * -HEAD_LEAN_SCALE, measured_fwd_accel*frametime * HEAD_LEAN_SCALE);        
    } 
}

void Head::addLean(float x, float z) {
    //  Add Body lean as impulse 
    leanSideways += x;
    leanForward += z;
}

void Head::setLeanForward(float dist){
    leanForward = dist;
}

void Head::setLeanSideways(float dist){
    leanSideways = dist;
}

//  Simulate the head over time 
void Head::simulate(float deltaTime)
{
    if (!noise)
    {
        //  Decay back toward center 
        Pitch *= (1.f - DECAY*2*deltaTime);
        Yaw *= (1.f - DECAY*2*deltaTime);
        Roll *= (1.f - DECAY*2*deltaTime);
    }
    else {
        //  Move toward new target  
        Pitch += (PitchTarget - Pitch)*10*deltaTime;   // (1.f - DECAY*deltaTime)*Pitch + ;
        Yaw += (YawTarget - Yaw)*10*deltaTime; //  (1.f - DECAY*deltaTime);
        Roll *= (1.f - DECAY*deltaTime);
    }
    
    leanForward *= (1.f - DECAY*30.f*deltaTime);
    leanSideways *= (1.f - DECAY*30.f*deltaTime);
    
    //  Update where the avatar's eyes are 
    //
    //  First, decide if we are making eye contact or not
    if (randFloat() < 0.005) {
        eyeContact = !eyeContact;
        eyeContact = 1;
        if (!eyeContact) {
            //  If we just stopped making eye contact,move the eyes markedly away
            EyeballPitch[0] = EyeballPitch[1] = EyeballPitch[0] + 5.0f + (randFloat() - 0.5f)*10.0f;
            EyeballYaw[0] = EyeballYaw[1] = EyeballYaw[0] + 5.0f + (randFloat()- 0.5f)*5.0f;
        } else {
            //  If now making eye contact, turn head to look right at viewer
            SetNewHeadTarget(0,0);
        }
    }
    
    const float DEGREES_BETWEEN_VIEWER_EYES = 3;
    const float DEGREES_TO_VIEWER_MOUTH = 7;

    if (eyeContact) {
        //  Should we pick a new eye contact target?
        if (randFloat() < 0.01) {
            //  Choose where to look next
            if (randFloat() < 0.1) {
                eyeContactTarget = MOUTH;
            } else {
                if (randFloat() < 0.5) eyeContactTarget = LEFT_EYE; else eyeContactTarget = RIGHT_EYE;
            }
        }
    }

    
    if (noise)
    {
        Pitch += (randFloat() - 0.5f)*0.2f*NoiseEnvelope;
        Yaw += (randFloat() - 0.5f)*0.3f*NoiseEnvelope;
        //PupilSize += (randFloat() - 0.5)*0.001*NoiseEnvelope;
        
        if (randFloat() < 0.005) MouthWidth = MouthWidthChoices[rand()%3];
        
        if (!eyeContact) {
            if (randFloat() < 0.01f)  EyeballPitch[0] = EyeballPitch[1] = (randFloat() - 0.5f)*20.0f;
            if (randFloat() < 0.01f)  EyeballYaw[0] = EyeballYaw[1] = (randFloat()- 0.5f)*10.0f;
        } else {
            float eye_target_yaw_adjust = 0;
            float eye_target_pitch_adjust = 0;
            if (eyeContactTarget == LEFT_EYE) eye_target_yaw_adjust = DEGREES_BETWEEN_VIEWER_EYES;
            if (eyeContactTarget == RIGHT_EYE) eye_target_yaw_adjust = -DEGREES_BETWEEN_VIEWER_EYES;
            if (eyeContactTarget == MOUTH) eye_target_pitch_adjust = DEGREES_TO_VIEWER_MOUTH;
            
            EyeballPitch[0] = EyeballPitch[1] = -Pitch + eye_target_pitch_adjust;
            EyeballYaw[0] = EyeballYaw[1] = -Yaw + eye_target_yaw_adjust;
        }
        
        
        
        if ((randFloat() < 0.005) && (fabs(PitchTarget - Pitch) < 1.0) && (fabs(YawTarget - Yaw) < 1.0))
        {
            SetNewHeadTarget((randFloat()-0.5f)*20.0f, (randFloat()-0.5f)*45.0f);
        }

        if (0)
        {

            //  Pick new target
            PitchTarget = (randFloat() - 0.5f)*45.0f;
            YawTarget = (randFloat() - 0.5f)*22.0f;
        }
        if (randFloat() < 0.01f)
        {
            EyebrowPitch[0] = EyebrowPitch[1] = BrowPitchAngle[rand()%3];
            EyebrowRoll[0] = EyebrowRoll[1] = BrowRollAngle[rand()%5];
            EyebrowRoll[1]*=-1;
        }
                         
    }
    hand->simulate(deltaTime);
                         
                         
}
      
void Head::render(int faceToFace, int isMine, float * myLocation)
{
    int side = 0;
    
    glm::vec3 cameraHead(myLocation[0], myLocation[1], myLocation[2]);
    float distanceToCamera = glm::distance(cameraHead, position);
    
    //  Always render own hand, but don't render head unless showing face2face
    glEnable(GL_DEPTH_TEST);
    glPushMatrix();
    
    glScalef(scale, scale, scale);
    glTranslatef(leanSideways, 0.f, leanForward);
    
    glRotatef(Yaw, 0, 1, 0);
    
    hand->render(1);
    
    //  Don't render a head if it is really close to your location, because that is your own head!
    if ((distanceToCamera > 1.0) || faceToFace) {
        
        glRotatef(Pitch, 1, 0, 0);
        glRotatef(Roll, 0, 0, 1);
        
        
        // Overall scale of head
        if (faceToFace) glScalef(1.5, 2.0, 2.0);
        else glScalef(0.75, 1.0, 1.0);
        glColor3fv(skinColor);


        //  Head
        glutSolidSphere(1, 30, 30);
        
        //std::cout << distanceToCamera << "\n";
        
        //  Ears
        glPushMatrix();
            glTranslatef(1.0, 0, 0);
            for(side = 0; side < 2; side++)
            {
                glPushMatrix();
                    glScalef(0.3f, 0.65f, .65f);
                    glutSolidSphere(0.5f, 30, 30);  
                glPopMatrix();
                glTranslatef(-2.0f, 0, 0);
            }
        glPopMatrix();
        

        // Eyebrows
        audioAttack = 0.9f*audioAttack + 0.1f*fabs(loudness - lastLoudness);
        lastLoudness = loudness;
    
        const float BROW_LIFT_THRESHOLD = 100;
        if (audioAttack > BROW_LIFT_THRESHOLD)
            browAudioLift += sqrt(audioAttack)/1000.0f;
        
        browAudioLift *= .90f;
        
        glPushMatrix();
            glTranslatef(-interBrowDistance/2.0f,0.4f,0.45f);
            for(side = 0; side < 2; side++)
            {
                glColor3fv(browColor);
                glPushMatrix();
                    glTranslatef(0, 0.35f + browAudioLift, 0);
                    glRotatef(EyebrowPitch[side]/2.0f, 1, 0, 0);
                    glRotatef(EyebrowRoll[side]/2.0f, 0, 0, 1);
                    glScalef(browWidth, browThickness, 1);
                    glutSolidCube(0.5f);
                glPopMatrix();
                glTranslatef(interBrowDistance, 0, 0);
            }
        glPopMatrix();
        
        
        // Mouth
        
        glPushMatrix();
            glTranslatef(0,-0.35f,0.75f);
            glColor3f(0,0,0);
            glRotatef(MouthPitch, 1, 0, 0);
            glRotatef(MouthYaw, 0, 0, 1);
            glScalef(MouthWidth*(.7f + sqrt(averageLoudness)/60.0f), MouthHeight*(1.0f + sqrt(averageLoudness)/30.0f), 1);
            glutSolidCube(0.5f);
        glPopMatrix();
        
        glTranslatef(0, 1.0, 0);
       
        glTranslatef(-interPupilDistance/2.0f,-0.68f,0.7f);
        // Right Eye
        glRotatef(-10, 1, 0, 0);
        glColor3fv(eyeColor);
        glPushMatrix(); 
        {
            glTranslatef(interPupilDistance/10.0f, 0, 0.05f);
            glRotatef(20, 0, 0, 1);
            glScalef(EyeballScaleX, EyeballScaleY, EyeballScaleZ);
            glutSolidSphere(0.25, 30, 30);
        }
        glPopMatrix();
        
        // Right Pupil
        if (!sphere) {
            sphere = gluNewQuadric();
            gluQuadricTexture(sphere, GL_TRUE);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            gluQuadricOrientation(sphere, GLU_OUTSIDE);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, iris_texture_width, iris_texture_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, &iris_texture[0]);
        }

        glPushMatrix();
        {
            glRotatef(EyeballPitch[1], 1, 0, 0);
            glRotatef(EyeballYaw[1] + PupilConverge, 0, 1, 0);
            glTranslatef(0,0,.35f);
            glRotatef(-75,1,0,0);
            glScalef(1.0f, 0.4f, 1.0f);
            
            glEnable(GL_TEXTURE_2D);
            gluSphere(sphere, PupilSize, 15, 15);
            glDisable(GL_TEXTURE_2D);
        }

        glPopMatrix();
        // Left Eye
        glColor3fv(eyeColor);
        glTranslatef(interPupilDistance, 0, 0);
        glPushMatrix(); 
        {
            glTranslatef(-interPupilDistance/10.0f, 0, .05f);
            glRotatef(-20, 0, 0, 1);
            glScalef(EyeballScaleX, EyeballScaleY, EyeballScaleZ);
            glutSolidSphere(0.25, 30, 30);
        }
        glPopMatrix();
        // Left Pupil
        glPushMatrix();
        {
            glRotatef(EyeballPitch[0], 1, 0, 0);
            glRotatef(EyeballYaw[0] - PupilConverge, 0, 1, 0);
            glTranslatef(0, 0, .35f);
            glRotatef(-75, 1, 0, 0);
            glScalef(1.0f, 0.4f, 1.0f);

            glEnable(GL_TEXTURE_2D);
            gluSphere(sphere, PupilSize, 15, 15);
            glDisable(GL_TEXTURE_2D);
        }
        
        glPopMatrix();

    }
    glPopMatrix();
 }

//  Transmit data to agents requesting it 

int Head::getBroadcastData(char* data)
{
    // Copy data for transmission to the buffer, return length of data
    sprintf(data, "H%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f",
            getRenderPitch() + Pitch, -getRenderYaw() + 180 -Yaw, Roll,
            position.x + leanSideways, position.y, position.z + leanForward,
            loudness, averageLoudness,
            hand->getPos().x, hand->getPos().y, hand->getPos().z);
    return strlen(data);
}

void Head::parseData(void *data, int size) {
    // parse head data for this agent
    glm::vec3 handPos(0,0,0);
    sscanf((char *)data, "H%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f",
           &Pitch, &Yaw, &Roll,
           &position.x, &position.y, &position.z,
           &loudness, &averageLoudness,
           &handPos.x, &handPos.y, &handPos.z);
    if (glm::length(handPos) > 0.0) hand->setPos(handPos);
}

void Head::SetNewHeadTarget(float pitch, float yaw)
{
    PitchTarget = pitch;
    YawTarget = yaw;
}
