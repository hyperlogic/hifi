//
// handshake.js
//
/* global Xform */
/* eslint indent: ["error", 4, { "outerIIFEBody": 0 }] */
/* eslint max-len: ["error", 1024, 4] */

(function() { // BEGIN LOCAL_SCOPE

Script.include("/~/system/libraries/Xform.js");


// globals transmitted to and from web app
var BLEND_FACTOR = 0.5;  // 0 means ik follows your controller, 1 follows theirs
var SHOW_CONTROLLERS = false;
var USE_LOCAL_IK = true;
var USE_STATIC_HAND_OFFSET = false;
var USE_HAPTICS = true;

//
// tablet app boiler plate
//

var TABLET_BUTTON_NAME = "HANDSHAKE";
var HTML_URL = "https://s3.amazonaws.com/hifi-public/tony/html/handshake2.html?2";

var tablet = Tablet.getTablet("com.highfidelity.interface.tablet.system");
var tabletButton = tablet.addButton({
    text: TABLET_BUTTON_NAME,
    icon: "https://s3.amazonaws.com/hifi-public/tony/icons/tpose-i.svg",
    activeIcon: "https://s3.amazonaws.com/hifi-public/tony/icons/tpose-a.svg"
});

tabletButton.clicked.connect(function () {
    if (shown) {
        tablet.gotoHomeScreen();
    } else {
        tablet.gotoWebScreen(HTML_URL);
    }
});

var shown = false;

function onScreenChanged(type, url) {
    if (type === "Web" && url === HTML_URL) {
        tabletButton.editProperties({isActive: true});
        if (!shown) {
            // hook up to event bridge
            tablet.webEventReceived.connect(onWebEventReceived);
        }
        shown = true;
    } else {
        tabletButton.editProperties({isActive: false});
        if (shown) {
            // disconnect from event bridge
            tablet.webEventReceived.disconnect(onWebEventReceived);
        }
        shown = false;
    }
}

function onWebEventReceived(msg) {
    if (msg.name === "init-complete") {
        var values = [{name: "blend-factor", val: BLEND_FACTOR * 100, checked: false},
                      {name: "show-controllers", val: "on", checked: SHOW_CONTROLLERS},
                      {name: "use-local-ik", val: "on", checked: USE_LOCAL_IK},
                      {name: "use-static-hand-offset", val: "on", checked: USE_STATIC_HAND_OFFSET},
                      {name: "use-haptics", val: "on", checked: USE_HAPTICS}];
        tablet.emitScriptEvent(JSON.stringify(values));
    } else if (msg.name === "blend-factor") {
        BLEND_FACTOR = parseInt(msg.val, 10) / 100;
    } else if (msg.name === "show-controllers") {
        SHOW_CONTROLLERS = msg.checked;
    } else if (msg.name === "use-local-ik") {
        USE_LOCAL_IK = msg.checked;
    } else if (msg.name === "use-static-hand-offset") {
        USE_STATIC_HAND_OFFSET = msg.checked;
    } else if (msg.name === "use-haptics") {
        USE_HAPTICS = msg.checked;
    }
}

tablet.screenChanged.connect(onScreenChanged);

function shutdownTabletApp() {
    tablet.removeButton(tabletButton);
    if (shown) {
        tablet.webEventReceived.disconnect(onWebEventReceived);
        tablet.gotoHomeScreen();
    }
    tablet.screenChanged.disconnect(onScreenChanged);
}

//
// end tablet app boiler plate
//

function clamp(val, min, max) {
    return Math.max(min, Math.min(max, val));
}

var RED = {x: 1, y: 0, z: 0, w: 1};
var MAGENTA = {x: 1, y: 0, z: 1, w: 1};
var BLUE = {x: 0, y: 0, z: 1, w: 1};
var CYAN = {x: 0, y: 1, z: 1, w: 1};
var QUAT_Y_180 = {x: 0, y: 1, z: 0, w: 0};
var QUAT_IDENTITY = {x: 0, y: 0, z: 0, w: 1};
var LEFT_HAND = 0;
var RIGHT_HAND = 1;

var STATIC_RIGHT_HAND_DELTA_XFORM = new Xform({"x":-0.9805938601493835,"y":-0.06915176659822464,"z":0.10610653460025787,"w":-0.14965057373046875},
                                              {"x":-0.010323882102966309,"y":0.20587468147277832,"z":0.035437583923339844});
var STATIC_LEFT_HAND_DELTA_XFORM = new Xform({"x":-0.9791237115859985,"y":-0.16682694852352142,"z":-0.0822417140007019,"w":0.0819871574640274},
                                             {"x":-0.037941932678222656,"y":0.19198226928710938,"z":0.05402088165283203});
var rightHandDeltaXform = STATIC_RIGHT_HAND_DELTA_XFORM;
var leftHandDeltaXform = STATIC_LEFT_HAND_DELTA_XFORM;

var shakeRightHandAvatarId;
var shakeLeftHandAvatarId;
var SHAKE_TRIGGER_DISTANCE = 0.5;

var HAPTIC_PULSE_FIRST_STRENGTH = 1.0;
var HAPTIC_PULSE_FIRST_DURATION = 20.0;
var HAPTIC_PULSE_DISTANCE = 0.01;  // 1 cm
var HAPTIC_PULSE_DURATION = 16.0;
var HAPTIC_PULSE_MIN_STRENGTH = 0.3;
var HAPTIC_PULSE_MAX_STRENGTH = 0.5;
var HAPTIC_PULSE_MIN_DISTANCE = 0.1;
var HAPTIC_PULSE_MAX_DISTANCE = 0.5;

// ctor
function HapticBuddy(hand) {
    this.hand = hand;
    this.enabled = false;
    this.lastPulseDistance = 0;
}

HapticBuddy.prototype.start = function (myHand, otherHand) {
    if (USE_HAPTICS) {
        Controller.triggerHapticPulse(HAPTIC_PULSE_FIRST_STRENGTH, HAPTIC_PULSE_FIRST_DURATION, this.hand);
        this.enabled = true;
        this.lastPulseDistance = Vec3.distance(myHand.controllerPos, otherHand.controllerPos);
    }
};

HapticBuddy.prototype.update = function (myHand, otherHand) {
    if (this.enabled) {
        var distance = Vec3.distance(myHand.controllerPos, otherHand.controllerPos);
        if (Math.abs(this.lastPulseDistance - distance) > HAPTIC_PULSE_DISTANCE) {
            this.lastPulseDistance = distance;
            // convert distance into a value from 0 to 1
            var factor = clamp((distance - HAPTIC_PULSE_MIN_DISTANCE) / (HAPTIC_PULSE_MAX_DISTANCE - HAPTIC_PULSE_MIN_DISTANCE), 0, 1);
            // convert factor into a strength factor
            var strength = factor * (HAPTIC_PULSE_MAX_STRENGTH - HAPTIC_PULSE_MIN_STRENGTH) + HAPTIC_PULSE_MIN_STRENGTH;
            Controller.triggerHapticPulse(strength, HAPTIC_PULSE_DURATION, this.hand);
        }
    }
};

HapticBuddy.prototype.stop = function () {
    this.enabled = false;
};

var rightHapticBuddy = new HapticBuddy(RIGHT_HAND);
var leftHapticBuddy = new HapticBuddy(LEFT_HAND);

// ctor
function SoundBuddy(url) {
    this.sound = SoundCache.getSound(url);
    this.injector = null;
}

SoundBuddy.prototype.play = function (options, doneCallback) {
    if (this.sound.downloaded) {
        if (this.injector) {
            this.injector.setOptions(options);
            this.injector.restart();
        } else {
            this.injector = Audio.playSound(this.sound, options);
            this.injector.finished.connect(function () {
                if (doneCallback) {
                    doneCallback();
                }
            });
        }
    }
};

var CLAP_SOUND = "https://s3.amazonaws.com/hifi-public/tony/audio/slap.wav";
var clapSound = new SoundBuddy(CLAP_SOUND);

function tweenXform(a, b, alpha) {
    return new Xform(Quat.mix(a.rot, b.rot, alpha), Vec3.sum(Vec3.multiply(1 - alpha, a.pos), Vec3.multiply(alpha, b.pos)));
}

function findClosestAvatarHand(key, myHand) {
    var avatarIds = Object.keys(prevAvatarMap);
    var closestId;
    var closestDist = Number.MAX_VALUE;
    avatarIds.forEach(function (id) {
        if (id !== MyAvatar.SELF_ID) {
            var hand = prevAvatarMap[id][key];
            var dist = Vec3.distance(myHand.jointPos, hand.jointPos);
            if (dist < closestDist) {
                closestId = id;
                closestDist = dist;
            }
        }
    });
    return closestId;
}

function calculateDeltaOffset(myHand, otherHand) {
    var myJointXform = new Xform(myHand.jointRot, myHand.jointPos);
    var otherJointXform = new Xform(otherHand.jointRot, otherHand.jointPos);
    var palmOffset = new Xform(QUAT_IDENTITY, Vec3.subtract(otherHand.palmPos, myHand.palmPos));
    return Xform.mul(otherJointXform.inv(), Xform.mul(palmOffset, myJointXform));
}

// Controller system callback on leftTrigger pull or release.
function leftTrigger(value) {
    if (value === 1) {
        if (!shakeLeftHandAvatarId) {
            if (prevAvatarMap[MyAvatar.SELF_ID]) {
                var myHand = prevAvatarMap[MyAvatar.SELF_ID].leftHand;
                var otherId = findClosestAvatarHand('leftHand', myHand);
                if (otherId) {
                    var otherHand = prevAvatarMap[otherId].leftHand;
                    if (Vec3.distance(otherHand.jointPos, myHand.jointPos) < SHAKE_TRIGGER_DISTANCE) {
                        Messages.sendMessage("Hifi-Hand-Disabler", "left");
                        if (USE_STATIC_HAND_OFFSET) {
                            leftHandDeltaXform = STATIC_LEFT_HAND_DELTA_XFORM;
                        } else {
                            leftHandDeltaXform = calculateDeltaOffset(myHand, otherHand);
                        }
                        shakeLeftHandAvatarId = otherId;
                        leftHapticBuddy.start(myHand, otherHand);
                        clapSound.play({position: myHand.jointPos, loop: false, localOnly: true});
                    }
                }
            }
        }
    } else {
        // unset shakeLeftHandAvatarId
        if (shakeLeftHandAvatarId) {
            shakeLeftHandAvatarId = undefined;
        }
        leftHapticBuddy.stop();
        Messages.sendMessage("Hifi-Hand-Disabler", "none");
    }
}

// Controller system callback on rightTrigger pull or release.
function rightTrigger(value) {
    if (value === 1) {
        if (!shakeRightHandAvatarId) {
            if (prevAvatarMap[MyAvatar.SELF_ID]) {
                var myHand = prevAvatarMap[MyAvatar.SELF_ID].rightHand;
                var otherId = findClosestAvatarHand('rightHand', myHand);
                if (otherId) {
                    var otherHand = prevAvatarMap[otherId].rightHand;
                    if (Vec3.distance(otherHand.jointPos, myHand.jointPos) < SHAKE_TRIGGER_DISTANCE) {
                        Messages.sendMessage("Hifi-Hand-Disabler", "right");
                        if (USE_STATIC_HAND_OFFSET) {
                            rightHandDeltaXform = STATIC_RIGHT_HAND_DELTA_XFORM;
                        } else {
                            rightHandDeltaXform = calculateDeltaOffset(myHand, otherHand);
                        }
                        shakeRightHandAvatarId = otherId;
                        rightHapticBuddy.start(myHand, otherHand);
                        clapSound.play({position: myHand.jointPos, loop: false, localOnly: true});
                    }
                }
            }
        }
    } else {
        // unset shakeRightHandAvatarId
        if (shakeRightHandAvatarId) {
            shakeRightHandAvatarId = undefined;
        }
        rightHapticBuddy.stop();
        Messages.sendMessage("Hifi-Hand-Disabler", "none");
    }
}

var connectionMapping = Controller.newMapping('handshake-grip');
var animationHandlerId;
var ANIM_VARS = [
    "leftHandPosition",
    "leftHandRotation",
    "rightHandPosition",
    "rightHandRotation"
];

function handTween(myHand, otherHand, handDelta) {
    var myXform = new Xform(myHand.controllerRot, myHand.controllerPos);
    var otherXform = Xform.mul(new Xform(otherHand.controllerRot, otherHand.controllerPos), handDelta);
    return tweenXform(myXform, otherXform, BLEND_FACTOR);
}

function init() {
    connectionMapping.from(Controller.Standard.RTClick).peek().to(rightTrigger);
    connectionMapping.from(Controller.Standard.LTClick).peek().to(leftTrigger);
    connectionMapping.enable();

    animationHandlerId = MyAvatar.addAnimationStateHandler(function (props) {

        var result = {};
        if (props.leftHandRotation) {
            result.leftHandRotation = props.leftHandRotation;
        }
        if (props.leftHandPosition) {
            result.leftHandPosition = props.leftHandPosition;
        }
        if (props.rightHandRotation) {
            result.rightHandRotation = props.rightHandRotation;
        }
        if (props.rightHandPosition) {
            result.rightHandPosition = props.rightHandPosition;
        }

        var otherHand, myHand, targetXform, localTargetXform;
        var avatarXform = new Xform(Quat.multiply(MyAvatar.orientation, QUAT_Y_180), MyAvatar.position);

        if (shakeRightHandAvatarId && props.rightHandRotation && props.rightHandPosition) {
            otherHand = prevAvatarMap[shakeRightHandAvatarId].rightHand;
            myHand = prevAvatarMap[MyAvatar.SELF_ID].rightHand;
            if (otherHand) {
                targetXform = handTween(myHand, otherHand, rightHandDeltaXform);
                localTargetXform = Xform.mul(avatarXform.inv(), targetXform);
                result.rightHandRotation = localTargetXform.rot;
                result.rightHandPosition = localTargetXform.pos;
                rightHapticBuddy.update(myHand, otherHand);
            }
        }

        if (shakeLeftHandAvatarId && props.leftHandRotation && props.leftHandPosition) {
            otherHand = prevAvatarMap[shakeLeftHandAvatarId].leftHand;
            myHand = prevAvatarMap[MyAvatar.SELF_ID].leftHand;
            if (otherHand) {
                targetXform = handTween(myHand, otherHand, leftHandDeltaXform);
                localTargetXform = Xform.mul(avatarXform.inv(), targetXform);
                result.leftHandRotation = localTargetXform.rot;
                result.leftHandPosition = localTargetXform.pos;
                leftHapticBuddy.update(myHand, otherHand);
            }
        }

        return result;
    }, ANIM_VARS);

    Script.update.connect(update);
    Script.scriptEnding.connect(shutdown);
}

function isIdentity(rot, pos) {
    var EPSILON = 0.01;
    return (Math.abs(rot.x) < EPSILON &&
            Math.abs(rot.y) < EPSILON &&
            Math.abs(rot.z) < EPSILON &&
            Math.abs(rot.w) - 1.0 < EPSILON && // 1 or -1 is ok.
            Math.abs(pos.x) < EPSILON &&
            Math.abs(pos.y) < EPSILON &&
            Math.abs(pos.z) < EPSILON);
}

function fetchHandData(id, jointName, controllerMatName) {
    var avatar = id ? AvatarManager.getAvatar(id) : MyAvatar;
    var jointIndex = avatar.getJointIndex(jointName);
    var mat = avatar[controllerMatName];
    var result = {
        controllerRot: {x: 0, y: 0, z: 0, w: 1},
        controllerPos: {x: 0, y: 0, z: 0},
        controllerValid: false,
        jointRot: {x: 0, y: 0, z: 0, w: 1},
        jointPos: {x: 0, y: 0, z: 0},
        jointValid: false
    };

    if (jointIndex >= 0) {
        // transform joint into world space
        var avatarXform = new Xform(avatar.orientation, avatar.position);
        var localXform = new Xform(avatar.getAbsoluteJointRotationInObjectFrame(jointIndex),
                                   avatar.getAbsoluteJointTranslationInObjectFrame(jointIndex));
        var worldXform = Xform.mul(avatarXform, localXform);
        result.jointRot = worldXform.rot;
        result.jointPos = worldXform.pos;
        result.jointValid = true;

        // check to see if hand controller is valid
        var localControllerXform = new Xform(Mat4.extractRotation(mat),
                                             Mat4.extractTranslation(mat));
        if (!isIdentity(localControllerXform.rot, localControllerXform.pos)) {
            var worldControllerXform = Xform.mul(avatarXform, localControllerXform);
            result.controllerRot = worldControllerXform.rot;
            result.controllerPos = worldControllerXform.pos;
            result.controllerValid = true;
        } else {
            result.controllerRot = result.jointRot;
            result.controllerPos = result.jointPos;
            result.controllerValid = false;
        }
    }

    var jointXform = new Xform(result.jointRot, result.jointPos);
    var localPalmPos;

    var PALM_Y_OFFSET_FACTOR = 0.8;
    var PALM_Z_OFFSET_FACTOR = 0.4;

    if (jointName === "LeftHand") {

        // transform palm into frame of the hand.
        localPalmPos = jointXform.inv().xformPoint(avatar.getLeftPalmPosition());

        // adjust the palm position to better match the handshake palm position.
        localPalmPos.y = localPalmPos.y * PALM_Y_OFFSET_FACTOR;
        localPalmPos.z = localPalmPos.z * PALM_Z_OFFSET_FACTOR;

        // transform local palm back into world space
        result.palmPos = jointXform.xformPoint(localPalmPos);
        result.palmRot = avatar.getLeftPalmRotation();
    } else {

        // transform palm into frame of the hand.
        localPalmPos = jointXform.inv().xformPoint(avatar.getRightPalmPosition());

        // adjust the palm position to better match the handshake palm position.
        localPalmPos.y = localPalmPos.y * PALM_Y_OFFSET_FACTOR;
        localPalmPos.z = localPalmPos.z * PALM_Z_OFFSET_FACTOR;

        // transform local palm back into world space
        result.palmPos = jointXform.xformPoint(localPalmPos);
        result.palmRot = avatar.getRightPalmRotation();
    }

    return result;
}

function addAvatar(id, data) {
    print("addAvatar(" + id + ")");
    print("typeof id = " + (typeof id));
    print("JSON.stringify(id) = " + JSON.stringify(id));
    if (id === "null") {
        id = MyAvatar.SELF_ID;
    }
    print("addAvatar(" + id + ")");
    updateAvatar(id, data);
}

function updateAvatar(id, data) {

    if (SHOW_CONTROLLERS && data.leftHand.controllerValid) {
        DebugDraw.addMarker("leftHandController" + id, data.leftHand.controllerRot, data.leftHand.controllerPos, CYAN);
        DebugDraw.addMarker("leftHandPalm" + id, data.leftHand.palmRot, data.leftHand.palmPos, BLUE);
    } else {
        DebugDraw.removeMarker("leftHandController" + id);
        DebugDraw.removeMarker("leftHandPalm" + id);
    }

    if (SHOW_CONTROLLERS && data.rightHand.controllerValid) {
        DebugDraw.addMarker("rightHandController" + id, data.rightHand.controllerRot, data.rightHand.controllerPos, MAGENTA);
        DebugDraw.addMarker("rightHandPalm" + id, data.rightHand.palmRot, data.rightHand.palmPos, RED);
    } else {
        DebugDraw.removeMarker("rightHandController" + id);
        DebugDraw.removeMarker("rightHandPalm" + id);
    }

    // local IK on other avatars hands
    var avatar = AvatarManager.getAvatar(id);
    if (id !== MyAvatar.SELF_ID && avatar) {
        var rightHandIndex = avatar.getJointIndex("RightHand");
        if (rightHandIndex >= 0) {
            if (shakeRightHandAvatarId === id && USE_LOCAL_IK) {
                var myHand = prevAvatarMap[MyAvatar.SELF_ID].rightHand;
                var otherHand = prevAvatarMap[shakeRightHandAvatarId].rightHand;
                if (otherHand) {
                    var targetXform = handTween(myHand, otherHand, rightHandDeltaXform);
                    var otherTargetXform = Xform.mul(targetXform, rightHandDeltaXform.inv());

                    // world frame
                    if (avatar.pinJoint) { // API, not available on some clients.
                        avatar.pinJoint(rightHandIndex, otherTargetXform.pos, otherTargetXform.rot);
                    }
                }
            } else {
                avatar.clearPinOnJoint(rightHandIndex);
            }
        }

        var leftHandIndex = avatar.getJointIndex("LeftHand");
        if (leftHandIndex >= 0) {
            if (shakeLeftHandAvatarId === id && USE_LOCAL_IK) {
                var myHand = prevAvatarMap[MyAvatar.SELF_ID].leftHand;
                var otherHand = prevAvatarMap[shakeLeftHandAvatarId].leftHand;
                if (otherHand) {
                    var targetXform = handTween(myHand, otherHand, leftHandDeltaXform);
                    var otherTargetXform = Xform.mul(targetXform, leftHandDeltaXform.inv());

                    // world frame
                    if (avatar.pinJoint) { // API, not available on some clients.
                        avatar.pinJoint(leftHandIndex, otherTargetXform.pos, otherTargetXform.rot);
                    }
                }
            } else {
                avatar.clearPinOnJoint(leftHandIndex);
            }
        }
    }
}

function removeAvatar(id) {
    print("removeAvatar(" + id + ")");
    DebugDraw.removeMarker("leftHandController" + id);
    DebugDraw.removeMarker("leftHandJoint" + id);
    DebugDraw.removeMarker("rightHandController" + id);
    DebugDraw.removeMarker("rightHandJoint" + id);
}

var prevAvatarMap = {};

function update(dt) {
    var AVATAR_RANGE = 5; // meters
    var avatarIds = AvatarManager.getAvatarsInRange(MyAvatar.position, AVATAR_RANGE);
    var avatarMap = {};
    avatarIds.forEach(function (id) {
        if (id === null || id === MyAvatar.sessionUUID) {
            id = MyAvatar.SELF_ID;
        }
        avatarMap[id] = {
            leftHand: fetchHandData(id, "LeftHand", "controllerLeftHandMatrix"),
            rightHand: fetchHandData(id, "RightHand", "controllerRightHandMatrix")
        };
    });

    // detect removal
    Object.keys(prevAvatarMap).forEach(function (id) {
        if (!avatarMap[id]) {
            removeAvatar(id);
        }
    });

    // detect addition
    Object.keys(avatarMap).forEach(function (id) {
        if (prevAvatarMap[id]) {
            updateAvatar(id, avatarMap[id]);
        } else {
            addAvatar(id, avatarMap[id]);
        }
    });

    prevAvatarMap = avatarMap;
}

function shutdown() {
    MyAvatar.removeAnimationStateHandler(animationHandlerId);

    // remove all avatars
    Object.keys(prevAvatarMap).forEach(function (id) {
        removeAvatar(id);
    });
    connectionMapping.disable();
    shutdownTabletApp();
}

init();

}()); // END LOCAL_SCOPE
