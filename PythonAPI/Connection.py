import INETConsts
import socket

def connectTo(address, port, listening_port = 5001):
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.bind((address, listening_port))
        sock.listen()
        conn, addr = sock.accept()
        with conn:
            print(f"Connected by {addr}")
            while True:
                data = conn.recv(1024)
                if not data:
                    break
                conn.sendall(data)