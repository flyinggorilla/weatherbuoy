const https = require('https');
const zlib = require('zlib');
const pem = require('pem');
const express = require('express');
const { weatherBuoyApp } = require('./weatherbuoy');
const { exit } = require('process');
// install openssl for the dynamic SSL generation
// npm install pem
// npm install zlib

const mode = process.env.NODE_ENV; // set to "production" when in prod
process.env["NODE_TLS_REJECT_UNAUTHORIZED"] = 0;
var listenPort = 9100;
var isProduction = (mode == 'production');
if (isProduction)
    listenPort = process.env.PORT;

console.log("mode: ", mode);
console.log("port: " + listenPort);

if (!process.env.WEATHERBUOY_DATARECEIVER_URL) {
    console.log("Please provide environment variable \"set WEATHERBUOY_DATARECEIVER_URL=http://<hostname>:<port>/weatherbuoy\"");
    exit();
}

var app = express()
pem.createCertificate({ days: 1, selfSigned: true }, function (err, keys) {
    if (err) {
        throw err
    }

    weatherBuoyApp(app, keys.certificate, process.env.WEATHERBUOY_DATARECEIVER_URL);

    server = https.createServer({ rejectUnauthorized: false, requestCert: false, key: keys.serviceKey, cert: keys.certificate },
        app).listen(listenPort,
            () => console.log(`Example app listening on port ${listenPort}!`));

})
