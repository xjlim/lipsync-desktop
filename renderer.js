const fs = require("fs");
const cp = require("child_process");

const HEADERFILE = "settings.h";
const ARDUINO_MAC_PATH = "/Applications/Arduino.app/Contents/MacOS/Arduino";
const ARDUINO_WINDOWS_PATH =
  "C:\\Program Files (x86)\\Arduino\\arduino_debug.exe";
const SETTINGS_CONSTANTS = {
  interval: "INTERVAL"
};
const FIRMWAREFILE = "Blink.ino";
const DEFAULT_INTERVAL = 1000;

let timer;
let settings = {
  interval: DEFAULT_INTERVAL
};
let flashFlag = false;

// start with default
reset();

const flashButton = document.getElementById("flash-btn");
flashButton.addEventListener("click", function() {
  const interval = document.getElementById("interval-input").value;
  const output = serialize({ interval });
  fs.writeFileSync(HEADERFILE, output, "utf8");
  flashFlag = true;
  upload();
});

const resetButton = document.getElementById("reset-btn");
resetButton.addEventListener("click", function() {
  reset();
});

const revert = document.getElementById("revert");
revert.addEventListener("click", function() {
  closeModal();
  revertSettings();
});

const keep = document.getElementById("keep");
keep.addEventListener("click", function() {
  storeSettings();
  closeModal();
});

function serialize(settings) {
  return Object.keys(settings)
    .map(key => `#define ${SETTINGS_CONSTANTS[key]} ${settings[key]}`)
    .join("");
}

function upload() {
  const blinkContainer = document.getElementById("blink-container");
  const uploadStatus = document.getElementById("upload-status");
  blinkContainer.classList.add("hidden");
  uploadStatus.classList.remove("hidden");
  // run this after DOM manipulation
  setTimeout(() => {
    const arduinoPath =
      process.platform === "win32" ? ARDUINO_WINDOWS_PATH : ARDUINO_MAC_PATH;
    const output = cp.spawnSync(arduinoPath, ["--upload", FIRMWAREFILE], {
      encoding: "utf8"
    });
    const result = output.stdout || "Fail";
    console.log("[Upload]", result);
    if (flashFlag) {
      triggerFailSafe();
    }
    blinkContainer.classList.remove("hidden");
    uploadStatus.classList.add("hidden");
  }, 0);
}

function reset() {
  document.getElementById("interval-input").value = DEFAULT_INTERVAL;
};

function triggerFailSafe() {
  document.getElementById("flash-modal").style.display = "block";
  let count = 3;
  document.getElementById("flash-countdown").innerHTML = `Reverting to previous Blink settings in ${count} seconds`;
  timer = setInterval(() => {
    if (count == 0) {
      closeModal();
      revertSettings();
    } else {
      document.getElementById("flash-countdown").innerHTML = `Reverting to previous Blink settings in ${--count} seconds`;
    }
  }, 1000);
}

function closeModal() {
  clearInterval(timer);
  document.getElementById("flash-modal").style.display = "none";
}

function storeSettings() {
  settings.interval = document.getElementById("interval-input").value;
}

function revertSettings() {
  const intervalInput =  document.getElementById("interval-input");
  intervalInput.value = settings.interval;
  const output = serialize({ interval: intervalInput.value });
  fs.writeFileSync(HEADERFILE, output, "utf8");
  flashFlag = false;
  upload();
}