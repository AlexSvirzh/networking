import socket
import sys
import threading
import time

def listen_port(sock):
    print("[THREAD] Слушатель запущен и ждет P2P сообщений...")
    while True:
        try:
            data, addr = sock.recvfrom(1024)
            msg = data.decode('utf-8')
            if "Hello P2P" in msg:
                print(f"\n[SUCCESS] >>> P2P УСПЕХ! От {addr}: {msg}")
            else:
                print(f"\n[INFO] Сообщение от {addr}: {msg}")
        except Exception:
            break

def start_client(server_ip, server_port):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(('0.0.0.0', 0))
    print(f"[CLIENT] Клиент на порту {sock.getsockname()[1]}")
    print(f"[CLIENT] Стучимся на сервер {server_ip}:{server_port}...")
    sock.sendto(b"HELLO_SERVER", (server_ip, int(server_port)))
    print("[CLIENT] Жду ответ от сервера...")
    
    peer_ip = None
    peer_port = None
    
    try:
        data, addr = sock.recvfrom(1024)
        msg = data.decode('utf-8')
        if msg.startswith('PEER'):
            parts = msg.split('|')
            peer_ip = parts[1]
            peer_port = int(parts[2])
            print(f"[CLIENT] СЕРВЕР ПРИСЛАЛ ПИРА: {peer_ip}:{peer_port}")
    except Exception as e:
        print(e)
        return

    listener = threading.Thread(target=listen_port, args=(sock,), daemon=True)
    listener.start()
    
    print(f"[CLIENT] Начинаю бомбардировку {peer_ip}:{peer_port}...")
    for i in range(10):
        msg = f"Hello P2P! (Packet {i})"
        sock.sendto(msg.encode('utf-8'), (peer_ip, peer_port))
        time.sleep(0.5)
        
    while True:
        time.sleep(1)

if __name__ == "__main__":
    s_ip = sys.argv[1]
    s_port = sys.argv[2]
    start_client(s_ip, s_port)
