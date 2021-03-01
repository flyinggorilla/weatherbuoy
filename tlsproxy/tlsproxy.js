// https://riptutorial.com/node-js/example/19326/tls-socket--server-and-client
// https://nodejs.org/en/knowledge/cryptography/how-to-use-the-tls-module/

const tls = require('tls');
const pem = require('pem');
//import pem from 'pem'; // npm install pem AND also install OpenSSL and put it on path


const LISTENPORT = 9100;
const FORWARDPORT = 9101
const HOST = "localhost";// '127.0.0.1'

var srvSock;
var disconnected = true;

pem.createCertificate({ days: 1, selfSigned: true }, function (err, keys) {
    if (err) {
        throw err;
    }

    var serverOptions = {
        key: keys.serviceKey,
        cert: keys.certificate,
        rejectUnauthorized: false,
        requestCert: false,
        //enableTrace: true
    };
    var clientOptions = {
        rejectUnauthorized: false,
        allowHalfOpen: true
    };

    var client = null;

    var server = tls.createServer(serverOptions, function (serverSocket) {
        srvSock = serverSocket;
        serverSocket.on('data', function (data) {

            console.log('Request: %s [it is %d bytes long]',
                data.toString(), //data.toString().replace(/(\n)/gm, ""),
                data.length);

            if (!client) {
                client = tls.connect(FORWARDPORT, HOST, clientOptions, function () {
                    if (client.authorized) {
                        console.log("***RECONNECTING");
                        disconnected = false;
                    } else {
                        console.log("***RECONNECTING Connection not authorized: " + client.authorizationError)
                    }
                });

                client.on("data", function (data) {
                    console.log('Response: %s [it is %d bytes long]',
                        data.toString(), //data.toString().replace(/(\n)/gm, ""),
                        data.length);
                    srvSock.write(data);
                    //client.end();
                });

                client.on("end", function () {
                    console.log('EOT CLIENT');
                });

                client.on('close', function () {
                    console.log("Connection closed");
                });

                client.on('error', function (error) {
                    console.error("CLIENT ERROR ", error);
                    client = tls.connect(FORWARDPORT, HOST, clientOptions, function () {
                        if (client.authorized) {
                            console.log("Connection RECONNECTED .");
                        } else {
                            console.log("Connection not authorized: " + client.authorizationError)
                        }
                    });
                    client.on("data", function (data) {
                        console.log('Response: %s [it is %d bytes long]',
                            data.toString(), //data.toString().replace(/(\n)/gm, ""),
                            data.length);
                        srvSock.write(data);
                        //client.end();
                    });

                    client.on("end", function () {
                        console.log('EOT CLIENT');
                    });

                    client.on('close', function () {
                        console.log("Connection closed");
                        client.destroy();
                        client = null;
                    });

                    client.on('error', function (error) {
                        console.error("CLIENT ERROR ", error);
                        client.destroy();
                        client = null;
                    });
                });

            }


            client.write(data);

        });

        serverSocket.on('end', function () {
            console.log('EOT SERVER (End Of Transmission)');
        });
    });

    server.listen(LISTENPORT, function () {
        console.log("I'm listening at %s, on port %s", HOST, LISTENPORT);
    });

    server.on('error', function (error) {
        console.error("server error: ", error);
        server.destroy();
    });

});