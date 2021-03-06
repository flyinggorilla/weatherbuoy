const https = require('https');
const zlib = require('zlib');
const express = require('express');
// install openssl for the dynamic SSL generation
// npm install pem
// npm install zlib

/*const KEEP_ALIVE_TIMEOUT = 20; // seconds
const MAX_SSID_HOSTNAME_LENGTH = 32;

const mode = process.env.NODE_ENV; // set to "production" when in prod
process.env["NODE_TLS_REJECT_UNAUTHORIZED"] = 0;
var listenPort = 9100;
var isProduction = (mode == 'production');
if (isProduction)
    listenPort = process.env.PORT;

console.log("mode: ", mode);
console.log("port: " + listenPort);
*/

exports.weatherBuoyApp = function(app) {
    global.weatherBuoy = {
        sendMessage: undefined,
        systems: {}
    }

    app.keepAliveTimeout = KEEP_ALIVE_TIMEOUT * 1000; // 20 seconds

    app.get('/weatherbuoy/firmware.bin', function (req, res) {
        console.log("Request to weatherbuoy firmware!", req.query);
        var path = require('path');
        // "C:\Users\bernd\Documents\weatherbuoy\build\esp32weatherbuoy.bin"
        res.sendFile(path.join(__dirname + '/../build/esp32weatherbuoy.bin'));
    });
    

    app.use(express.text()); // express.raw() is another option;
    app.get('/weatherbuoy', function (req, res) {
        console.log("Request to weatherbuoy", req.query)

        console.log("HEADERS:", req.headers);

        // detect web browser to return HTML
        let bBrowser = false;
        if (req.headers["accept"]) {
            req.headers.accept.toLowerCase.includes("text/html");
            bBrowser = true;
        } 

        res.setHeader("Content-Type", "text/plain");
        let message = "";
        let errMsg = "";

        let simpleNumberRegex = "^[0-9]{1,12}$"; // https://regex101.com/
        let simpleStringRegex = "^[A-Z,a-z,0-9,_.=!]{3," + MAX_SSID_HOSTNAME_LENGTH + "}$"; // 3 to 32 characters!!!  https://regex101.com/
        let urlValidatorRegex = "^(http|https):\/\/(\d*)\/?(.*)$"; // simple, so it can handle also IP addresses

        // check which weatherbuoy to message <all> or by <hostname>
        if (typeof req.query.to != 'undefined') {
            let to = req.query.to;
            if (to == "all" || to.match(simpleStringRegex)) {
                message += "to: " + to + "\r\n";
            } else {
                errMsg += "ERROR: invalid receipient to: <all>|<hostname> '" + to + "'\r\n";
            }
        }

        // scan for config
        if (typeof req.query.hostname != 'undefined') {
            if (req.query.hostname.match(simpleStringRegex)) {
                message += "set-hostname: " + req.query.hostname + "\r\n";
            } else {
                errMsg += "ERROR: invalid hostname '" + req.query.hostname + "'\r\n";
            }
        }

        if (typeof req.query.apssid != 'undefined') {
            if (req.query.apssid.match(simpleStringRegex)) {
                message += "set-apssid: " + req.query.apssid + "\r\n";
            } else {
                errMsg += "ERROR: invalid AP-SSID '" + req.query.apssid + "'\r\n";
            }
        }

        if (typeof req.query.stassid != 'undefined') {
            if (req.query.stassid.match(simpleStringRegex)) {
                message += "set-stassid: " + req.query.stassid + "\r\n";
            } else {
                errMsg += "ERROR: invalid STA-SSID '" + req.query.apSsid + "'\r\n";
            }
        }

        if (typeof req.query.appass != 'undefined') {
            if (req.query.appass.match(simpleStringRegex)) {
                message += "set-appass: " + req.query.appass + "\r\n";
            } else {
                errMsg += "ERROR: invalid AP password '" + req.query.appass + "'\r\n";
            }
        }

        if (typeof req.query.stapass != 'undefined') {
            if (req.query.stapass.match(simpleStringRegex)) {
                message += "set-stapass: " + req.query.stapass + "\r\n";
            } else {
                errMsg += "ERROR: invalid STA password '" + req.query.stapass + "'\r\n";
            }
        }


        if (typeof req.query.intervaldaytime != 'undefined') {
            if (req.query.stapass.match(simpleNumberRegex)) {
                message += "set-intervaldaytime: " + req.query.intervaldaytime + "\r\n";
            } else {
                errMsg += "ERROR: invalid data/health sending interval in seconds, '" + req.query.intervaldaytime + "'\r\n";
            }
        }

        if (typeof req.query.intervalnighttime != 'undefined') {
            if (req.query.stapass.match(simpleNumberRegex)) {
                message += "set-intervalnighttime: " + req.query.intervalnighttime + "\r\n";
            } else {
                errMsg += "ERROR: invalid data/health sending interval in seconds, '" + req.query.intervalnighttime + "'\r\n";
            }
        }

        if (typeof req.query.intervalhealth != 'undefined') {
            if (req.query.stapass.match(simpleNumberRegex)) {
                message += "set-intervalhealth: " + req.query.intervalhealth + "\r\n";
            } else {
                errMsg += "ERROR: invalid data/health sending interval in seconds, '" + req.query.intervalhealth + "'\r\n";
            }
        }

        if (typeof req.query.firmware != 'undefined') {
            if (req.query.firmware.endsWith(".bin")) {
                message += "set-firmware: " + req.query.firmware + "\r\n";
                message += "set-cert-pem: " + Buffer.from(keys.certificate) + "\r\n";
            } else {
                errMsg += "ERROR: invalid ota URL '" + req.query.firmware + "'\r\n";
            }
        }

        if (typeof req.query.targeturl != 'undefined') {
            if (req.query.targeturl.match(urlValidatorRegex)) {
                message += "set-targeturl: " + req.query.targeturl + "\r\n";
            } else {
                errMsg += "ERROR: invalid target URL '" + req.query.firmwatargeturlrepath + "'\r\n";
            }
        }

        let command = null;
        // scan for commands
        if (typeof req.query.command != 'undefined') {
            if (req.query.command == "restart" || req.query.command == "diagnose" || req.query.command == "update" || req.query.command == "config") {
                command = req.query.command;
                if (command == "update") {
                    if (message.indexOf("set-firmware:") < 0) {
                        console.log("ERROR: cannot perform OTA update, because firmware is missing.'");
                        res.status(400).send('Error: OTA firmare filename missing!')
                        return;
                    }
                }
            }
        } 
        
        if (command) {
            message += "command: " + command + "\r\n";
        }

        let resMsg = null;
        if (errMsg.length) {
            console.log(errMsg);
            res.status(400).send(errMsg);
            return;
        } 


        if (command || message.length) {
            message += "timestamp: " + new Date().toISOString() + "\r\n";
            if (command) {
                global.weatherbuoyMessage = message;
                resMsg = "Command '" + command + "' to Weatherbuoy stations POSTED!: \r\n--->\r\n" + message + "<---";
                res.status(200);
            } else {
                resMsg = "No command, so stay relaxed, nothing happened: \r\n--->\r\n" + message + "<---";
                res.status(202);
            }
        } else {
            resMsg = "Welcome to Weatherbuoy command center:\r\n";
            resMsg += JSON.stringify(global.weatherBuoy, null, 4) + "\r\n";
            resMsg += "example: https://atterwind.info/weatherbuoy?command=diagnose&to=test.weatherbuoy\r\n";
            resMsg += "example: https://atterwind.info/weatherbuoy?command=restart&to=test.weatherbuoy\r\n";
            resMsg += "example: https://atterwind.info/weatherbuoy?command=config&to=test.weatherbuoy&apssid=test.weatherbuoy&appass=secret\r\n";
            resMsg += "example: https://atterwind.info/weatherbuoy?command=update&to=test.weatherbuoy&firmware=esp32weatherbuoy202101010.bin\r\n";
            resMsg += "usage: curl [--insecure] \"<url>\"\r\n";
            resMsg += "       curl --insecure \"https://localhost:9100/weatherbuoy?to=testWeatherbuoy&restart\"\r\n";
            resMsg += "receipient: to=<hostname>\r\n";
            resMsg += "commands: command=[restart|diagnose|config|update]\r\n";
            resMsg += "configs: apssid=<*>, appass=<*>, stassid=<*>,d stapass=<*>, hostname=<*>, targeturl=<url>, firmware=<[/?&=*]*.bin>\r\n";
            resMsg += "configs: intervaldaytime=<seconds>, intervalnighttime=<seconds>, intervalhealth=<seconds>\r\n";
            resMsg += "special commands: [status | clear] - to view the status of message delivery or clear the message\r\n";
            res.status(200);
        }
        if (global.weatherbuoyMessage != undefined) {
            resMsg +="\r\nStatus: ===>\r\n" + global.weatherbuoyMessage + "<===\r\n"; 
        }
        console.log(resMsg);
        res.send(resMsg);
    })


    app.use(express.text({ defaultCharset: "ascii", type: "text/*" })); // express.raw() is another option;
    app.post('/weatherbuoy', (req, res, next) => {
        let responseToWeatherbuoyBody = "";
        if (false) {
            //console.log(req.body);
            console.log(req.method, " request to weatherbouy ", req.query)
            console.log("STORE host: " + req.hostname)
            console.log("subdomains: " + req.subdomains)
            console.log("headers", req.rawHeaders);
            console.log("query: ", req.query);
            //console.log("body: --->\r\n" + req.body + "<---");
        }
        console.log(req.body);

        // parse hostname of weatherbuoy
        let systemHostname = null;
        let systemValue = null;
        let systemHeapFreeMin = null;
        let systemHeapFree = null;
        let systemAppversion = null;
        let linesFromWeatherbuoy = req.body.split("\r\n");
        linesFromWeatherbuoy.forEach((m)=>{ kv = m.split(": "); if (kv[0] == "system") systemValue = kv[1];});
        try {
            systemValue = systemValue.split(",");
            systemAppversion = systemValue[0];
            systemHostname = systemValue[1];
            systemUptime = systemValue[2];
            systemHeapFree = systemValue[3];
            systemHeapFreeMin = systemValue[4];
            console.log("Weatherbuoy: " + systemHostname + ", " + systemAppversion + ", " + systemHeapFree + ", " + systemHeapFreeMin);
        } catch {
            console.log("Error, garbled system data from weatherbuoy: " + systemHostname + ", " + systemAppversion + ", " + systemHeapFree + ", " + systemHeapFreeMin);
            systemHostname = null;
        }

        if (systemHostname) {
            if (!global.weatherBuoy.systems[systemHostname]) {
                global.weatherBuoy.systems[systemHostname] = {};
            }
            let system = global.weatherBuoy.systems[systemHostname];
            system.systemAppVersion = systemAppversion;
            system.systemUptime = systemUptime;
            system.systemHeapFree = systemHeapFree;
            system.systemHeapFreeMin = systemHeapFreeMin;
            linesFromWeatherbuoy.forEach((m)=>{ 
                kv = m.split(": "); 
                if (kv[1]) {
                    system[kv[0]] = kv[1];
                };
            });

            //console.log(global.weatherBuoy);
            global.weatherBuoy.sendMessage = "";
            console.log(JSON.stringify(global.weatherBuoy, null, 4));
            global.weatherBuoy.sendMessage = undefined;

            if (global.weatherBuoy.sendMessage != undefined) {
                console.log("*---------weatherbuoymessage-------------");
                console.log(global.weatherBuoy.sendMessage);
                console.log("----------weatherbuoymessage------------*");
                let sendmsg = global.weatherBuoy.sendMessage.split("\r\n");
                let sendToHostname = null;
                sendmsg.forEach((m)=>{ kv = m.split(": "); if (kv[0] == "to") sendToHostname = kv[1];});
                console.log("sendToHostname: " + sendToHostname);
    
                // if there is a hostname match, we will forward the message to the weatherbuy
                if (sendToHostname && systemHostname && (sendToHostname == systemHostname)) {
                    responseToWeatherbuoyBody = global.weatherBuoy.sendMessage; 
                    console.log("Weatherbuoy " + systemHostname + " did FETCH the message: \r\n--->\r\n" + responseToWeatherbuoyBody + "<---");
                }
            }
    


        }

        res.set('Content-Type', 'text/plain');
        res.set('Keep-Alive', "timeout=" + KEEP_ALIVE_TIMEOUT);
        res.set("Content-Length", responseToWeatherbuoyBody.length);
        res.status(200);
        res.send(responseToWeatherbuoyBody);
        global.weatherBuoy.sendMessage = undefined;
    });
}


/*
app.get('/data', (appreq, appres) => {
    console.log("host: " + appreq.hostname)
    console.log("subdomains: " + appreq.subdomains)
    console.log(appreq.query)
    var scope = "today";
    if (typeof appreq.query.scope != 'undefined') {
        scope = appreq.query.scope;
    }

    var station = "kammer";
    if (typeof appreq.query.station != 'undefined') {
        station = appreq.query.station;
    }

    console.log(`station: ${station} scope: ${scope}`);

    //let scope = "24h";
    //let where = "time > now() - 1d";
    let resolution = "1h";

    let days = parseInt(scope.replace("d", "")) // d1, d3, d7, ...
    if (!days) days = 0; // today
    let startTime = new Date();
    startTime.setDate(startTime.getDate() - days);
    startTime.setHours(isProduction ? -2 : 0) // delta from UTC
    startTime.setMinutes(0);
    startTime.setSeconds(0)
    startTime.setMilliseconds(0);
    let where = "time > '" + startTime.toISOString() + "' ";

    let uri =  encodeURI('/query?db=weather&u=' + influxdb_username + '&p=' + influxdb_password + '&epoch=ms&q=') + encodeURIComponent(query);


    const options = {
        host: influxdb_hostname,
        port: influxdb_port,
        path: uri,
        headers: { 'accept-encoding': 'gzip,deflate' }
    };

    https.get(options, (queryres) => {
        var chunks = [];
        console.log('statusCode:', queryres.statusCode);
        console.log('headers:', queryres.headers);

        queryres.on('data', function (chunk) {
            chunks.push(chunk);
            //console.log("huhuu - reading cbhunk: " + chunk)
        });

        queryres.on('end', function () {
            //if(.headers['content-encoding'] == 'gzip'){
            //console.log("BODY:" + body);
            var gzippedBody = Buffer.concat(chunks);
            let unzippedBody = zlib.gunzipSync(gzippedBody).toString();

            //var influxdata = JSON.parse(unzippedBody);
            //console.log("Got a response: ", influxdata.toString());

            const response = {
                statusCode: 200,
                body: unzippedBody // , //JSON.stringify('Hello from Lambda!'),
                //isBase64Encoded: false
            };
            appres.set('Content-Encoding', 'gzip')
            appres.set("Content-type", "application/json")
            appres.set("Access-Control-Allow-Origin", "*");
            appres.send(gzippedBody);
        });
    }).on('error', function (e) {
        console.log("Got an error: ", e);
        appres.status(500).send("Got an error");
    });

});*/

