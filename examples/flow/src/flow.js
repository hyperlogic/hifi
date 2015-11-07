FlowGraph = require("./flow-graph.js").FlowGraph;

// create a box entity
var spawnPoint = Vec3.sum(MyAvatar.position,
                          Vec3.multiply(3, Quat.getFront(Camera.getOrientation())));
var red = { red: 255, green: 0, blue: 0 };

// ctor
var App = function () {
    Script.scriptEnding.connect(function () {
        this.shutDown();
    });

    this.box = Entities.addEntity({ type: "Box",
                                    position: spawnPoint,
                                    dimentions: { x: 1, y: 1, z: 1 },
                                    color: { red: 100, green: 100, blue: 255 },
                                    gravity: { x: 0, y: 0, z: 0 },
                                    visible: true,
                                    locked: false,
                                    lifetime: 6000 });
    this.nodes = [];
    this.redNode = new FlowGraph.Constant(red);
    this.nodes.push(this.redNode);
    this.entityNode = new FlowGraph.EntityProperty(this.box);
    this.nodes.push(this.entityNode);
    this.clickNode = new FlowGraph.Click();
    this.nodes.push(this.clickNode);

    FlowGraph.connect(this.redNode, "value", this.entityNode, "color");
    FlowGraph.connect(this.clickNode, "click", this.entityNode, "trigger$");

    var i, l = this.nodes.length;
    for (i = 0; i < l; i++) {
        this.nodes[i].start();
    }
};

App.prototype.shutDown = function () {
    Entities.deleteEntity(this.box);
};

var app = new App();


