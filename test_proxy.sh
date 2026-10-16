pkill -f AsyncHttpProxy 2>/dev/null || true
pkill -f "nc -l" 2>/dev/null || true
sleep 1
cd build
./AsyncHttpProxy 5555 & python3 -c 'print("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: 4096\r\n\r\n" + "A"*4096, end="")' | nc -l 127.0.0.1 -p 8000 & wget -e use_proxy=yes -e http_proxy=127.0.0.1:5555 127.0.0.1:8000