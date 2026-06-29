import socket
import sys

BIND_IP = '0.0.0.0'
BIND_PORT = 5005

def start_server():
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.bind((BIND_IP, BIND_PORT))
        print(f"[SERVER] Слушаю на {BIND_IP}:{BIND_PORT}")
    except Exception as e:
        print(f"[ERROR] {e}")
        return

    clients = []
    print("[SERVER] Ожидание клиентов...")
    
    while True:
        data, addr = sock.recvfrom(1024)
        msg = data.decode('utf-8').strip()
        print(f"[SERVER] Получено сообщение от {addr}: {msg}")
        
        if addr not in clients:
            clients.append(addr)
            
        if len(clients) == 2:
            print(f"[SERVER] Оба клиента подключены. Обмен данными...")
            c1 = clients[0]
            c2 = clients[1]
            
            sock.sendto(f"PEER|{c2[0]}|{c2[1]}".encode('utf-8'), c1)
            sock.sendto(f"PEER|{c1[0]}|{c1[1]}".encode('utf-8'), c2)
            
            print("[SERVER] Данные отправлены. Сброс списка клиентов.")
            clients = []

if __name__ == "__main__":
    start_server()
