//
// elbowIkDebug.js
//
/* global Xform */
/* eslint max-len: ["error", 1024, 4] */

(function() { // BEGIN LOCAL_SCOPE

    Script.include("/~/system/libraries/Xform.js");

    var USE_NEW_MODEL = false;
    var SOLUTION_SOURCE = 0;  // 0 = RelaxToUnderPoses, RelaxToLimitCenterPoses, PreviousSolution, UnderPoses, LimitCenterPoses.
    var USE_POLE_VECTOR = true;
    var USE_HMD_RECENTER = true;

    //
    // tablet app boiler plate
    //

    var TABLET_BUTTON_NAME = "ELBOWDBG";
    var HTML_URL = "https://s3.amazonaws.com/hifi-public/tony/html/elbowikdbg.html?3";

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
            var values = [{name: "use-new-model", val: "on", checked: USE_NEW_MODEL},
                          {name: "solution-source", val: SOLUTION_SOURCE, checked: false},
                          {name: "use-pole-vector", val: "on", checked: USE_POLE_VECTOR},
                          {name: "use-hmd-recenter", val: "on", checked: USE_HMD_RECENTER}];
            tablet.emitScriptEvent(JSON.stringify(values));
        } else if (msg.name === "solution-source") {
            SOLUTION_SOURCE = parseInt(msg.val, 10);
        } else if (msg.name === "use-new-model") {
            USE_NEW_MODEL = msg.checked;

            // AJT: hack re-purpose this broken flag.
            MyAvatar.shouldRenderLocally = USE_NEW_MODEL;

        } else if (msg.name === "use-pole-vector") {
            USE_POLE_VECTOR = msg.checked;
        } else if (msg.name === "use-hmd-recenter") {
            USE_HMD_RECENTER = msg.checked;
            MyAvatar.hmdLeanRecenterEnabled = USE_HMD_RECENTER;
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

    function init() {

        MyAvatar.hmdLeanRecenterEnabled = USE_HMD_RECENTER;

        // AJT: hack re-purpose this broken flag.
        MyAvatar.shouldRenderLocally = USE_NEW_MODEL;

        var ANIM_VARS = [
            "leftHandPoleVectorEnabled",
            "rightHandPoleVectorEnabled",
            "solutionSource"
        ];

        animationHandlerId = MyAvatar.addAnimationStateHandler(function (props) {

            var result = {};
            if (props.leftHandPoleVectorEnabled) {
                result.leftHandPoleVectorEnabled = props.leftHandPoleVectorEnabled;
            }
            if (props.rightHandPoleVectorEnabled) {
                result.rightHandPoleVectorEnabled = props.rightHandPoleVectorEnabled;
            }
            if (props.solutionSource) {
                result.solutionSource = props.solutionSource;
            }

            result.solutionSource = SOLUTION_SOURCE;
            result.leftHandPoleVectorEnabled = USE_POLE_VECTOR;
            result.rightHandPoleVectorEnabled = USE_POLE_VECTOR;

            return result;
        }, ANIM_VARS);



        Script.update.connect(update);
        Script.scriptEnding.connect(shutdown);
    }

    function update(dt) {
        ;
    }

    function shutdown() {
        MyAvatar.removeAnimationStateHandler(animationHandlerId);
        shutdownTabletApp();
    }

    init();

}()); // END LOCAL_SCOPE
