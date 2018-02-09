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
const DEFAULT_SHORT_SIPPUFF_DURATION = 2;
const DEFAULT_VERY_LONG_SIPPUFF_DURATION = 6;
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
let saved_settings = initial_settings;

reset();

window.$ = window.jQuery = require('jquery');
$(function() {
  $('#speed_counter').slider();
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
  console.log('reversed');
  closeModal();
  revertSettings();
});

const keep = document.getElementById('keep');
keep.addEventListener('click', function() {
  storeSettings();
  closeModal();
});

document.getElementById('reversed').addEventListener('change', rsip_puff);
document
  .getElementById('speed_counter')
  .addEventListener('change', update_value);
document
  .getElementById('sip_threshold')
  .addEventListener('change', update_value);
document
  .getElementById('puff_threshold')
  .addEventListener('change', update_value);
document
  .getElementById('short_sippuff_duration')
  .addEventListener('change', update_value);
document
  .getElementById('very_long_sippuff_duration')
  .addEventListener('change', update_value);

function updateDOM() {
  const settings = getSettings();
  Object.keys(settings).map(key => {
    if (key === 'reversed') {
      return;
    }
    if (key === 'sip_threshold' || key === 'puff_threshold') {
      document.getElementById(`${key}_label`).innerHTML = settings[key] * 4;
    } else {
      document.getElementById(`${key}_label`).innerHTML = settings[key];
    }
  });
}

function update_value(event) {
  const setting = event.target.id;
  document.getElementById(
    `${setting}_label`
  ).innerHTML = document.getElementById(setting).value;
}

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
  updateDOM();
}

function getSettings() {
  const speed_counter = document.getElementById('speed_counter').value - 1;
  const sip_threshold = document.getElementById('sip_threshold').value / 4;
  const puff_threshold = document.getElementById('puff_threshold').value / 4;
  const short_sippuff_duration = document.getElementById(
    'short_sippuff_duration'
  ).value;
  const very_long_sippuff_duration = document.getElementById(
    'very_long_sippuff_duration'
  ).value;
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
  document.getElementById('speed_counter').value = speed_counter;
  document.getElementById('sip_threshold').value = sip_threshold;
  document.getElementById('puff_threshold').value = puff_threshold;
  document.getElementById(
    'short_sippuff_duration'
  ).value = short_sippuff_duration;
  document.getElementById(
    'very_long_sippuff_duration'
  ).value = very_long_sippuff_duration;
  document.getElementById('reversed').value = reversed;
}

function serialize() {
  const settings = getSettings();
  return Object.keys(settings)
    .map(key => `#define ${SETTINGS_CONSTANTS[key]} ${settings[key]}`)
    .join('\n');
}

function upload() {
  const blinkContainer = document.getElementById('blink-container');
  const uploadStatus = document.getElementById('upload-status');
  blinkContainer.classList.add('hidden');
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
      dialog.showErrorBox('Blink', 'Upload failed');
    } else if (flashFlag) {
      triggerFailSafe();
    }
    blinkContainer.classList.remove('hidden');
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
  updateDOM();
  const output = serialize();
  fs.writeFileSync(path.resolve(__dirname, HEADERFILE), output, 'utf8');
  flashFlag = false;
  upload();
}
function reset() {
  setSettings(initial_settings);
  updateDOM();
}
