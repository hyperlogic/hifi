// Spawns a booth  and a bunch of special sound cartridges that a user can grab and place close to them.
// Once cartridges are inside sphere, they will listen for Entities.callEntityMethod from client and play their sound accordingly
// cartriges should be as stateless as possible
Script.include("../libraries/utils.js");
var orientation = Camera.getOrientation();
orientation = Quat.safeEulerAngles(orientation);
orientation.x = 0;
orientation = Quat.fromVec3Degrees(orientation);

var SPHERE_RADIUS = 1;
var UPDATE_TIME = 5000;

var activeCartridges = [];


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


function update() {
    // Get all entities in search sphere and for the ones that have soundURLs and are cartridges, call their entityMethod with appropriate data
    //DOing it with entity scripts makes it more scalable to multiuser since a friend can come grab cartriges from you
    // Folks can create thier own loops and share them amongst each other.
    // True multiuser DJing
    var entities = Entities.findEntities(spherePosition, SPHERE_RADIUS);
    entities.forEach(function(entity) {
        var name = Entities.getEntityProperties(entity, "name").name;
        var userData = getEntityUserData(entity);

        if (name === "Sound Cartridge" && userData.soundURL && !cartridgeInActiveList(entity)) {
            //We have a cartridge- play it

            Entities.callEntityMethod(entity, "playSound");
            print("PLAY SOUND")
            activeCartridges.push(entity);
        }
    });


}

function cartridgeInActiveList(cartridgeToCheck) {
    // Check to see if specified cartrige is in this active list
    for (var i = 0; i < activeCartridges.length; i++) {
        if (entitiesEqual(activeCartridges[i], cartridgeToCheck)) {
            return true;
        }
    }
    return false;}

function entitiesEqual(entityA, entityB) {
    if (!entityA || !entityB) {
        print("ONE OR BOTH OF THESE ENTITIES ARE UNDEFINED");
        return false;
    }
    var isEqual = JSON.stringify(entityA) === JSON.stringify(entityB) ? true : false;
    return isEqual;
}

function cleanup() {
    Overlays.deleteOverlay(sphereOverlay);
}


Script.setInterval(update, UPDATE_TIME);
Script.scriptEnding.connect(cleanup);