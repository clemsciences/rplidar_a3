#!/usr/bin/python3
# coding: utf-8

import socketserver
import time
import codecs

host = "127.0.0.1"
port = 17685

with codecs.open("measures", "r", encoding="utf-8") as f:
    measures = f.read()


class LidarHandler(socketserver.BaseRequestHandler):

    def handle(self):
        try:
            while True:
                self.request.sendall(measures.encode("utf-8"))
                time.sleep(1)
        except ConnectionResetError:
            print("Fini")


if __name__ == "__main__":

    with socketserver.TCPServer((host, port), LidarHandler) as server:
        server.serve_forever()
