const fs = require('fs');
const path = require('path');

function getFirmwareVersionFromBin(binbuffer) {
    if (!binbuffer || binbuffer.length < 300*1024) {
        throw "invalid firmware data";
    }
    let firmwarenameIdx = binbuffer.indexOf("esp32weatherbuoy");
    if (firmwarenameIdx < 0 || firmwarenameIdx > 1024) {
        // usually at byte 80 there is the text "esp32weatherbuoy" in the .bin file
        throw "not an esp32weatherbuoy firmware";
    }

    // version information is usually at position 47, "\0vYYMMDD.HHMM"
    let versionIdx = binbuffer.indexOf("\0v");
    if (versionIdx < 16 || versionIdx > firmwarenameIdx) {
        throw "no version information found.";
    }
    let version = binbuffer.toString("ascii", versionIdx+1, versionIdx+13);
    return version;

}



//let buffer = fs.readFileSync("/Users/bernd/Documents/weatherbuoy/build/esp32weatherbuoy.bin");

let buffer = fs.readFileSync("./build/esp32weatherbuoy.bin");
//console.log("Buffer length:", buffer.length);
console.log("Firmware version:", getFirmwareVersionFromBin(buffer));