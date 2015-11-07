(function e(t,n,r){function s(o,u){if(!n[o]){if(!t[o]){var a=typeof require=="function"&&require;if(!u&&a)return a(o,!0);if(i)return i(o,!0);var f=new Error("Cannot find module '"+o+"'");throw f.code="MODULE_NOT_FOUND",f}var l=n[o]={exports:{}};t[o][0].call(l.exports,function(e){var n=t[o][1][e];return s(n?n:e)},l,l.exports,e,t,n,r)}return n[o].exports}var i=typeof require=="function"&&require;for(var o=0;o<r.length;o++)s(r[o]);return s})({1:[function(require,module,exports){
// ctor
var Click = function () {
    // outputs
    this.outputs = {};
    this.outputs.click = [];

    var self = this;

    // TODO: i cant figure out how to make mouse work?!?
    Controller.keyPressEvent.connect(function (event) {
        if (event.text === 'g') {
            self.onClick(event);
        }
    });
};

Click.prototype.start = function () {
    // no-op
};

Click.prototype.onClick = function (event) {
    // TODO: filter based on event type.
    var clickObservers = this.outputs.click;
    var i, l = clickObservers.length;
    for (i = 0; i < l; i++) {
        clickObservers[i](event);
    }
};

// exports
exports.Click = Click;

},{}],2:[function(require,module,exports){
// ctor
var Constant = function (value) {
    this.value = value;

    // TODO: abstract this into base prototype.
    this.outputs = {};
    this.outputs.value = [];
};

Constant.prototype.start = function () {
    // TODO: abstract this into emit prototype
    var valueObservers = this.outputs.value;
    var i, l = valueObservers.length;
    for (i = 0; i < l; i++) {
        valueObservers[i](this.value);
    }
};

// exports
exports.Constant = Constant;

},{}],3:[function(require,module,exports){
// ctor
var EntityProperty = function (id) {
    this.id = id;
    this.dirty = 0;
    var self = this;

    // inputs
    this.inputs = {};
    this.inputs.position = function (position) {
        self.position = position;
        self.dirty |= 0x01;
    };
    this.inputs.orientation = function (orientation) {
        self.orientation = orientation;
        self.dirty |= 0x02;
    };
    this.inputs.color = function (color) {
        self.color = color;
        self.dirty |= 0x04;
    };
    this.inputs.trigger$ = function () {
        var props = {};
        if (self.dirty & 0x01) {
            props.position = self.position;
        } else if (self.dirty & 0x02) {
            props.orientation = self.orientation;
        } else if (self.dirty & 0x04) {
            props.color = self.color;
        }
        Entities.editEntity(self.id, props);
        self.dirty = 0;
    };
};

EntityProperty.prototype.start = function () {
    // no-op
};

// exports
exports.EntityProperty = EntityProperty;

},{}],4:[function(require,module,exports){
var FlowGraph = {};
var Constant = require("./constant.js").Constant;
var EntityProperty = require("./entity-property.js").EntityProperty;
var Click = require("./click.js").Click;

function isArray (a) {
    return Object.prototype.toString.call(a) === '[object Array]';
}

function isFunction (f) {
    var getType = {};
    return f && getType.toString.call(f) === '[object Function]';
}

// connect two nodes
FlowGraph.connect = function (aNode, aProp, bNode, bProp) {
    if (aNode.outputs && aNode.outputs[aProp] && isArray(aNode.outputs[aProp])) {
        if (bNode.inputs && bNode.inputs[bProp] && isFunction(bNode.inputs[bProp])) {
            aNode.outputs[aProp].push(bNode.inputs[bProp]);
        } else {
            throw "FlowGraph.connect() Error with second node, aProp = " + aProp + ", bProp = " + bProp;
        }
    } else {
        throw "FlowGraph.connect() Error with first node, aProp = " + aProp + ", bProp = " + bProp;
    }
};

// exports
exports.FlowGraph = FlowGraph;
exports.FlowGraph.Constant = Constant;
exports.FlowGraph.EntityProperty = EntityProperty;
exports.FlowGraph.Click = Click;

},{"./click.js":1,"./constant.js":2,"./entity-property.js":3}],5:[function(require,module,exports){
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



},{"./flow-graph.js":4}]},{},[1,2,3,4,5]);
