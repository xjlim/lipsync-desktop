const fs = require('fs')
const path = require('path')
const BrowserWindow = require('electron').remote.BrowserWindow
const ipc = require('electron').ipcRenderer

const HEADERFILE = 'settings.h'
const ARDUINO_MAC_PATH = '/Applications/Arduino.app/Contents/MacOS/Arduino'
const ARDUINO_WINDOWS_PATH = 'C:\\Program Files (x86)\\Arduino'

function serialize(stringObject) {
  return Object.keys(stringObject).map(key => key + ' ' + stringObject[key] + '\n').join('')
}

function upload() {
  // TODO: verify windows path
  const arduinoPath = process.platform === 'win32' ? '' : ARDUINO_MAC_PATH
  const windowID = BrowserWindow.getFocusedWindow().id
  const invisPath = 'file://' + path.join(__dirname, 'invisible.html')
  let win = new BrowserWindow({ width: 400, height: 400, show: false })
  win.loadURL(invisPath)

  win.webContents.on('did-finish-load', function () {
    win.webContents.send('upload', arduinoPath, windowID)
  })
}

const flashButton = document.getElementById('flash_button')
flashButton.addEventListener('click', function (event) {
  event.preventDefault()
  const sipThreshold = document.getElementById('sip_threshold').value
  const puffThreshold = document.getElementById('puff_threshold').value
  const values = {
    sipThreshold,
    puffThreshold
  }
  const output = serialize(values)
  fs.writeFileSync(HEADERFILE, output, 'utf8');
  console.log(sipThreshold, puffThreshold)
  upload()
})

ipc.on('upload-done', function (event, result) {
  const uploadMessage = document.getElementById('upload_message')
  uploadMessage.textContent = result
})