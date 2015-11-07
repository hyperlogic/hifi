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
