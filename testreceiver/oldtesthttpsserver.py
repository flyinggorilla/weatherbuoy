# generate TLS certificates here
# generate certificate with hostname "<hostname>"
# https://www.selfsignedcertificate.com/

from http.server import HTTPServer, BaseHTTPRequestHandler
from io import BytesIO
import ssl


class SimpleHTTPRequestHandler(BaseHTTPRequestHandler):

    def do_GET(self):
        self.send_response(200)
        self.end_headers()
        self.wfile.write(b'Hello, world!')

    def do_POST(self):
        content_length = int(self.headers['Content-Length'])
        body = self.rfile.read(content_length)
        self.send_response(200)
        self.end_headers()
        response = BytesIO()
        response.write(b'This is POST request. ')
        response.write(b'Received: ')
        response.write(body)
        self.wfile.write(response.getvalue())


httpd = HTTPServer(('studio2', 9100), SimpleHTTPRequestHandler)
httpd.socket = ssl.wrap_socket (httpd.socket, 
        keyfile="certificates/studio2.key", 
        certfile='certificates/studio2.cert', 
        server_side=True)

httpd.serve_forever()