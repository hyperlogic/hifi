// Spawns a booth  and a bunch of special sound cartridges that a user can grab and place close to them.
// Once cartridges are inside sphere, they will listen for Entities.callEntityMethod from client and play their sound accordingly
// cartriges should be as stateless as possible
//Eventually separate cartridge spawner into separate file
Script.include("../libraries/utils.js");
var orientation = Camera.getOrientation();
orientation = Quat.safeEulerAngles(orientation);
orientation.x = 0;
orientation = Quat.fromVec3Degrees(orientation);
var cartridgePosition = Vec3.sum(MyAvatar.getHeadPosition(), Vec3.multiply(1.1, Quat.getFront(orientation)));

var SPHERE_RADIUS = 1;
var UPDATE_TIME = 100;

var SCRIPT_URL = Script.resolvePath("soundCartridgeEntityScript.js?v1" + Math.random());
var cartridge = Entities.addEntity({
    type: "Box",
    name: "Sound Cartridge",
    script: SCRIPT_URL,
    position: cartridgePosition,
    dynamic: true,
    // collidesWith: "",
    dimensions: {x: 0.1, y: 0.1, z: 0.1},
    damping: 1,
    angularDamping: 1,
    color: {red: 20, green: 20, blue: 200},
    userData: JSON.stringify({
        soundURL: "https://s3-us-west-1.amazonaws.com/hifi-content/eric/Sounds/hackathonSounds/synth/chillstep-synth.wav"
    })
});

var spherePosition = MyAvatar.position;
var sphereOverlay = Overlays.addOverlay('sphere', {
                size: SPHERE_RADIUS * 2,
                position: MyAvatar.position,
                color: {
                    red: 200,
                    green: 10,
                    blue: 10
                },
                alpha: 0.2,
                solid: true,
                visible: true
            })

var activeCartidge = "";
function update() {
    // Get all entities in search sphere and for the ones that have soundURLs and are cartridges, call their entityMethod with appropriate data
    //DOing it with entity scripts makes it more scalable to multiuser since a friend can come grab cartriges from you
    // Folks can create thier own loops and share them amongst each other.
    // True multiuser DJing
    var entities = Entities.findEntities(spherePosition, SPHERE_RADIUS);
    entities.forEach(function(entity) {
        var name = Entities.getEntityProperties(entity, "name").name;
        var userData = getEntityUserData(entity);

        if(name === "Sound Cartridge" && userData.soundURL && !entitiesEqual(entity, activeCartidge)) {
            //We have a cartridge- play it

            Entities.callEntityMethod(entity, "playSound");
            activeCartidge = entity;
        }
    });


}

function entitiesEqual(entityA, entityB) {
    if (!entityA || !entityB) {
        print("ONE OR BOTH OF THESE ENTITIES ARE UNDEFINED");
        return false;
    }
    return JSON.stringify(entityA) === JSON.stringify(entityB);
}

function cleanup() {
    Overlays.deleteOverlay(sphereOverlay);
    Entities.deleteEntity(cartridge);
}


Script.setInterval(update, UPDATE_TIME);
Script.scriptEnding.connect(cleanup);