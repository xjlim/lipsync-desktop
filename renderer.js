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





let flags = 0;
let success = false;

  const dot1 = document.getElementById("one");
  dot1.addEventListener("mouseenter", mouseEnter1);
  dot1.addEventListener("mouseleave", mouseLeave1);
  console.log("enter");


function mouseEnter1() {
    
    if (!success) {
      if (flags === 0) {
        document.getElementById("two").style.fill = "red";
        document.getElementById("instruction").innerHTML = "Now move your mouse to the top right dot"
        flags++
      } else if (flags === 4) {
        document.getElementById("instruction").innerHTML = "Success!"
        success = true

      } else {
        document.getElementById("instruction").innerHTML = "Failed. Move your mouse to the top left dot"
        clear();
        flags = 0
      }
    }
}

function mouseLeave1() {
    //document.getElementById("two").style.fill = "blue";
}

 const dot2 = document.getElementById("two");
  dot2.addEventListener("mouseenter", mouseEnter2);
  dot2.addEventListener("mouseleave", mouseLeave2);
  console.log("enter");


function mouseEnter2() {
 
    
     if (!success) {
      if (flags === 1) {
      document.getElementById("three").style.fill = "red";
       document.getElementById("two").style.fill = "blue";
      
        document.getElementById("instruction").innerHTML = "Now move your mouse to the bottom right dot"
        flags++
      } else {
        document.getElementById("instruction").innerHTML = "Failed. Move your mouse to the top left dot"
        clear();
        flags = 0
      }
    }
}

function mouseLeave2() {
    //document.getElementById("three").style.fill = "blue";
}
 const dot3 = document.getElementById("three");
  dot3.addEventListener("mouseenter", mouseEnter3);
  dot3.addEventListener("mouseleave", mouseLeave3);
  console.log("enter");


function mouseEnter3() {
  
   
     if (!success) {
            if (flags === 2) {

       document.getElementById("four").style.fill = "red";
      document.getElementById("three").style.fill = "blue";
        document.getElementById("instruction").innerHTML = "Now move your mouse to the bottom left dot"
        flags++
      } else {
        document.getElementById("instruction").innerHTML = "Failed. Move your mouse to the top left dot"
        clear();
        flags = 0
      }
    }
}

function mouseLeave3() {
    //document.getElementById("four").style.fill = "blue";
}
 const dot4 = document.getElementById("four");
  dot4.addEventListener("mouseenter", mouseEnter4);
  dot4.addEventListener("mouseleave", mouseLeave4);
  console.log("enter");


function mouseEnter4() {
 
    if (!success) {
            if (flags === 3) {

      document.getElementById("four").style.fill = "blue";
    document.getElementById("one").style.fill = "red";
        document.getElementById("instruction").innerHTML = "Now move your mouse to the top left dot"
        flags++
      } else {
        document.getElementById("instruction").innerHTML = "Failed. Move your mouse to the top left dot"
        clear();
        flags = 0
      }
    }
}

function mouseLeave4() {
   // document.getElementById("one").style.fill = "blue";
}

function clear() {
      document.getElementById("one").style.fill = "blue";
       document.getElementById("two").style.fill = "blue";
       document.getElementById("three").style.fill = "blue";
       document.getElementById("four").style.fill = "blue";

}