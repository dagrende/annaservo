var params = getUrlParams(document.location.href);
console.log(params);

d3json('/stop');
function getAbsUrl(relativeUrl) {
  return (params['server'] || '') + relativeUrl;
}

var stepHandler = stepsSimulator();

// create html ui
var body = d3.select('body');
var titleRow = body.append('div').attr('class', 'titleRow');
var timeRow = body.append('div').attr('class', 'timeRow');
timeRow.append('span').text('Time to this step:');
var timeToStep = timeRow.append('input').on('change', function(d) {
  stepHandler.getCurrentStep().timeToStep = parseInt(this.value);
  sendStep(stepHandler.currentStepIndex, true);
});
timeRow.append('span').text('s');
var sliders = body.append('div').attr('class', 'sliders');
renderSliders(sliders);
var buttonRow = body.append('div').attr('class', 'buttonRow');
buttonRow.append('button').attr({id: 'prevButton', title: 'Previous step'}).on('click', stepHandler.toPreviousStep).text('<');
buttonRow.append('button').attr({id: 'nextButton', title: 'Next step'}).on('click', stepHandler.toNextStep).text('>');
buttonRow.append('button').attr({id: 'addButton', title: 'Add'}).on('click', stepHandler.addStep).text('+');
buttonRow.append('button').attr({id: 'deleteButton', title: 'Delete'}).on('click', stepHandler.deleteStep).text('-');
buttonRow.append('button').attr({id: 'startButton', title: 'Start'}).on('click', function() {d3json('/start')}).text('start');
buttonRow.append('button').attr({id: 'stopButton', title: 'Stop'}).on('click', function() {d3json('/stop')}).text('stop');
buttonRow.append('button').attr({id: 'saveButton', title: 'Save'}).on('click', function() {d3json('/save')}).text('save');

// load program from board
d3.json(getAbsUrl('/steps'), function(error, json) {
  if (error) {
    console.log(error);
  } else {
    console.log(json);
    stepHandler.setSteps(json);
  }
});

function renderSliders() {
  titleRow.text('Step ' + stepHandler.currentStepIndex + ' of ' + (stepHandler.steps.length - 1));
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
    sendStep(stepHandler.currentStepIndex, true);
  });
}

function d3json(relativeUrl) {
  d3.json(getAbsUrl(relativeUrl),
    function(error, json) {
      if (error) {
        console.log(error);
      } else {
        console.log(json);
      }
    }
  );
}

// tell controller to move servos to step at index, and to save the step
function sendStep(stepIndex, isSet) {
  console.log('sendStep(' + stepIndex + ')');
  var step = stepHandler.steps[stepIndex];
  var stepString = getStepString(step);
  d3json('/move/' + stepString);
  if (isSet && stepIndex > 0) {
    d3json('/set/' + (stepIndex - 1) + '/' + stepString);
  }
}

function getStepString(step) {
  return step.timeToStep + ',' + step.positions.join(',');
}

stepHandler.on('change', function() {
  renderSliders();
  sendStep(stepHandler.currentStepIndex, false);
});

stepHandler.on('move', function() {
  renderSliders();
  sendStep(stepHandler.currentStepIndex, false);
});

// represents the servo controller
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
      fireEvent('move');
    },
    toNextStep: function() {
      if (state.currentStepIndex < state.steps.length - 1) state.currentStepIndex++;
      fireEvent('move');
    },
    addStep: function() {
      var step = state.steps[stepHandler.currentStepIndex];
      state.steps.splice(state.currentStepIndex + 1, 0, {timeToStep: step.timeToStep, positions: step.positions.slice()});
      state.currentStepIndex++;
      d3json('/add/' + (state.currentStepIndex - 1) + '/' + getStepString(state.steps[state.currentStepIndex]));
      fireEvent('change');
    },
    deleteStep: function() {
      if (state.steps.length > 1 && state.currentStepIndex > 0) {
        state.steps.splice(state.currentStepIndex, 1);
        d3json('/remove/' + (state.currentStepIndex - 1) + '/1');
        if (state.currentStepIndex >= state.steps.length) {
          state.currentStepIndex = state.steps.length - 1;
        }
        fireEvent('change');
      }
    },
    setSteps: function(steps) {
      if (steps) {
        console.log('setSteps(' + JSON.stringify(steps, null, '  ') + ')');
        state.steps = steps;
        state.currentStepIndex = 0;
        if (state.steps.length == 0) {
          // add a first step
          state.steps.push({timeToStep: 0, positions: [90, 90, 90, 90, 90, 90]});
        } else {
          // duplicate first step to get the experiment step
          state.steps.splice(0, 0, {timeToStep: state.steps[0].timeToStep, positions: state.steps[0].positions.slice(0)});
        }
        fireEvent('change');
      }
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

function getUrlParams(uri) {
  var urlParams = {};
  if (window.location.search) {
    var match,
      pl     = /\+/g,  // Regex for replacing addition symbol with a space
      search = /([^&=]+)=?([^&]*)/g,
      decode = function (s) { return decodeURIComponent(s.replace(pl, " ")); },
      query  = window.location.search.substring(1);
    while (match = search.exec(query))
       urlParams[decode(match[1])] = decode(match[2]);
  }
  return urlParams;
}
