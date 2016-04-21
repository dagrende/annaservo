function getAbsUrl(relativeUrl) {
  return "http://192.168.0.20" + relativeUrl;
}

var stepHandler = stepsSimulator();

var body = d3.select('body');
var titleRow = body.append('div').attr('class', 'titleRow');
var timeRow = body.append('div').attr('class', 'timeRow');
timeRow.append('span').text('Time to this step:');
var timeToStep = timeRow.append('input').on('change', function(d) {
  stepHandler.getCurrentStep().timeToStep = parseInt(this.value);
});
timeRow.append('span').text('s');
var sliders = body.append('div').attr('class', 'sliders');
renderSliders(sliders);
var buttonRow = body.append('div').attr('class', 'buttonRow');
buttonRow.append('button').attr({id: 'prevButton', title: 'Previous step'}).on('click', stepHandler.toPreviousStep).text('<');
buttonRow.append('button').attr({id: 'addButton', title: 'Add'}).on('click', stepHandler.addStep).text('+');
buttonRow.append('button').attr({id: 'deleteButton', title: 'Delete'}).on('click', stepHandler.deleteStep).text('-');
buttonRow.append('button').attr({id: 'nextButton', title: 'Next step'}).on('click', stepHandler.toNextStep).text('>');

d3.json(getAbsUrl('/steps'), function(error, json) {
  if (error) {
    console.log(error);
  } else {
    console.log(json);
    stepHandler.setSteps(json);
  }
});

function renderSliders() {
  titleRow.text('Step ' + (stepHandler.currentStepIndex + 1) + ' of ' + stepHandler.steps.length);
  timeToStep.node().value = stepHandler.getCurrentStep().timeToStep;
  var slider = sliders.selectAll('input').data(stepHandler.getCurrentStep().positions);
  slider.enter().append('input')
  slider.attr({
    type: 'range',
    min: 0,
    max: 180
  })
  .each(function(d, i) {this.value = d})
  .on('change', function(d, i) {
    stepHandler.getCurrentStep().positions[i] = parseInt(this.value);
    sendStep(stepHandler.getCurrentStep());
  });
}

function sendStep(step) {
  d3.json(getAbsUrl('/set/' + step.timeToStep + ',' + step.positions.join(',')),
    function(error, json) {
      if (error) {
        console.log(error);
      } else {
        console.log(json);
      }
    }
  );
}

stepHandler.on('step', function() {
  renderSliders();
  sendStep(stepHandler.getCurrentStep());
});

function stepsSimulator() {
  function fireEvent(type) {
    var listeners = eventListeners[type];
    if (listeners) {
      for (i in listeners) {
        listeners[i](arguments);
      }
    }
  }
  var eventListeners = {};
  var state = {
    steps: [
      {timeToStep: 0, positions: [90, 90, 90, 90, 90, 90]}
    ],
    currentStepIndex: 0,
    getCurrentStep: function() {return state.steps[stepHandler.currentStepIndex]},
    toPreviousStep: function() {
      if (state.currentStepIndex > 0) state.currentStepIndex--;
      fireEvent('step');
    },
    toNextStep: function() {
      if (state.currentStepIndex < state.steps.length - 1) state.currentStepIndex++;
      fireEvent('step');
    },
    addStep: function() {
      var step = state.steps[stepHandler.currentStepIndex];
      state.steps.splice(state.currentStepIndex + 1, 0, {timeToStep: step.timeToStep, positions: step.positions.slice()});
      state.currentStepIndex++;
      fireEvent('step');
    },
    deleteStep: function() {
      if (state.steps.length > 1) {
        state.steps.splice(state.currentStepIndex, 1);
        if (state.currentStepIndex >= state.steps.length) {
          state.currentStepIndex = state.steps.length - 1;
        }
        fireEvent('step');
      }
    },
    setSteps: function(steps) {
      if (steps) {
        state.steps = steps;
        state.currentStepIndex = 0;
        if (state.steps.length == 0) {
          state.steps.push({timeToStep: 0, positions: [90, 90, 90, 90, 90, 90]});
        }
        fireEvent('step');
      }
    },
    save: function(values) {
        console.log('save',values);
    },
    on: function(type, listener) {
      var listeners = eventListeners[type];
      if (listeners) {
        listeners.push(listener)
      } else {
        eventListeners[type] = [listener]
      }
    }
  };
  return state;
}
