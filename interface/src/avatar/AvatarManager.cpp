//
//  AvatarManager.cpp
//  interface/src/avatar
//
//  Created by Stephen Birarda on 1/23/2014.
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include <string>

#include <QScriptEngine>

#include <glm/gtx/string_cast.hpp>

#include <GlowEffect.h>
#include <PerfStat.h>
#include <RegisteredMetaTypes.h>
#include <UUID.h>

#include "Application.h"
#include "Avatar.h"
#include "AvatarManager.h"
#include "Menu.h"
#include "MyAvatar.h"
#include "SceneScriptingInterface.h"

// 70 times per second - target is 60hz, but this helps account for any small deviations
// in the update loop
static const quint64 MIN_TIME_BETWEEN_MY_AVATAR_DATA_SENDS = (1000 * 1000) / 70;

// We add _myAvatar into the hash with all the other AvatarData, and we use the default NULL QUid as the key.
const QUuid MY_AVATAR_KEY;  // NULL key

static QScriptValue localLightToScriptValue(QScriptEngine* engine, const AvatarManager::LocalLight& light) {
    QScriptValue object = engine->newObject();
    object.setProperty("direction", vec3toScriptValue(engine, light.direction));
    object.setProperty("color", vec3toScriptValue(engine, light.color));
    return object;
}

static void localLightFromScriptValue(const QScriptValue& value, AvatarManager::LocalLight& light) {
    vec3FromScriptValue(value.property("direction"), light.direction);
    vec3FromScriptValue(value.property("color"), light.color);
}

void AvatarManager::registerMetaTypes(QScriptEngine* engine) {
    qScriptRegisterMetaType(engine, localLightToScriptValue, localLightFromScriptValue);
    qScriptRegisterSequenceMetaType<QVector<AvatarManager::LocalLight> >(engine);
}

AvatarManager::AvatarManager(QObject* parent) :
    _avatarFades() {
    // register a meta type for the weak pointer we'll use for the owning avatar mixer for each avatar
    qRegisterMetaType<QWeakPointer<Node> >("NodeWeakPointer");
    _myAvatar = QSharedPointer<MyAvatar>(new MyAvatar());
}

void AvatarManager::init() {
    _myAvatar->init();
    _avatarHash.insert(MY_AVATAR_KEY, _myAvatar);
}

void AvatarManager::updateMyAvatar(float deltaTime) {
    bool showWarnings = Menu::getInstance()->isOptionChecked(MenuOption::PipelineWarnings);
    PerformanceWarning warn(showWarnings, "AvatarManager::updateMyAvatar()");
    
    _myAvatar->update(deltaTime);
    
    quint64 now = usecTimestampNow();
    quint64 dt = now - _lastSendAvatarDataTime;
    
    if (dt > MIN_TIME_BETWEEN_MY_AVATAR_DATA_SENDS) {
        // send head/hand data to the avatar mixer and voxel server
        PerformanceTimer perfTimer("send");
        _myAvatar->sendAvatarDataPacket();
        _lastSendAvatarDataTime = now;
    }
}

void AvatarManager::updateOtherAvatars(float deltaTime) {
    if (_avatarHash.size() < 2 && _avatarFades.isEmpty()) {
        return;
    }
    bool showWarnings = Menu::getInstance()->isOptionChecked(MenuOption::PipelineWarnings);
    PerformanceWarning warn(showWarnings, "Application::updateAvatars()");

    PerformanceTimer perfTimer("otherAvatars");
    
    // simulate avatars
    AvatarHash::iterator avatarIterator = _avatarHash.begin();
    while (avatarIterator != _avatarHash.end()) {
        Avatar* avatar = reinterpret_cast<Avatar*>(avatarIterator.value().data());
        
        if (avatar == _myAvatar || !avatar->isInitialized()) {
            // DO NOT update _myAvatar!  Its update has already been done earlier in the main loop.
            // DO NOT update or fade out uninitialized Avatars
            ++avatarIterator;
        } else if (avatar->shouldDie()) {
            _avatarFades.push_back(avatarIterator.value());
            avatarIterator = _avatarHash.erase(avatarIterator);
        } else {
            avatar->simulate(deltaTime);
            ++avatarIterator;
        }
    }
    
    // simulate avatar fades
    simulateAvatarFades(deltaTime);
}

void AvatarManager::renderAvatars(RenderArgs::RenderMode renderMode, bool postLighting, bool selfAvatarOnly) {
    PerformanceWarning warn(Menu::getInstance()->isOptionChecked(MenuOption::PipelineWarnings),
                            "Application::renderAvatars()");
    bool renderLookAtVectors = Menu::getInstance()->isOptionChecked(MenuOption::RenderLookAtVectors);
    
    glm::vec3 cameraPosition = Application::getInstance()->getCamera()->getPosition();

    if (!selfAvatarOnly) {
        if (DependencyManager::get<SceneScriptingInterface>()->shouldRenderAvatars()) {
            foreach (const AvatarSharedPointer& avatarPointer, _avatarHash) {
                Avatar* avatar = static_cast<Avatar*>(avatarPointer.data());
                if (!avatar->isInitialized()) {
                    continue;
                }
                avatar->render(cameraPosition, renderMode, postLighting);
                avatar->setDisplayingLookatVectors(renderLookAtVectors);
            }
            renderAvatarFades(cameraPosition, renderMode);
        }
    } else {
        // just render myAvatar
        _myAvatar->render(cameraPosition, renderMode, postLighting);
        _myAvatar->setDisplayingLookatVectors(renderLookAtVectors);
    }
}

void AvatarManager::simulateAvatarFades(float deltaTime) {
    QVector<AvatarSharedPointer>::iterator fadingIterator = _avatarFades.begin();
    
    const float SHRINK_RATE = 0.9f;
    const float MIN_FADE_SCALE = 0.001f;

    while (fadingIterator != _avatarFades.end()) {
        Avatar* avatar = static_cast<Avatar*>(fadingIterator->data());
        avatar->setTargetScale(avatar->getScale() * SHRINK_RATE, true);
        if (avatar->getTargetScale() < MIN_FADE_SCALE) {
            fadingIterator = _avatarFades.erase(fadingIterator);
        } else {
            avatar->simulate(deltaTime);
            ++fadingIterator;
        }
    }
}

void AvatarManager::renderAvatarFades(const glm::vec3& cameraPosition, RenderArgs::RenderMode renderMode) {
    // render avatar fades
    Glower glower(renderMode == RenderArgs::NORMAL_RENDER_MODE ? 1.0f : 0.0f);
    
    foreach(const AvatarSharedPointer& fadingAvatar, _avatarFades) {
        Avatar* avatar = static_cast<Avatar*>(fadingAvatar.data());
        if (avatar != static_cast<Avatar*>(_myAvatar.data()) && avatar->isInitialized()) {
            avatar->render(cameraPosition, renderMode);
        }
    }
}

AvatarSharedPointer AvatarManager::newSharedAvatar() {
    return AvatarSharedPointer(new Avatar());
}

// virtual 
AvatarSharedPointer AvatarManager::addAvatar(const QUuid& sessionUUID, const QWeakPointer<Node>& mixerWeakPointer) {
    AvatarSharedPointer avatar = AvatarHashMap::addAvatar(sessionUUID, mixerWeakPointer);
    return avatar;
}

// protected
void AvatarManager::removeAvatarMotionState(Avatar* avatar) {
    AvatarMotionState* motionState= avatar->_motionState;
    if (motionState) {
        // clean up physics stuff
        motionState->clearObjectBackPointer();
        avatar->_motionState = nullptr;
        _avatarMotionStates.remove(motionState);
        _motionStatesToAdd.remove(motionState);
        _motionStatesToDelete.push_back(motionState);
    }
}

// virtual 
void AvatarManager::removeAvatar(const QUuid& sessionUUID) {
    AvatarHash::iterator avatarIterator = _avatarHash.find(sessionUUID);
    if (avatarIterator != _avatarHash.end()) {
        Avatar* avatar = reinterpret_cast<Avatar*>(avatarIterator.value().data());
        if (avatar != _myAvatar && avatar->isInitialized()) {
            removeAvatarMotionState(avatar);

            _avatarFades.push_back(avatarIterator.value());
            _avatarHash.erase(avatarIterator);
        }
    }
}

void AvatarManager::clearOtherAvatars() {
    // clear any avatars that came from an avatar-mixer
    AvatarHash::iterator avatarIterator =  _avatarHash.begin();
    while (avatarIterator != _avatarHash.end()) {
        Avatar* avatar = reinterpret_cast<Avatar*>(avatarIterator.value().data());
        if (avatar == _myAvatar || !avatar->isInitialized()) {
            // don't remove myAvatar or uninitialized avatars from the list
            ++avatarIterator;
        } else {
            removeAvatarMotionState(avatar);
            _avatarFades.push_back(avatarIterator.value());
            avatarIterator = _avatarHash.erase(avatarIterator);
        }
    }
    _myAvatar->clearLookAtTargetAvatar();
}

void AvatarManager::setLocalLights(const QVector<AvatarManager::LocalLight>& localLights) {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, "setLocalLights", Q_ARG(const QVector<AvatarManager::LocalLight>&, localLights));
        return;
    }
    _localLights = localLights;
}

QVector<AvatarManager::LocalLight> AvatarManager::getLocalLights() const {
    if (QThread::currentThread() != thread()) {
        QVector<AvatarManager::LocalLight> result;
        QMetaObject::invokeMethod(const_cast<AvatarManager*>(this), "getLocalLights", Qt::BlockingQueuedConnection,
            Q_RETURN_ARG(QVector<AvatarManager::LocalLight>, result));
        return result;
    }
    return _localLights;
}

VectorOfMotionStates& AvatarManager::getObjectsToDelete() {
    _tempMotionStates.clear();
    _tempMotionStates.swap(_motionStatesToDelete);
    return _tempMotionStates;
}

VectorOfMotionStates& AvatarManager::getObjectsToAdd() {
    _tempMotionStates.clear();

    for (auto motionState : _motionStatesToAdd) {
        _tempMotionStates.push_back(motionState);
    }
    _motionStatesToAdd.clear();
    return _tempMotionStates;
}

VectorOfMotionStates& AvatarManager::getObjectsToChange() {
    _tempMotionStates.clear();
    for (auto state : _avatarMotionStates) {
        if (state->_dirtyFlags > 0) {
            _tempMotionStates.push_back(state);
        }
    }
    return _tempMotionStates;
}

void AvatarManager::handleOutgoingChanges(VectorOfMotionStates& motionStates) {
    // TODO: extract the MyAvatar results once we use a MotionState for it.
}

void AvatarManager::handleCollisionEvents(CollisionEvents& collisionEvents) {
    // TODO: expose avatar collision events to JS
}

void AvatarManager::updateAvatarPhysicsShape(const QUuid& id) {
    AvatarHash::iterator avatarItr = _avatarHash.find(id);
    if (avatarItr != _avatarHash.end()) {
        Avatar* avatar = static_cast<Avatar*>(avatarItr.value().data());
        AvatarMotionState* motionState = avatar->_motionState;
        if (motionState) {
            motionState->addDirtyFlags(EntityItem::DIRTY_SHAPE);
        } else {
            ShapeInfo shapeInfo;
            avatar->computeShapeInfo(shapeInfo);
            btCollisionShape* shape = ObjectMotionState::getShapeManager()->getShape(shapeInfo);
            if (shape) {
                AvatarMotionState* motionState = new AvatarMotionState(avatar, shape);
                avatar->_motionState = motionState;
                _motionStatesToAdd.insert(motionState);
                _avatarMotionStates.insert(motionState);
            }
        }
    }
}
