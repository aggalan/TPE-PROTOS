#!/usr/bin/env bash
set -m                                   # job control

SERVER=0.0.0.0
PORT=1080                                # tu servidor SOCKS
N=500                                    # conexiones a generar

# saludo SOCKSv5: \x05(version) \x01(nmethods) \x00(NO_AUTH)
greeting=$(printf '\x05\x01\x00')

echo "Lanzando $N clientes contra $SERVER:$PORT"

# 1. Abrir las conexiones en segundo plano
for i in $(seq 1 "$N"); do
  { printf "$greeting" | nc -N "$SERVER" "$PORT" >& /dev/null; } &
done

# 2. Contar cuántas están ESTAB cada segundo hasta alcanzar N
while true; do
  # --- usa ss si existe; si no, cae a netstat ---
  if command -v ss >/dev/null 2>&1; then
    estab=$(ss -tan state ESTAB "( sport = :$PORT )" | wc -l)
  else
    # macOS o sistemas sin ss
    estab=$(netstat -an | grep "\.$PORT " | grep ESTABLISHED | wc -l)
  fi

  printf '\r%s conexiones establecidas: %s/%s' "$(date +%T)" "$estab" "$N"

  # Salir cuando llegamos (o superamos) la meta
  [ "$estab" -ge "$N" ] && break
  sleep 1
done
echo    # salto de línea final

# 3. Mantener vivas las conexiones un rato para que puedas inspeccionar
echo "Conexiones listas. Pulsa Ctrl-C para cerrar o espera 20 s…"
sleep 20
