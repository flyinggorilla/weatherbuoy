const https = require('https');
const zlib = require('zlib');
const pem = require('pem');
const express = require('express');
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

var app = express()


pem.createCertificate({ days: 1, selfSigned: true }, function (err, keys) {
    if (err) {
        throw err
    }
    app.use(express.text()); // express.raw() is another option;

/*    app.get('/', function (req, res) {
        console.log("request to home", req.query)
        res.send('server running!')
    })

    app.get('/weatherbuoy', function (req, res) {
        console.log("request to weatherbuoy", req.query)
        res.send('weatherbuoy server running!')
    })


    if (mode != 'production') {
        var path = require('path');
    
        app.get('/firmware.bin', function (rootreq, rootres) {
            rootres.sendFile(path.join(__dirname + '/firmware.bin'));
        });
    }
    
    var queryResponse = ""; */

    app.use(express.text({defaultCharset: "ascii", type: "text/*"})); // express.raw() is another option;
    app.post('/weatherbuoy', (req, res, next) => {
        console.log(req.body);
        console.log(req.method, " request to weatherbouy ", req.query)
        console.log("STORE host: " + req.hostname)
        console.log("subdomains: " + req.subdomains)
        console.log("headers", req.rawHeaders);
        console.log("query: ", req.query);
        console.log("body: ", req.body);
        responseBody  = "otaurl: https://10.10.29.104:9100/firmware.bin\r\n";
        responseBody += "reboot: 0\r\n";
        responseBody += "diagnose: all\r\n";
        res.set('Content-Type', 'text/plain');
        res.set("Content-Length", responseBody.length);
        res.status(200);        
        res.send(responseBody);
    });
    
    https.createServer({ key: keys.serviceKey, cert: keys.certificate }, 
                        app).listen(listenPort, 
                            () => console.log(`Example app listening on port ${listenPort}!`));
})


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

