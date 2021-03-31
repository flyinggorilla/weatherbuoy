const express = require('express');
const path = require('path');
const https = require("https");
const http = require("http");
//const Dequeue = require('dequeue'); // https://github.com/lleo/node-dequeue

exports.weatherBuoyApp = function(app, certificatePem = null, forwardingurl) {
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


    app.keepAliveTimeout = KEEP_ALIVE_TIMEOUT * 1000; 

    app.get('/weatherbuoy/firmware.bin', function (req, res) {
        // console.log("Request to weatherbuoy firmware!", req.query);
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
            if (value && value["data"]) {
                return value.data.length;
            }
            return "-";
        }
        return value;
    }
    
    app.get('/weatherbuoy/status', function (req, res) {
        // console.log("Request to weatherbuoy status data!");
        res.setHeader("Content-Type", "application/json");
        res.send(JSON.stringify(global.weatherBuoy, jsonReplacer));
        res.status(200);
    })

    // in case of using the extended upload limit only for firmware upload path
    // app.use("/weatherbuoy/firmware", express.raw({limit: "1500kb"}));
    app.use(express.raw({limit: "1500kb"}));
    app.put("/weatherbuoy", (req, res) => {
        // console.log("Firmware upload PUT request.");
        //console.log(req.headers);
        //console.log(req.body.length);
        if (req.body.length > 384*1024 && req.body.length < 1500*1024) {
            global.weatherBuoy.firmwarebin = req.body;
            let msg = "Firmware received " + req.body.length + " bytes";
            // console.log(msg);
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
        // console.log("Request to weatherbuoy", req.query)
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
                // console.log(r);
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
            resMsg += 'example: curl "https://<hostname>/weatherbuoy?command=diagnose&to=buoytest"\r\n';
            resMsg += 'example: curl "https://<hostname>/weatherbuoy?command=restart&to=buoytest"\r\n';
            resMsg += 'example: curl "https://<hostname>/weatherbuoy?command=config&to=buoytest&apssid=buoytest&appass=secret\r\n';
            resMsg += 'example: curl -H "Content-Type: application/octet-stream" -T .\\build\\esp32weatherbuoy.bin "https://<hostname>/weatherbuoy?command=update&to=buoytest"\r\n';
            resMsg += 'usage: curl [--insecure | -k] "<url>"\r\n';
            resMsg += '       curl --insecure "https://<hostname>/weatherbuoy?to=testWeatherbuoy&restart"\r\n';
            resMsg += "receipient: to=<hostname>\r\n";
            resMsg += "commands: command=[restart|diagnose|config|update|clear]\r\n";
            resMsg += "configs: apssid=<*>, appass=<*>, stassid=<*>,d stapass=<*>, hostname=<*>, targeturl=<url>\r\n";
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
            console.log(req.method, " request to weatherbouy ", req.query)
            console.log("STORE host: " + req.hostname)
            console.log("subdomains: " + req.subdomains)
            console.log("headers", req.rawHeaders);
            console.log("query: ", req.query);
            console.log(req.body);
        }

        // parse hostname of weatherbuoy
        let systemHostname = null;
        let systemValue = null;
        let systemHeapFreeMin = null;
        let systemHeapFree = null;
        let systemAppversion = null;
        let maximetValue = null;
        let linesFromWeatherbuoy = req.body.split("\r\n");
        linesFromWeatherbuoy.forEach((m)=>{ kv = m.split(": "); if (kv[0] == "system") systemValue = kv[1];});
        linesFromWeatherbuoy.forEach((m)=>{ kv = m.split(": "); if (kv[0] == "maximet") maximetValue = kv[1];});
        try {
            systemValue = systemValue.split(",");
            systemAppversion = systemValue[0];
            systemHostname = systemValue[1];
            systemUptime = systemValue[2];
            systemHeapFree = systemValue[3];
            systemHeapFreeMin = systemValue[4];
            // console.log("Weatherbuoy: " + systemHostname + ", " + systemAppversion + ", " + systemUptime + ", " + systemHeapFree + ", " + systemHeapFreeMin);
            if (maximetValue) {
                // console.log("MaximetValue: ", maximetValue);
                ProcessMeasurements(systemHostname, systemUptime, systemHeapFree, systemHeapFreeMin, maximetValue);
            }
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
            system.systemLastSeen = Date.now();
            linesFromWeatherbuoy.forEach((m)=>{ 
                kv = m.split(": "); 
                if (kv[1]) {
                    system[kv[0]] = kv[1];
                };
            });

            //console.log(JSON.stringify(global.weatherBuoy, null, 4));

            if (global.weatherBuoy.command.active) {
                // console.log("*---------weatherbuoymessage-------------");
                // console.log(global.weatherBuoy.command.active);
                // console.log("----------weatherbuoymessage------------*");
                let sendmsg = global.weatherBuoy.command.active.split("\r\n");
                let sendToHostname = null;
                let sendCommand = null;
                sendmsg.forEach((m)=>{ kv = m.split(": "); if (kv[0] == "to") sendToHostname = kv[1];});
                sendmsg.forEach((m)=>{ kv = m.split(": "); if (kv[0] == "command") sendCommand = kv[1];});
                // console.log("sendToHostname: ", sendToHostname, " command: ", sendCommand);
    
                // if there is a hostname match, we will forward the message to the weatherbuy
                if (sendToHostname && systemHostname && (sendToHostname == systemHostname)) {
                    responseToWeatherbuoyBody = global.weatherBuoy.command.active; 
                    // console.log("Weatherbuoy " + systemHostname + " did FETCH the message: \r\n--->\r\n" + responseToWeatherbuoyBody + "<---");

                    //"last": "to: buoytest\r\ncommand: diagnose\r\ntimestamp: 2021-03-07T10:19:46.156Z\r\n",
                    global.weatherBuoy.command.last.command = sendCommand;
                    global.weatherBuoy.command.last.to = sendToHostname;
                    global.weatherBuoy.command.last.time = new Date().toISOString();
                }

            }
        }

        res.set('Content-Type', 'text/plain');
        res.set('Keep-Alive', "timeout=" + KEEP_ALIVE_TIMEOUT);
        res.status(200);
        if (responseToWeatherbuoyBody.length) {
            res.set("Content-Length", responseToWeatherbuoyBody.length);
            res.send(responseToWeatherbuoyBody);
        } else {
            res.end();
        }
        global.weatherBuoy.command.active = "";
    });

    // GMX501 Full data
    // -------------------------------
    // NODE,DIR,SPEED,CDIR,AVGDIR,AVGSPEED,GDIR,GSPEED,AVGCDIR,WINDSTAT,PRESS,PASL,PSTN,RH,TEMP,DEWPOINT,AH,COMPASSH,SOLARRAD,SOLARHOURS,WCHILL,HEATIDX,AIRDENS,WBTEMP,SUNR,SNOON,SUNS,SUNP,TWIC,TWIN,TWIA,XTILT,YTILT,ZORIENT,USERINF,TIME,VOLT,STATUS,CHECK
    // -,DEG,MS,DEG,DEG,MS,DEG,MS,DEG,-,HPA,HPA,HPA,%,C,C,G/M3,DEG,WM2,HRS,C,C,KG/M3,C,-,-,-,DEG,-,-,-,DEG,DEG,-,-,-,V,-,-

    // \x02Q,168,000.02,213,000,000.00,053,000.05,000,0000,0991.1,1046.2,0991.4,035,+023.2,+007.1,07.42,045,0000,00.00,,,1.2,+014.2,04:55,11:11,17:27,325:-33,04:22,03:45,03:06,-34,+55,+1,,2021-03-27T21:19:31.7,+04.6,0000,\x030F
    // \x02Q,160,000.02,205,000,000.00,053,000.05,000,0000,0991.0,1046.1,0991.3,035,+023.2,+007.1,07.40,045,0000,00.00,,,1.2,+014.2,04:55,11:11,17:27,325:-33,04:22,03:45,03:06,-34,+55,+1,,2021-03-27T21:19:32.7,+04.6,0000,\x0304
    // \x02Q,156,000.02,201,000,000.00,053,000.05,000,0000,0991.0,1046.1,0991.3,035,+023.2,+007.0,07.38,045,0000,00.00,,,1.2,+014.2,04:55,11:11,17:27,325:-33,04:22,03:45,03:06,-34,+55,+1,,2021-03-27T21:19:33.7,+04.6,0000,\x030A        

    let columnsCsv = "NODE,DIR,SPEED,CDIR,AVGDIR,AVGSPEED,GDIR,GSPEED,AVGCDIR,WINDSTAT,PRESS,PASL,PSTN,RH,TEMP,DEWPOINT,AH,COMPASSH,SOLARRAD,SOLARHOURS,WCHILL,HEATIDX,AIRDENS,WBTEMP,SUNR,SNOON,SUNS,SUNP,TWIC,TWIN,TWIA,XTILT,YTILT,ZORIENT,USERINF,TIME,VOLT,STATUS,CHECK";
    let unitsCsv = "-,DEG,MS,DEG,DEG,MS,DEG,MS,DEG,-,HPA,HPA,HPA,%,C,C,G/M3,DEG,WM2,HRS,C,C,KG/M3,C,-,-,-,DEG,-,-,-,DEG,DEG,-,-,-,V,-,-";

    let columns = columnsCsv.split(",");
    let units = unitsCsv.split(",");


    function processMaximet(data, columns, units) {
        const unitsFloat = ["MS", "HPA", "%", "C", "G/M3", "WM2", "KG/M3", "V"];
        const unitsText = ["-", ];
        const unitsInt = ["DEG", "HRS"];
        const STX = "\x02";
        const ETX = "\x03";

        let stxPos = data.indexOf(STX);
        let etxPos = data.lastIndexOf(ETX);

        if (stxPos != 0 || etxPos <= 2) {
            console.log("invalid data");
            return;
        }

        let dataPoints = data.substring(stxPos+1, etxPos-1).split(",");
        //console.log(dataPoints);

        let maximet = {};
        for (p in dataPoints) {
            if (unitsFloat.includes(units[p])) {
                dataPoints[p] = parseFloat(dataPoints[p]);
            } else if (unitsInt.includes(units[p])) {
                dataPoints[p] = parseInt(dataPoints[p]);
            } else {
            }
            maximet[columns[p]] = dataPoints[p];
        }
    
        // console.log(dataPoints);
        // console.log(measurement);
        return maximet;
    }    

    class Measurement {
        constructor(timestamp) {
            this.timestamp = timestamp;
        }
    }

    function maximetToMeasurement(maximet) {
        let measurement = new Measurement(new Date());

        measurement.windspeed_ms = maximet["SPEED"];
        measurement.windspeed_average_ms = maximet["AVGSPEED"];
        measurement.windspeed_gust_ms = maximet["GSPEED"];
        measurement.winddirection = maximet["CDIR"];
        measurement.winddirection_average = maximet["AVGCDIR"];
        measurement.winddirection_gust = maximet["GDIR"]+maximet["COMPASSH"];
        measurement.temperature = maximet["TEMP"];
        measurement.solarradiation = maximet["SOLARRAD"];
        measurement.rain_intensity = null;
        measurement.pressure = maximet["PASL"];
        measurement.humidity_relative = maximet["RH"];
        measurement.humidity_absolute = maximet["AH"];
        measurement.water_temperature = null;
        measurement.water_depth = null;
        return measurement;
    }

    let hourlyQueue = {};

    function getHourlyKey(timestamp) {
        return timestamp.getDay()*100 + timestamp.getHours();
    }

    function ProcessMeasurements(host, uptime, heapfree, heapfreemin, maximetdata) {
        //let where = "time > '" + startTime.toISOString() + "' ";


//        let sendmsg = global.weatherBuoy.command.active.split("\r\n");
        try {
            let maximet = processMaximet(maximetdata, columns, units);
            let measurement = maximetToMeasurement(maximet);

            let key = getHourlyKey(measurement.timestamp);
            let hourly = hourlyQueue[key];
            if (!hourly) {
                hourly = { };
                hourlyQueue[key] = hourly;
            }

            let hourlyStation = hourly[host];
            if (!hourly[host]) {
                hourlyStation = {    timestamp : [],
                                    windspeed_ms : [],
                                    windspeed_average_ms : [],
                                    windspeed_gust_ms : [],
                                    winddirection : [],
                                    winddirection_average : [],
                                    winddirection_gust : [],
                                    temperature : [],
                                    solarradiation : [],
                                    rain_intensity : [],
                                    pressure : [],
                                    humidity_relative : [],
                                    humidity_absolute : [],
                                    water_temperature : [],
                                    water_depth : [],
                                };
                hourly[host] = hourlyStation;
            }

            hourlyStation.timestamp.push(measurement.timestamp);
            hourlyStation.windspeed_ms.push(measurement.windspeed_ms);
            hourlyStation.windspeed_average_ms.push(measurement.windspeed_average_ms);
            hourlyStation.windspeed_gust_ms.push(measurement.windspeed_gust_ms);
            hourlyStation.winddirection.push(measurement.winddirection);
            hourlyStation.winddirection_average.push(measurement.winddirection_average);
            hourlyStation.winddirection_gust.push(measurement.winddirection_gust);
            hourlyStation.temperature.push(measurement.temperature);
            hourlyStation.solarradiation.push(measurement.solarradiation);
            hourlyStation.rain_intensity.push(measurement.rain_intensity);
            hourlyStation.pressure.push(measurement.pressure);
            hourlyStation.humidity_relative.push(measurement.humidity_relative);
            hourlyStation.humidity_absolute.push(measurement.humidity_absolute);
            hourlyStation.water_temperature.push(measurement.water_temperature);
            hourlyStation.water_depth.push(measurement.water_depth);

            console.log(hourlyQueue[key]);



            
            //let uri =  encodeURI('/query?db=weather&u=' + influxdb_username + '&p=' + influxdb_password + '&epoch=ms&q=') + encodeURIComponent(query);
            let url = forwardingurl + "?host="+host+"&uptime="+uptime+"&heapfree="+heapfree+"&heapfreemin="+heapfreemin+"&temp="+measurement.temperature;
            // console.log(url);
            const options = {
                headers: { 'Content-type': 'application/json' }
            };
        
            http.get(url, options, (queryres) => {
                // console.log('statusCode:', queryres.statusCode);
                // console.log('headers:', queryres.headers);
            }).on('error', function (e) {
                console.log("Got an error: ", e);
            }); 
    
        } catch (error) {
            console.error(error);
            console.log("Error processing measurements or forwarding data.")
        }
    

    }

    


}



