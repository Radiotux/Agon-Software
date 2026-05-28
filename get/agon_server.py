import os
import socket
import sys

PORT = 2000
CHUNK_SIZE = 32768  # 32 KB safe paging network windows

transfer_sessions = {}

def generate_file_list():
    """Dynamically generates lista.txt following the custom filename|size retro-format"""
    ignore_list = ['lista.txt', 'agon_server.py', 'get.bin', 'get.txt', 'README.TXT']
    print("[*] Scanning local directory and refreshing 'lista.txt'...")
    with open("lista.txt", "w") as f:
        for filename in os.listdir('.'):
            if os.path.isfile(filename) and filename not in ignore_list:
                size = os.path.getsize(filename)
                f.write(f"{filename}|{size}\n")
                print(f"  -> Indexed: {filename} ({size} bytes)")

def main():
    generate_file_list()

    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

    try:
        server.bind(("0.0.0.0", PORT))
        server.listen(5)

        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            s.connect(("8.8.8.8", 80))
            ip_address = s.getsockname()[0]
            s.close()
        except:
            ip_address = "192.168.0.15"

        print(f"\n=== AGON GET v2.2 - BLOCK AUTO-PAGING FILE SERVER ===")
        print(f"[*] Server IP (Enter on Agon Light console): {ip_address}")
        print(f"[*] Listening on port: {PORT}")
        print(f"[*] Network secure data stream sliding window: {CHUNK_SIZE // 1024} KB")
        print("-" * 60)

        while True:
            conn, addr = server.accept()
            client_ip = addr[0]

            request = conn.recv(1024).decode('utf-8', errors='ignore')
            if not request:
                conn.close()
                continue

            filename = 'lista.txt'
            try:
                lines = request.split('\r\n')
                if lines and len(lines) > 0:
                    parts = lines[0].split(' ')
                    if len(parts) > 1:
                        parsed_path = parts[1].lstrip('/')
                        if parsed_path:
                            filename = parsed_path
            except:
                filename = 'lista.txt'

            if filename == 'lista.txt':
                if client_ip in transfer_sessions:
                    del transfer_sessions[client_ip]
                print(f"\n[+] Inbound connection from {client_ip} -> Requesting file list grid.")
                if os.path.isfile('lista.txt'):
                    with open('lista.txt', 'rb') as f:
                        content = f.read()
                    conn.sendall(b"HTTP/1.0 200 OK\r\n\r\n")
                    conn.sendall(content)
                conn.close()
                continue

            if os.path.isfile(filename):
                file_size = os.path.getsize(filename)

                if client_ip not in transfer_sessions or transfer_sessions[client_ip]['filename'] != filename:
                    transfer_sessions[client_ip] = {'filename': filename, 'offset': 0}
                    print(f"\n[>] Starting new stream session: '{filename}' ({file_size} bytes)")

                current_offset = transfer_sessions[client_ip]['offset']

                try:
                    with open(filename, 'rb') as f:
                        f.seek(current_offset)
                        chunk_data = f.read(CHUNK_SIZE)

                    transfer_sessions[client_ip]['offset'] += len(chunk_data)

                    if current_offset == 0:
                        conn.sendall(b"HTTP/1.0 200 OK\r\n\r\n")

                    conn.sendall(chunk_data)
                    print(f"  -> Dispatched chunk range: {current_offset} to {transfer_sessions[client_ip]['offset']} bytes.")

                    if transfer_sessions[client_ip]['offset'] >= file_size:
                        print(f"[OK] Transfer completed successfully for file '{filename}'.")
                        del transfer_sessions[client_ip]

                except Exception as e:
                    print(f"  [-] Error processing stream transmission: {e}")
            else:
                conn.sendall(b"HTTP/1.0 404 Not Found\r\n\r\n")

            conn.close()

    except OSError:
        print(f"[CRITICAL] Port {PORT} is already in use by another application.")
    except KeyboardInterrupt:
        print("\n[*] Shutting down sliding window file server...")
        server.close()

if __name__ == "__main__":
    main()
