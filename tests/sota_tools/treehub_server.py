#!/usr/bin/python3

import argparse
import cgi
import os
import signal
import ssl
import subprocess
import sys
import time
from contextlib import ExitStack
from http.server import BaseHTTPRequestHandler, HTTPServer
from random import seed, randrange
from pathlib import Path
from tempfile import TemporaryDirectory


class TreehubServerHandler(BaseHTTPRequestHandler):
    made_requests = 0

    def do_HEAD(self):
        if self.drop_check():
            print("Dropping HEAD request %s" % self.path)
            return
        print("Processing HEAD request %s" % self.path)
        path = os.path.join(repo_path, self.path[1:])
        if os.path.exists(path):
            self.send_response_only(200)
        else:
            self.send_response_only(404)
        self.end_headers()

    def do_GET(self):
        if self.drop_check():
            print("Dropping GET request %s" % self.path)
            return
        print("Processing GET request %s" % self.path)
        path = os.path.join(repo_path, self.path[1:])
        if os.path.exists(path):
            if args.sleep and args.sleep > 0.0:
                time.sleep(args.sleep)
            self.send_response_only(200)
            self.end_headers()
            with open(path, 'rb') as source:
                while True:
                    data = source.read(1024)
                    if not data:
                        break
                    self.wfile.write(data)
        else:
            self.send_response_only(404)
            self.end_headers()

    def do_HEAD(self):
        if self.drop_check():
            print("Dropping HEAD request %s" % self.path)
            return
        print("Processing HEAD request %s" % self.path)
        path = os.path.join(repo_path, self.path[1:])
        if os.path.exists(path):
            self.send_response_only(200)
        else:
            self.send_response_only(404)
        self.end_headers()

    def do_POST(self):
        print("POST: %s\n" % self.path)
        ctype, pdict = cgi.parse_header(self.headers['Content-Type'])
        if ctype == 'multipart/form-data':
            pdict['boundary'] = bytes(pdict['boundary'], 'utf-8')
            fields = cgi.parse_multipart(self.rfile, pdict)
            if "file" in fields:
                full_path = os.path.join(repo_path, self.path[1:])
                os.system("mkdir -p %s" % os.path.dirname(full_path))
                with open(full_path, "wb") as obj_file:
                    obj_file.write(fields['file'][0])
                self.send_response_only(200)
                self.end_headers()
                return
        self.send_response_only(400)
        self.end_headers()

    def drop_check(self):
        self.__class__.made_requests += 1
        if args.fail and args.fail > 0:
            return self.__class__.made_requests % args.fail == 0
        else:
            return False


def sig_handler(signum, frame):
    sys.exit(0)


def create_repo(path):
    tree = Path(path) / 'tree'
    tree.mkdir()
    seed(0)
    for i in range(10):
        file = tree / str(i)
        file.with_suffix('.bin').write_bytes(bytes([randrange(256) for _ in range(2**18)]))

    subprocess.run(['ostree', 'init', '--mode=archive-z2',
                    '--repo={}'.format(path)], check=True)
    subprocess.run(['ostree', '--repo={}'.format(path), 'commit', '--consume',
                    '--branch=master', '--owner-uid=0', '--owner-gid=0',
                    '--timestamp=1970-01-01 00:00:00 +0000', str(tree)],
                   check=True)


parser = argparse.ArgumentParser()
parser.add_argument('-p', '--port', type=int, required=True,
                    help='listening port')
parser.add_argument('-c', '--create', action='store_true',
                    help='create new ostree repo')
parser.add_argument('-d', '--dir', help='ostree repo directory')
parser.add_argument('-f', '--fail', type=int, help='fail every nth request')
parser.add_argument('-s', '--sleep', type=float,
                    help='sleep for n.n seconds for every GET request')
parser.add_argument('-t', '--tls', action='store_true',
                    help='require TLS from clients')
args = parser.parse_args()

signal.signal(signal.SIGTERM, sig_handler)
try:
    with ExitStack() as stack:
        if args.dir:
            repo_path = args.dir
        else:
            repo_path = stack.enter_context(TemporaryDirectory(prefix='treehub-'))
        if args.create:
            create_repo(repo_path)
        httpd = HTTPServer(('', args.port), TreehubServerHandler)
        if args.tls:
            httpd.socket = ssl.wrap_socket(httpd.socket,
                                           certfile='tests/fake_http_server/client.crt',
                                           ca_certs='tests/fake_http_server/client.crt',
                                           keyfile='tests/fake_http_server/client.key',
                                           server_side=True)
        httpd.serve_forever()
except (SystemExit, KeyboardInterrupt) as e:
    print("%s exiting..." % sys.argv[0])
