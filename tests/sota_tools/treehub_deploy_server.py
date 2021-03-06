#!/usr/bin/python3

import sys
import codecs
import socket
import ssl
import cgi
import os
from http.server import BaseHTTPRequestHandler, HTTPServer
from json import dump
from tempfile import NamedTemporaryFile


class TreehubServerHandler(BaseHTTPRequestHandler):
    made_requests = {}

    def __init__(self, *args):
        BaseHTTPRequestHandler.__init__(self, *args)

    def do_HEAD(self):
        print("HEAD: %s\n" % self.path)
        if self.drop_check():
            return
        self.send_response_only(404)
        self.end_headers()

    def do_POST(self):
        print("POST: %s\n" % self.path)
        ctype, pdict = cgi.parse_header(self.headers['Content-Type'])
        if ctype == 'multipart/form-data':
            pdict['boundary'] = bytes(pdict['boundary'], 'utf-8')
            fields = cgi.parse_multipart(self.rfile, pdict)
            if "file" in fields:
                full_path =  os.path.join(tmp_dir, self.path[1:])
                os.system("mkdir -p %s" % os.path.dirname(full_path))
                with open(full_path, "wb") as obj_file:
                    obj_file.write(fields['file'][0])
                self.send_response_only(200)
                self.end_headers()
                return
        self.send_response_only(400)
        self.end_headers()

    def drop_check(self):
        if drop_connection_every_n_request == 0:
            return False
        self.made_requests[self.path] = self.made_requests.get(self.path, 0) + 1
        print("request number: %d" % self.made_requests[self.path])
        if self.made_requests[self.path] == 1:
            return True
        else:
            if drop_connection_every_n_request == self.made_requests[self.path]:
                self.made_requests[self.path] = 0
            return False


class ReUseHTTPServer(HTTPServer):
    def server_bind(self):
        self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        HTTPServer.server_bind(self)


tmp_dir = sys.argv[2]
server_address = ('', int(sys.argv[1]))
httpd = ReUseHTTPServer(server_address, TreehubServerHandler)
if len(sys.argv) == 4 and sys.argv[3]:
    print("Dropping connection after every: %s request" % sys.argv[3])
    drop_connection_every_n_request = int(sys.argv[3])
else:
    drop_connection_every_n_request = 0

httpd.socket = ssl.wrap_socket (httpd.socket,
                                certfile='tests/fake_http_server/client.crt',
                                keyfile='tests/fake_http_server/client.key',
                                server_side=True,
                                ca_certs = "tests/fake_http_server/client.crt")

try:
    httpd.serve_forever()
except KeyboardInterrupt as k:
    print("%s exiting..." % sys.argv[0])

