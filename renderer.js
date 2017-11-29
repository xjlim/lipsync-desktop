// region imports
const fs = require("fs");
const path = require("path");
const cp = require("child_process");
const {dialog} = require('electron').remote;
// endregion

// region constants
const HEADERFILE = "settings.h";
const ARDUINO_MAC_PATH = "/Applications/Arduino.app/Contents/MacOS/Arduino";
const ARDUINO_WINDOWS_PATH =
  "C:\\Program Files (x86)\\Arduino\\arduino_debug.exe";
const SETTINGS_CONSTANTS = {
  interval: "INTERVAL"
};
const FIRMWAREFILE = "Blink.ino";
const DEFAULT_INTERVAL = 1000;
const COUNTDOWN_TIME = 20;
// endregion

// region globals
let timer;
let settings = {
  interval: DEFAULT_INTERVAL
};
let flashFlag = false;
let flags = 0;
let success = false;
let points = [];
let start = false;
// endregion

// start with default
reset();

// region events
const flashButton = document.getElementById("flash-btn");
flashButton.addEventListener("click", function() {
  const interval = document.getElementById("interval-input").value;
  const output = serialize({ interval });
  fs.writeFileSync(path.resolve(__dirname, HEADERFILE), output, "utf8");
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

const dot1 = document.getElementById("one");
dot1.addEventListener("mouseenter", mouseEnter1);
dot1.addEventListener("mouseleave", mouseLeave1);

const dot2 = document.getElementById("two");
dot2.addEventListener("mouseenter", mouseEnter2);
dot2.addEventListener("mouseleave", mouseLeave2);

const dot3 = document.getElementById("three");
dot3.addEventListener("mouseenter", mouseEnter3);
dot3.addEventListener("mouseleave", mouseLeave3);

const dot4 = document.getElementById("four");
dot4.addEventListener("mouseenter", mouseEnter4);
dot4.addEventListener("mouseleave", mouseLeave4);

const calibrateContainer = document.getElementById("calibrate-container");
const polyline = document.getElementById("polyline");
calibrateContainer.addEventListener("mousemove", function(event) {
  if (!start) return;
  points.push(event.offsetX, event.offsetY);
  polyline.setAttribute("points", points);
});

calibrateContainer.addEventListener("mouseup", function() {
  if (success) return;
  start = !start;
  if (start) {
    document.getElementById("two").style.fill = "red";
    document.getElementById("one").style.fill = "blue";
    document.getElementById("instruction").innerHTML =
      "Now move your mouse to the top right dot.";
    flags++;
    polyline.setAttribute("points", "");
    points = [];
  } else {
    if (!success) {
      document.getElementById("instruction").innerHTML =
        "Failed. Move your mouse to the top left dot.";
      clear();
      flags = 0;
    }
  }
});

calibrateContainer.addEventListener("mouseleave", function() {
  if (start) {
    start = false;
    document.getElementById("instruction").innerHTML =
      "Failed. Move your mouse to the top left dot";
    clear();
    flags = 0;
  }
});

const navButtons = document.querySelectorAll(".nav-btn");
navButtons.forEach(btn =>
  btn.addEventListener("click", navButtonsClickHandler)
);
// endregion

// region functions
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
    const output = cp.spawnSync(
      arduinoPath,
      ["--upload", path.resolve(__dirname, FIRMWAREFILE)],
      {
        encoding: "utf8"
      }
    );
    const result = output.stdout;
    console.log("[Upload]", result || "Upload failed");
    if (!result) {
      dialog.showErrorBox("Blink", "Upload failed");
    } else if (flashFlag) {
      triggerFailSafe();
    }
    blinkContainer.classList.remove("hidden");
    uploadStatus.classList.add("hidden");
  }, 0);
}

function reset() {
  document.getElementById("interval-input").value = DEFAULT_INTERVAL;
}

function triggerFailSafe() {
  document.getElementById("flash-modal").style.display = "block";
  let count = COUNTDOWN_TIME;
  document.getElementById(
    "flash-countdown"
  ).innerHTML = `Reverting to previous Blink settings in ${count} seconds`;
  timer = setInterval(() => {
    if (count == 0) {
      closeModal();
      revertSettings();
    } else {
      document.getElementById(
        "flash-countdown"
      ).innerHTML = `Reverting to previous Blink settings in ${--count} seconds`;
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
  const intervalInput = document.getElementById("interval-input");
  intervalInput.value = settings.interval;
  const output = serialize({ interval: intervalInput.value });
  fs.writeFileSync(path.resolve(__dirname, HEADERFILE), output, "utf8");
  flashFlag = false;
  upload();
}

function mouseEnter1() {
  dot1.removeEventListener("mouseenter", mouseEnter1);
  if (!success) {
    if (flags === 0) {
      document.getElementById("instruction").innerHTML =
        "Start the trace with a tap.";
    } else if (flags === 4) {
      document.getElementById("instruction").innerHTML = "Success!";
      const redo = document.createElement("button");
      redo.innerHTML = "Redo";
      redo.setAttribute("id", "redo");
      redo.addEventListener("click", function() {
        document.getElementById("instruction").innerHTML =
          "Move your mouse to the top left dot.";
        clear();
        success = false;
        flags = 0;
        polyline.setAttribute("points", "");
        points = [];
      });
      document.getElementById("instruction").appendChild(redo);
      success = true;
      start = false;
    } else {
      start = false;
      document.getElementById("instruction").innerHTML =
        "Failed. Move your mouse to the top left dot.";
      clear();
      flags = 0;
    }
  }
}

function mouseLeave1() {
  // workaround for bug where mouseenter is fired twice in close succession with trace polyline
  setTimeout(() => {
    dot1.addEventListener("mouseenter", mouseEnter1);
  }, 1000);

  if (!success) {
    if (flags === 0) {
      document.getElementById("instruction").innerHTML =
        "Move your mouse to the top left dot.";
    }
  }
}

function mouseEnter2() {
  dot2.removeEventListener("mouseenter", mouseEnter2);
  if (start && !success) {
    if (flags === 1) {
      document.getElementById("three").style.fill = "red";
      document.getElementById("two").style.fill = "blue";
      document.getElementById("instruction").innerHTML =
        "Now move your mouse to the bottom right dot.";
      flags++;
    } else {
      start = false;
      document.getElementById("instruction").innerHTML =
        "Failed. Move your mouse to the top left dot.";
      clear();
      flags = 0;
    }
  }
}

function mouseLeave2() {
  setTimeout(() => {
    dot2.addEventListener("mouseenter", mouseEnter2);
  }, 1000);
}

function mouseEnter3() {
  dot3.removeEventListener("mouseenter", mouseEnter3);
  if (start && !success) {
    if (flags === 2) {
      document.getElementById("four").style.fill = "red";
      document.getElementById("three").style.fill = "blue";
      document.getElementById("instruction").innerHTML =
        "Now move your mouse to the bottom left dot.";
      flags++;
    } else {
      start = false;
      document.getElementById("instruction").innerHTML =
        "Failed. Move your mouse to the top left dot.";
      clear();
      flags = 0;
    }
  }
}

function mouseLeave3() {
  setTimeout(() => {
    dot3.addEventListener("mouseenter", mouseEnter3);
  }, 1000);
}

function mouseEnter4() {
  dot4.removeEventListener("mouseenter", mouseEnter4);
  if (start && !success) {
    if (flags === 3) {
      document.getElementById("four").style.fill = "blue";
      document.getElementById("one").style.fill = "red";
      document.getElementById("instruction").innerHTML =
        "Now move your mouse to the top left dot.";
      flags++;
    } else {
      start = false;
      document.getElementById("instruction").innerHTML =
        "Failed. Move your mouse to the top left dot.";
      clear();
      flags = 0;
    }
  }
}

function mouseLeave4() {
  setTimeout(() => {
    dot4.addEventListener("mouseenter", mouseEnter4);
  }, 1000);
}

function clear() {
  document.getElementById("one").style.fill = "blue";
  document.getElementById("two").style.fill = "blue";
  document.getElementById("three").style.fill = "blue";
  document.getElementById("four").style.fill = "blue";
}

function navButtonsClickHandler(event) {
  navButtons.forEach(btn => btn.classList.remove("is-active"));
  event.target.classList.add("is-active");
  const sections = document.querySelectorAll("section");
  sections.forEach(section => section.classList.remove("is-shown"));
  if (event.target.innerText === "Home") {
    document.getElementById("home").classList.add("is-shown");
  } else {
    document.getElementById("calibration").classList.add("is-shown");
  }
}
// endregion
