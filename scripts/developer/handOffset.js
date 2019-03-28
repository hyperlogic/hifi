var leftHandRotation = {x: -83, y: -36, z: -81};
var leftHandTranslation = {x: -0.05, y: -0.0, z: 0.14};
var rightHandRotation = {x: -83, y: 36, z: 81};
var rightHandTranslation = {x: 0.05, y: 0, z: 0.14};

//
// tablet app boiler plate
//

var TABLET_BUTTON_NAME = "HANDOFF";
var HTML_URL = "https://s3.amazonaws.com/hifi-public/tony/html/handOffset.html";

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
            shownChanged(true);
        }
        shown = true;
    } else {
        tabletButton.editProperties({isActive: false});
        if (shown) {
            // disconnect from event bridge
            tablet.webEventReceived.disconnect(onWebEventReceived);
            shownChanged(false);
        }
        shown = false;
    }
}

function onWebEventReceived(msg) {
    if (msg.name === "init-complete") {
        var values = [
            {name: "left-hand-x-rotation", val: leftHandRotation.x, checked: false},
            {name: "left-hand-y-rotation", val: leftHandRotation.y, checked: false},
            {name: "left-hand-z-rotation", val: leftHandRotation.z, checked: false},
            {name: "left-hand-x-translation", val: leftHandTranslation.x, checked: false},
            {name: "left-hand-y-translation", val: leftHandTranslation.y, checked: false},
            {name: "left-hand-z-translation", val: leftHandTranslation.z, checked: false},
            {name: "right-hand-x-rotation", val: rightHandRotation.x, checked: false},
            {name: "right-hand-y-rotation", val: rightHandRotation.y, checked: false},
            {name: "right-hand-z-rotation", val: rightHandRotation.z, checked: false},
            {name: "right-hand-x-translation", val: rightHandTranslation.x, checked: false},
            {name: "right-hand-y-translation", val: rightHandTranslation.y, checked: false},
            {name: "right-hand-z-translation", val: rightHandTranslation.z, checked: false}
        ];
        tablet.emitScriptEvent(JSON.stringify(values));
    } else if (msg.name === "left-hand-x-rotation") {
        leftHandRotation.x = Number(msg.val);
    } else if (msg.name === "left-hand-y-rotation") {
        leftHandRotation.y = Number(msg.val);
    } else if (msg.name === "left-hand-z-rotation") {
        leftHandRotation.z = Number(msg.val);
    } else if (msg.name === "left-hand-x-translation") {
        leftHandTranslation.x = Number(msg.val);
    } else if (msg.name === "left-hand-y-translation") {
        leftHandTranslation.y = Number(msg.val);
    } else if (msg.name === "left-hand-z-translation") {
        leftHandTranslation.z = Number(msg.val);
    } else if (msg.name === "right-hand-x-rotation") {
        rightHandRotation.x = Number(msg.val);
    } else if (msg.name === "right-hand-y-rotation") {
        rightHandRotation.y = Number(msg.val);
    } else if (msg.name === "right-hand-z-rotation") {
        rightHandRotation.z = Number(msg.val);
    } else if (msg.name === "right-hand-x-translation") {
        rightHandTranslation.x = Number(msg.val);
    } else if (msg.name === "right-hand-y-translation") {
        rightHandTranslation.y = Number(msg.val);
    } else if (msg.name === "right-hand-z-translation") {
        rightHandTranslation.z = Number(msg.val);
    }

    var device = Controller.findDevice("Vive");
    Controller.setControllerOffset(device, Quat.fromVec3Degrees(leftHandRotation), leftHandTranslation, 0);
    Controller.setControllerOffset(device, Quat.fromVec3Degrees(rightHandRotation), rightHandTranslation, 1);

    print("AJT: leftHand, rot = (" + leftHandRotation.x + ", " + leftHandRotation.y + ", " + leftHandRotation.z + "), trans = (" + leftHandTranslation.x + ", " + leftHandTranslation.y + ", " + leftHandTranslation.z + ")");
    print("AJT: rightHand, rot = (" + rightHandRotation.x + ", " + rightHandRotation.y + ", " + rightHandRotation.z + "), trans = (" + rightHandTranslation.x + ", " + rightHandTranslation.y + ", " + rightHandTranslation.z + ")");
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

function shownChanged(newShown) {
    if (newShown) {
        var device = Controller.findDevice("Vive");
        Controller.setControllerOffset(device, Quat.fromVec3Degrees(leftHandRotation), leftHandTranslation, 0);
        Controller.setControllerOffset(device, Quat.fromVec3Degrees(rightHandRotation), rightHandTranslation, 1);
    } else {
        // disable offset
        var device = Controller.findDevice("Vive");
        Controller.setControllerOffset(device, {x: 0, y: 0, z: 0, w: 1}, {x: 0, y: 0, z: 0}, 0);
        Controller.setControllerOffset(device, {x: 0, y: 0, z: 0, w: 1}, {x: 0, y: 0, z: 0}, 1);
    }
}

Script.scriptEnding.connect(function() {
    tablet.removeButton(tabletButton);
});

