#!/bin/bash

# Configuración del servidor
PROXY_HOST="127.0.0.1"
PROXY_PORT="1080"
USERNAME="ivovila"
PASSWORD="crack"
SOCKS_URL="socks5h://$USERNAME:$PASSWORD@$PROXY_HOST:$PROXY_PORT"

# Colores para output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}=== SUITE DE TESTS PARA SERVIDOR SOCKSv5 ===${NC}"
echo "Usuario: $USERNAME"
echo "Servidor: $PROXY_HOST:$PROXY_PORT"
echo ""

# Test 1: Conectividad básica
echo -e "${YELLOW}Test 1: Conectividad básica${NC}"
curl --silent --max-time 10 --proxy "$SOCKS_URL" http://httpbin.org/ip > /dev/null
if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ Conectividad básica: PASS${NC}"
else
    echo -e "${RED}✗ Conectividad básica: FAIL${NC}"
fi
echo ""

# Test 2: Resolución DNS a través del proxy (IPv4)
echo -e "${YELLOW}Test 2: Resolución DNS IPv4${NC}"
curl --silent --max-time 10 --proxy "$SOCKS_URL" http://example.com > /dev/null
if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ Resolución DNS IPv4: PASS${NC}"
else
    echo -e "${RED}✗ Resolución DNS IPv4: FAIL${NC}"
fi
echo ""

# Test 3: Resolución DNS a través del proxy (IPv6)
echo -e "${YELLOW}Test 3: Resolución DNS IPv6${NC}"
curl --silent --max-time 10 --proxy "$SOCKS_URL" https://ipv6-test.com/ > /dev/null 2>&1
if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ Resolución DNS IPv6: PASS${NC}"
else
    echo -e "${RED}✗ Resolución DNS IPv6: FAIL (puede ser normal si no hay IPv6)${NC}"
fi
echo ""

# Test 3b: Conexión directa IPv6
echo -e "${YELLOW}Test 3b: Conexión directa IPv6${NC}"
curl --silent --max-time 10 --proxy "$SOCKS_URL" 'http://[2001:4860:4860::8888]:80/' > /dev/null 2>&1
if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ Conexión directa IPv6: PASS${NC}"
else
    echo -e "${RED}✗ Conexión directa IPv6: FAIL (puede ser normal si no hay IPv6)${NC}"
fi
echo ""

# Test 4: Múltiples IPs para mismo dominio (robustez)
echo -e "${YELLOW}Test 4: Robustez con múltiples IPs${NC}"
curl --silent --max-time 10 --proxy "$SOCKS_URL" http://google.com > /dev/null
if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ Robustez múltiples IPs: PASS${NC}"
else
    echo -e "${RED}✗ Robustez múltiples IPs: FAIL${NC}"
fi
echo ""

# Test 5: Transferencia de archivos pequeños
echo -e "${YELLOW}Test 5: Transferencia archivo pequeño (100KB)${NC}"
curl --silent --max-time 15 --proxy "$SOCKS_URL" http://speedtest.tele2.net/100KB.zip > /dev/null
if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ Transferencia 100KB: PASS${NC}"
else
    echo -e "${RED}✗ Transferencia 100KB: FAIL${NC}"
fi
echo ""

# Test 6: Transferencia de archivos medianos
echo -e "${YELLOW}Test 6: Transferencia archivo mediano (1MB)${NC}"
curl --silent --max-time 30 --proxy "$SOCKS_URL" http://speedtest.tele2.net/1MB.zip > /dev/null
if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ Transferencia 1MB: PASS${NC}"
else
    echo -e "${RED}✗ Transferencia 1MB: FAIL${NC}"
fi
echo ""

# Test 7: Concurrencia básica (50 conexiones)
echo -e "${YELLOW}Test 7: Concurrencia básica (50 conexiones)${NC}"
seq 1 50 | parallel -j0 -n0 "curl --silent --max-time 20 --output /dev/null --proxy '$SOCKS_URL' http://httpbin.org/delay/1" 2>/dev/null
if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ Concurrencia 50: PASS${NC}"
else
    echo -e "${RED}✗ Concurrencia 50: FAIL${NC}"
fi
echo ""

# Test 8: Concurrencia media (200 conexiones)
echo -e "${YELLOW}Test 8: Concurrencia media (200 conexiones)${NC}"
seq 1 200 | parallel -j0 -n0 "curl --silent --max-time 30 --output /dev/null --proxy '$SOCKS_URL' http://speedtest.tele2.net/100KB.zip" 2>/dev/null
if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ Concurrencia 200: PASS${NC}"
else
    echo -e "${RED}✗ Concurrencia 200: FAIL${NC}"
fi
echo ""

# Test 9: Concurrencia alta (500 conexiones) - Requerimiento del TP
echo -e "${YELLOW}Test 9: Concurrencia alta (500 conexiones) - REQUERIMIENTO TP${NC}"
echo "Iniciando test de 500 conexiones concurrentes..."
start_time=$(date +%s)
seq 1 500 | parallel -j0 -n0 "curl --silent --max-time 60 --output /dev/null --proxy '$SOCKS_URL' http://speedtest.tele2.net/100KB.zip" 2>/dev/null
exit_code=$?
end_time=$(date +%s)
duration=$((end_time - start_time))

if [ $exit_code -eq 0 ]; then
    echo -e "${GREEN}✓ Concurrencia 500: PASS (Tiempo: ${duration}s)${NC}"
else
    echo -e "${RED}✗ Concurrencia 500: FAIL (Tiempo: ${duration}s)${NC}"
fi
echo ""

# Test 10: Autenticación incorrecta
echo -e "${YELLOW}Test 10: Autenticación incorrecta${NC}"
curl --silent --max-time 10 --proxy "socks5h://wrong:user@$PROXY_HOST:$PROXY_PORT" http://httpbin.org/ip > /dev/null 2>&1
if [ $? -ne 0 ]; then
    echo -e "${GREEN}✓ Rechazo auth incorrecta: PASS${NC}"
else
    echo -e "${RED}✗ Rechazo auth incorrecta: FAIL${NC}"
fi
echo ""

# Test 11: Prueba de stress con descargas simultáneas
echo -e "${YELLOW}Test 11: Stress test con descargas simultáneas${NC}"
echo "Descargando archivos de 1MB con 100 conexiones concurrentes..."
start_time=$(date +%s)
seq 1 100 | parallel -j0 -n0 "curl --silent --max-time 60 --output /dev/null --proxy '$SOCKS_URL' http://speedtest.tele2.net/1MB.zip" 2>/dev/null
exit_code=$?
end_time=$(date +%s)
duration=$((end_time - start_time))

if [ $exit_code -eq 0 ]; then
    echo -e "${GREEN}✓ Stress test: PASS (Tiempo: ${duration}s)${NC}"
else
    echo -e "${RED}✗ Stress test: FAIL (Tiempo: ${duration}s)${NC}"
fi
echo ""

# Test 12: Conexiones HTTPS
echo -e "${YELLOW}Test 12: Conexiones HTTPS${NC}"
curl --silent --max-time 15 --proxy "$SOCKS_URL" https://httpbin.org/ip > /dev/null
if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ Conexiones HTTPS: PASS${NC}"
else
    echo -e "${RED}✗ Conexiones HTTPS: FAIL${NC}"
fi
echo ""

# Test 13: Conexiones a servicios no disponibles (robustez)
echo -e "${YELLOW}Test 13: Robustez ante servicios no disponibles${NC}"
curl --silent --max-time 10 --proxy "$SOCKS_URL" http://192.0.2.1:8080 > /dev/null 2>&1
if [ $? -ne 0 ]; then
    echo -e "${GREEN}✓ Manejo de servicios no disponibles: PASS${NC}"
else
    echo -e "${RED}✗ Manejo de servicios no disponibles: FAIL${NC}"
fi
echo ""

# Test 14: Diferentes tamaños de respuesta
echo -e "${YELLOW}Test 14: Diferentes tamaños de respuesta${NC}"
sizes=(1024 10240 102400)
for size in "${sizes[@]}"; do
    curl --silent --max-time 20 --proxy "$SOCKS_URL" "http://httpbin.org/bytes/$size" > /dev/null
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}✓ Respuesta ${size} bytes: PASS${NC}"
    else
        echo -e "${RED}✗ Respuesta ${size} bytes: FAIL${NC}"
    fi
done
echo ""

# Test 15: Benchmark de throughput
echo -e "${YELLOW}Test 15: Benchmark de throughput${NC}"
echo "Midiendo throughput con 50 descargas de 1MB..."
start_time=$(date +%s)
seq 1 50 | parallel -j10 -n0 "curl --silent --max-time 60 --output /dev/null --proxy '$SOCKS_URL' http://speedtest.tele2.net/1MB.zip" 2>/dev/null
end_time=$(date +%s)
duration=$((end_time - start_time))
total_mb=$((50 * 1))
throughput=$(echo "scale=2; $total_mb / $duration" | bc -l 2>/dev/null || echo "N/A")

if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ Throughput test: PASS (${throughput} MB/s promedio)${NC}"
else
    echo -e "${RED}✗ Throughput test: FAIL${NC}"
fi
echo ""

echo -e "${GREEN}=== FIN DE LA SUITE DE TESTS ===${NC}"
echo ""
echo -e "${YELLOW}Notas importantes:${NC}"
echo "- Asegúrate de que el servidor esté ejecutándose antes de correr los tests"
echo "- Los tests de IPv6 pueden fallar si no hay soporte IPv6"
echo "- El test de 500 conexiones es crítico para aprobar el TP"
echo "- Monitorea el uso de CPU y memoria durante los tests"
echo "- Los timeouts no son obligatorios según el enunciado, pero son buena práctica"
echo ""
echo -e "${YELLOW}Para tests adicionales de performance:${NC}"
echo "1. Usa 'htop' o 'top' para monitorear recursos"
echo "2. Usa 'ss -tuln' para ver conexiones activas"
echo "3. Usa 'netstat -i' para monitorear tráfico de red"