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
