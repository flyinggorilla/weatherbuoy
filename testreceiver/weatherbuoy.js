const express = require('express');
const path = require('path');

exports.weatherBuoyApp = function(app, certificatePem) {
    const KEEP_ALIVE_TIMEOUT = 20; // seconds
    const MAX_SSID_HOSTNAME_LENGTH = 32;
    
    global.weatherBuoy = {
        systems: {},
        command: {
            last: {
                time: "",
                command: "",
                to: ""
            },
            active: ""            
        },
        firmwarebin: undefined
    }


    app.keepAliveTimeout = KEEP_ALIVE_TIMEOUT * 1000; // 20 seconds

    app.get('/weatherbuoy/firmware.bin', function (req, res) {
        console.log("Request to weatherbuoy firmware!", req.query);
        //var path = require('path');
        // "C:\Users\bernd\Documents\weatherbuoy\build\esp32weatherbuoy.bin"
        //res.sendFile(path.join(__dirname + '/../build/esp32weatherbuoy.bin'));
        if (global.weatherBuoy.firmwarebin) {
            res.setHeader("Content-Type", "application/octet-stream");
            res.send(global.weatherBuoy.firmwarebin);
            res.status(200);
        } else {
            console.log("Error: no firmware available.");
            res.status(404);
        }
    });

    let jsonReplacer = function (key, value) {
        if (key == "firmwarebin") {
            //console.log("replacer: ", key, ", ", value, "vtype: ", typeof value);
            if (value && value["data"]) {
                //console.log("type: ", typeof value.type);
                //console.log("data: ", typeof value.data);
                return value.data.length;
            }
            return "-";
        }
        return value;
    }
    
    app.get('/weatherbuoy/status', function (req, res) {
        console.log("Request to weatherbuoy status data!");
        res.setHeader("Content-Type", "application/json");
        res.send(JSON.stringify(global.weatherBuoy, jsonReplacer));
        res.status(200);
    })

    // in case of using the extended upload limit only for firmware upload path
    // app.use("/weatherbuoy/firmware", express.raw({limit: "1500kb"}));
    app.use(express.raw({limit: "1500kb"}));
    app.put("/weatherbuoy", (req, res) => {
        console.log("Firmware upload PUT request.");
        //console.log(req.headers);
        //console.log(req.body.length);
        if (req.body.length > 384*1024 && req.body.length < 1500*1024) {
            global.weatherBuoy.firmwarebin = req.body;
            let msg = "Firmware received " + req.body.length + " bytes";
            console.log(msg);
            processQueryStringCommand(req, res, msg);
            return;
        } 
        let msg = "Firmware declined " + req.body.length + " bytes";
        console.log(msg);
        res.send(msg);
        res.status(400);
    });

    app.use(express.text()); // express.raw() is another option;
    app.get('/weatherbuoy', function (req, res) {
        console.log("Request to weatherbuoy", req.query)
        processQueryStringCommand(req, res);
    })

    function processQueryStringCommand (req, res, firmwaremsg = null) {
        // detect web browser to return HTML
        let bBrowser = false;
        if (req.headers["accept"] && req.headers.accept.toLowerCase().includes("text/html")) {
            bBrowser = true;
            res.setHeader("Content-Type", "text/html");
            res.sendFile(path.join(__dirname + '/weatherbuoy.html'));
            return;
        } else  {
            res.setHeader("Content-Type", "text/plain");
        }

        let message = "";
        let errMsg = "";

        let simpleNumberRegex = "^[0-9]{1,12}$"; // https://regex101.com/
        let simpleStringRegex = "^[A-Z,a-z,0-9,_.=!]{3," + MAX_SSID_HOSTNAME_LENGTH + "}$"; // 3 to 32 characters!!!  https://regex101.com/
        let urlValidatorRegex = "^(http|https):\/\/(\d*)\/?(.*)$"; // simple, so it can handle also IP addresses

        let messageAddConfigParam = function(prefix, param, validationRegex) {
            let v = req.query[param];
            if (v) {
                if (v.match(validationRegex)) {
                    message += prefix + param + ": " + v + "\r\n";
                } else {
                    errMsg += "ERROR: invalid " + param + "'" + v + "'\r\n";
                }
            }
        }

        messageAddConfigParam("", "to", simpleStringRegex);
        messageAddConfigParam("set-", "hostname", simpleStringRegex);
        messageAddConfigParam("set-", "apssid", simpleStringRegex);
        messageAddConfigParam("set-", "stassid", simpleStringRegex);
        messageAddConfigParam("set-", "appass", simpleStringRegex);
        messageAddConfigParam("set-", "stapass", simpleStringRegex);
        messageAddConfigParam("set-", "intervaldaytime", simpleNumberRegex);
        messageAddConfigParam("set-", "intervalnighttime", simpleNumberRegex);
        messageAddConfigParam("set-", "intervalhealth", simpleNumberRegex);
        messageAddConfigParam("set-", "targeturl", urlValidatorRegex);

        if (req.query["firmware"]) {
            if (req.query.firmware.endsWith(".bin")) {
                message += "set-firmware: " + req.query.firmware + "\r\n";
            } else {
                errMsg += "ERROR: invalid ota URL '" + req.query.firmware + "'\r\n";
            }
        }

        let command = null;
        // scan for commands
        if (typeof req.query.command != 'undefined') {
            if (req.query.command == "restart" || req.query.command == "diagnose" || req.query.command == "update" || req.query.command == "config") {
                command = req.query.command;
                if (command == "update") {
                    if (certificatePem) {
                        message += "set-cert-pem: " + Buffer.from(certificatePem) + "\r\n";
                    }
                    if (!global.weatherBuoy.firmwarebin) {
                        console.log("ERROR: cannot perform OTA update, because firmware is missing.'");
                        res.status(400).send('Error: OTA firmare filename missing!')
                        return;
                    }
                }
            } else if (req.query.command == "clear") {
                global.weatherBuoy.command.active = "";
                global.weatherBuoy.firmwarebin = undefined;
                let r = "Cleared active message buffer, cleared firmware.";
                console.log(r);
                res.send(r);
                res.status(200);
                return;
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

        if (command || message.length || firmwaremsg) {
            message += "timestamp: " + new Date().toISOString() + "\r\n";
            resMsg = firmwaremsg ? firmwaremsg + "\r\n" : "";
            if (command) {
                global.weatherBuoy.command.active = message;
                resMsg += "Command '" + command + "' to Weatherbuoy stations POSTED!: \r\n--->\r\n" + message + "<---";
                res.status(200);
            } else {
                resMsg += "No command, so stay relaxed, nothing happened: \r\n--->\r\n" + message + "<---";
                res.status(firmwaremsg ? 200 : 202);
            }
        } else {
            resMsg = "Welcome to Weatherbuoy command center:\r\n";
            resMsg += JSON.stringify(global.weatherBuoy, jsonReplacer, 4) + "\r\n";
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
        if (global.weatherBuoy.command.active) {
            resMsg +="\r\nStatus: ===>\r\n" + global.weatherBuoy.command.active + "<===\r\n"; 
        }
        //console.log(resMsg);
        res.send(resMsg);


    }

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
            console.log("Weatherbuoy: " + systemHostname + ", " + systemAppversion + ", " + systemUptime + ", " + systemHeapFree + ", " + systemHeapFreeMin);
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

            //console.log(JSON.stringify(global.weatherBuoy, null, 4));

            if (global.weatherBuoy.command.active) {
                console.log("*---------weatherbuoymessage-------------");
                console.log(global.weatherBuoy.command.active);
                console.log("----------weatherbuoymessage------------*");
                let sendmsg = global.weatherBuoy.command.active.split("\r\n");
                let sendToHostname = null;
                let sendCommand = null;
                sendmsg.forEach((m)=>{ kv = m.split(": "); if (kv[0] == "to") sendToHostname = kv[1];});
                sendmsg.forEach((m)=>{ kv = m.split(": "); if (kv[0] == "command") sendCommand = kv[1];});
                console.log("sendToHostname: ", sendToHostname, " command: ", sendCommand);
    
                // if there is a hostname match, we will forward the message to the weatherbuy
                if (sendToHostname && systemHostname && (sendToHostname == systemHostname)) {
                    responseToWeatherbuoyBody = global.weatherBuoy.command.active; 
                    console.log("Weatherbuoy " + systemHostname + " did FETCH the message: \r\n--->\r\n" + responseToWeatherbuoyBody + "<---");

                    //"last": "to: test.weatherbuoy\r\ncommand: diagnose\r\ntimestamp: 2021-03-07T10:19:46.156Z\r\n",
                    global.weatherBuoy.command.last.command = sendCommand;
                    global.weatherBuoy.command.last.to = sendToHostname;
                    global.weatherBuoy.command.last.time = new Date().toISOString();
                }

            }
        }

        res.set('Content-Type', 'text/plain');
        res.set('Keep-Alive', "timeout=" + KEEP_ALIVE_TIMEOUT);
        res.set("Content-Length", responseToWeatherbuoyBody.length);
        res.status(200);
        res.send(responseToWeatherbuoyBody);
        global.weatherBuoy.command.active = "";
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

