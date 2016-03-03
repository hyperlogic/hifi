Script.include("../libraries/utils.js");

var orientation = Camera.getOrientation();
orientation = Quat.safeEulerAngles(orientation);
orientation.x = 0;
orientation = Quat.fromVec3Degrees(orientation);
var cartridgeBasePosition = Vec3.sum(MyAvatar.getHeadPosition(), Vec3.multiply(1.2, Quat.getFront(orientation)));
var cartridges = [];
spawnCartridges();

function spawnCartridges() {
    var SCRIPT_URL = Script.resolvePath("soundCartridgeEntityScript.js?v1" + Math.random());
    var cartridgeProps = {
        type: "Box",
        name: "Sound Cartridge",
        position: cartridgeBasePosition,
        script: SCRIPT_URL,
        dynamic: true,
        dimensions: {
            x: 0.1,
            y: 0.1,
            z: 0.1
        },
        damping: 1,
        angularDamping: 1,
        userData: JSON.stringify({
            soundURL: "https://s3-us-west-1.amazonaws.com/hifi-content/eric/Sounds/hackathonSounds/synth/chillstep-synth.wav"
        })
    };

    for (var i = 0; i < 2; i++) {
        cartridgeProps.position.x = cartridgeBasePosition.x + randFloat(-0.7, 0.7);
        cartridgeProps.color = {
            red: randFloat(0, 250),
            green: randFloat(0, 250),
            blue: randFloat(0, 250)
        };
        var cartridge = Entities.addEntity(cartridgeProps);
        cartridges.push(cartridge);
    }
}


function cleanup() {
    cartridges.forEach(function(cartridge) {
        Entities.deleteEntity(cartridge);
    });
}

Script.scriptEnding.connect(cleanup);