const fs = require('fs')
const HEADERFILE = 'settings.h'

function serialize(stringObject) {
  return Object.keys(stringObject).map(key => key + ' ' + stringObject[key] + '\n').join('')
}

const flashButton = document.getElementById('flash-btn')
flashButton.addEventListener('click', function (event) {
  const sipThreshold = document.getElementById('sip_threshold').value
  const puffThreshold = document.getElementById('puff_threshold').value
  const values = {
    sipThreshold,
    puffThreshold
  }
  const output = serialize(values)
  fs.writeFileSync(HEADERFILE, output, 'utf8');
  console.log(sipThreshold, puffThreshold)
})

