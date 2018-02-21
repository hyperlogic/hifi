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
var USE_STATIC_HAND_OFFSET = true;
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


var RED = {x: 1, y: 0, z: 0, w: 1};
var MAGENTA = {x: 1, y: 0, z: 1, w: 1};
var BLUE = {x: 0, y: 0, z: 1, w: 1};
var CYAN = {x: 0, y: 1, z: 1, w: 1};
var QUAT_Y_180 = {x: 0, y: 1, z: 0, w: 0};

var rightHandDeltaXform = new Xform({"x":-0.9805938601493835,"y":-0.06915176659822464,"z":0.10610653460025787,"w":-0.14965057373046875},
                                    {"x":-0.010323882102966309,"y":0.20587468147277832,"z":0.035437583923339844});
var leftHandDeltaXform = new Xform({"x":-0.9791237115859985,"y":-0.16682694852352142,"z":-0.0822417140007019,"w":0.0819871574640274},
                                   {"x":-0.037941932678222656,"y":0.19198226928710938,"z":0.05402088165283203});

var shakeRightHandAvatarId;
var SHAKE_TRIGGER_DISTANCE = 0.5;

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

function leftTrigger(value) {
    // TODO
}

function recordDeltaOffset(key) {
    var avatarIds = Object.keys(prevAvatarMap);
    if (avatarIds.length === 2) {
        var myAvatarId = (avatarIds[0] === MyAvatar.SELF_ID) ? avatarIds[0] : avatarIds[1];
        var otherAvatarId = (avatarIds[0] === MyAvatar.SELF_ID) ? avatarIds[1] : avatarIds[0];
        var myJointXform = new Xform(prevAvatarMap[myAvatarId][key].jointRot,
                                     prevAvatarMap[myAvatarId][key].jointPos);
        var otherJointXform = new Xform(prevAvatarMap[otherAvatarId][key].jointRot,
                                        prevAvatarMap[otherAvatarId][key].jointPos);
        var deltaXform = Xform.mul(otherJointXform.inv(), myJointXform);
        print("AJT: " + key + " deltaXform, rot = " + JSON.stringify(deltaXform.rot) + "pos = " + JSON.stringify(deltaXform.pos));

        // can slam IK target to (otherJointXform * delta)
    }
}

function rightTrigger(value) {

    if (value === 1) {

        if (!shakeRightHandAvatarId) {

            print("AJT: prevAvatarMap[MyAvatar.SELF_ID] = " + JSON.stringify(prevAvatarMap[MyAvatar.SELF_ID]));
            if (prevAvatarMap[MyAvatar.SELF_ID]) {
                var myHand = prevAvatarMap[MyAvatar.SELF_ID].rightHand;

                print("AJT: myHand = " + JSON.stringify(myHand));

                var otherId = findClosestAvatarHand('rightHand', myHand);

                print("AJT: otherId = " + otherId);
                if (otherId) {
                    var otherHand = prevAvatarMap[otherId].rightHand;

                    print("AJT: otherHand = " + JSON.stringify(otherHand));
                    print("AJT: distance = " + Vec3.distance(otherHand.jointPos, myHand.jointPos));

                    if (Vec3.distance(otherHand.jointPos, myHand.jointPos) < SHAKE_TRIGGER_DISTANCE) {

                        recordDeltaOffset('rightHand');

                        print("AJT: starting hand shake with avatar = " + otherId);
                        shakeRightHandAvatarId = otherId;
                    }
                }
            }
        }
    } else {
        // unset shakeRightHandAvatarId
        if (shakeRightHandAvatarId) {
            print("AJT: stopping hand shake with avatar = " + shakeRightHandAvatarId);
            shakeRightHandAvatarId = undefined;
        }
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

        // print("AJT: stateHandler props = " + JSON.stringify(props));

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

        if (shakeRightHandAvatarId && props.rightHandRotation && props.rightHandPosition) {
            var otherHand = prevAvatarMap[shakeRightHandAvatarId].rightHand;
            var myHand = prevAvatarMap[MyAvatar.SELF_ID].rightHand;
            if (otherHand) {
                var avatarXform = new Xform(Quat.multiply(MyAvatar.orientation, QUAT_Y_180), MyAvatar.position);
                var targetXform = handTween(myHand, otherHand, rightHandDeltaXform);
                var localTargetXform = Xform.mul(avatarXform.inv(), targetXform);
                result.rightHandRotation = localTargetXform.rot;
                result.rightHandPosition = localTargetXform.pos;
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
        }
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
    } else {
        DebugDraw.removeMarker("leftHandController" + id);
    }

    if (SHOW_CONTROLLERS && data.rightHand.controllerValid) {
        DebugDraw.addMarker("rightHandController" + id, data.rightHand.controllerRot, data.rightHand.controllerPos, MAGENTA);
    } else {
        DebugDraw.removeMarker("rightHandController" + id);
    }

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
    var avatarIds = AvatarManager.getAvatarIdentifiers(); // AvatarManager.getAvatarsInRange(MyAvatar.position, AVATAR_RANGE);
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
