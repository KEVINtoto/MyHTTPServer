
import argparse
import time
import socket
import ssl
import threading
from datetime import datetime

server_ip = None
port = None
url = None

class HTTPSRequestThread (threading.Thread):
    def __init__(self, threadID, benchtime):
        threading.Thread.__init__(self)
        self.threadID = threadID
        self.speed_ = 0
        self.failed_ = 0
        self.bytes_ = 0
        self.expire = False
        self.benchtime = benchtime

    def set_expire(self):
        self.expire = True

    def run(self):
        # print(datetime.now(), " ", self.threadID, " start.")
        threading.Timer(self.benchtime, self.set_expire).start()
        # socket.setdefaulttimeout(self.benchtime)
        while not self.expire:
            try:
                # print(datetime.now(), " ", self.threadID, ": connecting......")
                sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                ssock = ssl.wrap_socket(sock)
                ssock.settimeout(self.benchtime)
                ssock.connect((server_ip, port))
            except OSError as e:
                continue
                # print(datetime.now(), " ", self.threadID, ": connect timeout.")
            break
        
        if self.expire:
            self.failed_ += 1

        i = 0
        while 1:
            i += 1
            if self.expire:
                break
            request = F"GET {url} HTTP/1.1\r\nHost: {server_ip}\r\nConnection: keep-alive\r\n\r\n"
            try:
                send_bytes = ssock.send(request.encode())
                if send_bytes != len(request):
                    self.failed_ += 1
                    continue
            except OSError as e:
                self.failed_ += 1
                continue
            try:
                recv_bytes = len(ssock.recv(2048))
                if recv_bytes < 0:
                    self.failed_ += 1
                elif recv_bytes == 0:
                    break
                else:
                    self.bytes_ += recv_bytes
                    self.speed_ += 1
            except OSError as e:
                self.failed_ += 1

            if self.expire:
                break
            content = F"username={str(self.threadID)}_{str(i)}&passwd={str(self.threadID)}_{str(i)}&type=register"
            request = F"POST /login.action HTTP/1.1\r\nConnection: keep-alive\r\nContent-Length: {len(content)}\r\n\r\n{content}"
            try:
                send_bytes = ssock.send(request.encode())
                if send_bytes != len(request):
                    self.failed_ += 1
                    continue
            except OSError as e:
                self.failed_ += 1
                continue
            try:
                recv_bytes = len(ssock.recv(2048))
                if recv_bytes < 0:
                    self.failed_ += 1
                elif recv_bytes == 0:
                    break
                else:
                    self.bytes_ += recv_bytes
                    self.speed_ += 1
            except OSError as e:
                self.failed_ += 1
            
            if self.expire:
                break
            request = F"POST /quit.action HTTP/1.1\r\nConnection: keep-alive\r\nContent-Length: 0\r\n\r\n"
            try:
                send_bytes = ssock.send(request.encode())
                if send_bytes != len(request):
                    self.failed_ += 1
                    continue
            except OSError as e:
                self.failed_ += 1
                continue
            try:
                recv_bytes = len(ssock.recv(2048))
                if recv_bytes < 0:
                    self.failed_ += 1
                elif recv_bytes == 0:
                    break
                else:
                    self.bytes_ += recv_bytes
                    self.speed_ += 1
            except OSError as e:
                self.failed_ += 1

            if self.expire:
                break
            content = F"username={str(self.threadID)}_{str(i)}&passwd={str(self.threadID)}_{str(i)}&type=login"
            request = F"POST /login.action HTTP/1.1\r\nConnection: keep-alive\r\nContent-Length: {len(content)}\r\n\r\n{content}"
            try:
                send_bytes = ssock.send(request.encode())
                if send_bytes != len(request):
                    self.failed_ += 1
                    continue
            except OSError as e:
                self.failed_ += 1
                continue
            try:
                recv_bytes = len(ssock.recv(2048))
                if recv_bytes < 0:
                    self.failed_ += 1
                elif recv_bytes == 0:
                    break
                else:
                    self.bytes_ += recv_bytes
                    self.speed_ += 1
            except OSError as e:
                self.failed_ += 1

        ssock.close()
        # print(datetime.now(), " ", self.threadID, " stop.")

    def get_element(self):
        return self.failed_, self.speed_, self.bytes_

def main(args):
    failed_ = 0
    speed_ = 0
    bytes_ = 0
    benchtime = args.benchtime
    clients = args.clients
    threads = []

    for i in range(clients):
        thread = HTTPSRequestThread(i + 1, benchtime)
        threads.append(thread)
        thread.start()
        # thread.join(0)
        # thread.is_alive()

    time.sleep(benchtime)
    for t in threads:
        while t.is_alive():
            t.join()

    # print("all thread stop.")
    for t in threads:
        # t.join()
        f_, s_, b_ = t.get_element()
        failed_ += f_
        speed_ += s_
        bytes_ += b_

    print(F"\nRunning info: {clients} clients, running {int(benchtime)} seconds.\n")
    print(F"Speed={int((speed_ + failed_) / (benchtime / 60.0))} pages/min, {int(bytes_ / float(benchtime))} bytes/sec.\nRequests: {speed_} susceed, {failed_} failed.\n")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='my webbench.')
    parser.add_argument("-t", "--benchtime", type=float, default=30.0,
                        help="bench time.")
    parser.add_argument("-c", "--clients", type=int, default=1000,
                        help="number of clients.")
    parser.add_argument("-p", "--port", type=int, default=443,
                        help="port.")
    parser.add_argument("-h", "--host", type=str, default="127.0.0.1",
                        help="server ip.")
    parser.add_argument("-u", "--url", type=str, default="/index.html",
                        help="url.")
    args = parser.parse_args()
    print(args)
    server_ip = args.host
    port = args.post
    url = args.url
    main(args)
