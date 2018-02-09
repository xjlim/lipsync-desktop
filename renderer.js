const fs = require('fs');
const path = require('path');
const cp = require('child_process');
const { dialog } = require('electron').remote;

const HEADERFILE = 'settings.h';
const ARDUINO_MAC_PATH = '/Applications/Arduino.app/Contents/MacOS/Arduino';
const ARDUINO_WINDOWS_PATH =
  'C:\\Program Files (x86)\\Arduino\\arduino_debug.exe';

const SETTINGS_CONSTANTS = {
  speed_counter: 'SPEED_COUNTER_SETTING',
  sip_threshold: 'SIP_THRESHOLD_SETTING',
  puff_threshold: 'PUFF_THRESHOLD_SETTING ',
  short_sippuff_duration: 'SIPPUFF_SECONDARY_DURATION_THRESHOLD_SETTING',
  very_long_sippuff_duration: 'SIPPUFF_TERTIARY_DURATION_THRESHOLD_SETTING',
  reversed: 'SIP_PUFF_SETTING'
};
const FIRMWAREFILE = 'LipSync_Firmware.ino';
const DEFAULT_CURSOR_SPEED = 4;
const DEFAULT_SIP_THRESHOLD = 2;
const DEFAULT_PUFF_THRESHOLD = 2;
const DEFAULT_SHORT_SIPPUFF_DURATION = 1;
const DEFAULT_VERY_LONG_SIPPUFF_DURATION = 5;
const DEFAULT_REVERSED = 0;
const initial_settings = {
  speed_counter: DEFAULT_CURSOR_SPEED,
  sip_threshold: DEFAULT_SIP_THRESHOLD,
  puff_threshold: DEFAULT_PUFF_THRESHOLD,
  short_sippuff_duration: DEFAULT_SHORT_SIPPUFF_DURATION,
  very_long_sippuff_duration: DEFAULT_VERY_LONG_SIPPUFF_DURATION,
  reversed: DEFAULT_REVERSED
};

const COUNTDOWN_TIME = 20;

let timer;
let flashFlag = false;
let flags = 0;
let success = false;
let points = [];
let start = false;
let saved_settings = Object.assign({}, initial_settings);

window.$ = window.jQuery = require('jquery');
$(function() {
  var speed_counter_handle = $('#speed_counter');
  $('#speed_counter_slider').slider({
    create: function() {
      speed_counter_handle.text($(this).slider('value'));
    },
    change: function(event, ui) {
      speed_counter_handle.text(ui.value);
    },
    slide: function(event, ui) {
      speed_counter_handle.text(ui.value);
    },
    min: 0,
    max: 8,
    value: 4
  });

  var sip_threshold_handle = $('#sip_threshold');
  $('#sip_threshold_slider').slider({
    create: function() {
      sip_threshold_handle.text($(this).slider('value'));
    },
    change: function(event, ui) {
      sip_threshold_handle.text(ui.value);
    },
    slide: function(event, ui) {
      sip_threshold_handle.text(ui.value);
    },
    min: 1,
    max: 4,
    value: 2
  });

  var puff_threshold_handle = $('#puff_threshold');
  $('#puff_threshold_slider').slider({
    create: function() {
      puff_threshold_handle.text($(this).slider('value'));
    },
    change: function(event, ui) {
      puff_threshold_handle.text(ui.value);
    },
    slide: function(event, ui) {
      puff_threshold_handle.text(ui.value);
    },
    min: 1,
    max: 4,
    value: 2
  });

  $('#sippuff_duration_slider').slider({
    range: true,
    min: 1,
    max: 10,
    values: [1, 5],
    change: function(event, ui) {
      if (ui.values[0] === ui.values[1]) {
        return false;
      }

      $('#short_sippuff_duration').text(ui.values[0]);
      $('#very_long_sippuff_duration').text(ui.values[1]);
    },
    slide: function(event, ui) {
      if (ui.values[0] === ui.values[1]) {
        return false;
      }

      $('#short_sippuff_duration').text(ui.values[0]);
      $('#very_long_sippuff_duration').text(ui.values[1]);
    },
    create: function() {
      $('#short_sippuff_duration').text(
        $('#sippuff_duration_slider').slider('values', 0)
      );
      $('#very_long_sippuff_duration').text(
        $('#sippuff_duration_slider').slider('values', 1)
      );
    }
  });
  reset(); // done after initializing the sliders
});

const flashButton = document.getElementById('flash-btn');
flashButton.addEventListener('click', function() {
  const output = serialize();
  fs.writeFileSync(path.resolve(__dirname, HEADERFILE), output, 'utf8');
  flashFlag = true;
  upload();
});

const resetButton = document.getElementById('reset-btn');
resetButton.addEventListener('click', function() {
  reset();
});

const revert = document.getElementById('revert');
revert.addEventListener('click', function() {
  closeModal();
  revertSettings();
});

const keep = document.getElementById('keep');
keep.addEventListener('click', function() {
  storeSettings();
  closeModal();
});

function rsip_puff() {
  var pre_st = document.getElementById('sip_threshold').value;
  var pre_pt = document.getElementById('puff_threshold').value;

  if (document.getElementById('reversed').checked == true) {
    document.getElementById('sip_threshold').value = pre_pt;
    document.getElementById('puff_threshold').value = pre_st;
  } else {
    document.getElementById('sip_threshold').value = pre_pt;
    document.getElementById('puff_threshold').value = pre_st;
  }
}

function getSettings() {
  $('#speed_counter_slider').slider('value');
  const speed_counter = $('#speed_counter_slider').slider('value');
  const sip_threshold = $('#sip_threshold_slider').slider('value') / 4;
  const puff_threshold = $('#puff_threshold_slider').slider('value') / 4;
  const short_sippuff_duration = $('#sippuff_duration_slider').slider(
    'values',
    0
  );
  const very_long_sippuff_duration = $('#sippuff_duration_slider').slider(
    'values',
    1
  );
  const reversed = document.getElementById('reversed').checked ? 0 : 1;
  return {
    speed_counter,
    sip_threshold,
    puff_threshold,
    short_sippuff_duration,
    very_long_sippuff_duration,
    reversed
  };
}

function setSettings({
  speed_counter,
  sip_threshold,
  puff_threshold,
  short_sippuff_duration,
  very_long_sippuff_duration,
  reversed
}) {
  $('#speed_counter_slider').slider('value', speed_counter);
  $('#sip_threshold_slider').slider('value', sip_threshold);
  $('#puff_threshold_slider').slider('value', puff_threshold);
  $('#sippuff_duration_slider').slider('values', 0, short_sippuff_duration);
  $('#sippuff_duration_slider').slider('values', 1, very_long_sippuff_duration);
  document.getElementById('reversed').checked = reversed;
}

function serialize() {
  const settings = getSettings();
  return Object.keys(settings)
    .map(key => `#define ${SETTINGS_CONSTANTS[key]} ${settings[key]}`)
    .join('\n');
}

function upload() {
  const settingsContainer = document.getElementById('settings-container');
  const uploadStatus = document.getElementById('upload-status');
  settingsContainer.classList.add('hidden');
  uploadStatus.classList.remove('hidden');
  // run this after DOM manipulation
  setTimeout(() => {
    const arduinoPath =
      process.platform === 'win32' ? ARDUINO_WINDOWS_PATH : ARDUINO_MAC_PATH;
    const output = cp.spawnSync(
      arduinoPath,
      ['--upload', path.resolve(__dirname, FIRMWAREFILE)],
      {
        encoding: 'utf8'
      }
    );
    console.log(output);
    const result = output.stdout;
    console.log('[Upload]', result || 'Upload failed');
    if (output.status === 1) {
      dialog.showErrorBox('Settings', 'Upload failed');
    } else if (flashFlag) {
      triggerFailSafe();
    }
    settingsContainer.classList.remove('hidden');
    uploadStatus.classList.add('hidden');
  }, 0);
}

function triggerFailSafe() {
  document.getElementById('flash-modal').style.display = 'block';
  let count = COUNTDOWN_TIME;
  document.getElementById(
    'flash-countdown'
  ).innerHTML = `Reverting to previous settings in ${count} seconds`;
  timer = setInterval(() => {
    if (count == 0) {
      closeModal();
      revertSettings();
    } else {
      document.getElementById(
        'flash-countdown'
      ).innerHTML = `Reverting to previous settings in ${--count} seconds`;
    }
  }, 1000);
}

function closeModal() {
  clearInterval(timer);
  document.getElementById('flash-modal').style.display = 'none';
}

function storeSettings() {
  const settings = getSettings();
  Object.keys(settings).map(key => (saved_settings[key] = settings[key]));
}

function revertSettings() {
  setSettings(saved_settings);
  const output = serialize();
  fs.writeFileSync(path.resolve(__dirname, HEADERFILE), output, 'utf8');
  flashFlag = false;
  upload();
}

function reset() {
  setSettings(initial_settings);
}
