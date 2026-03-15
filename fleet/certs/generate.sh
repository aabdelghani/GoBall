#!/usr/bin/env bash
# =============================================================================
# GoBall Fleet — TLS Certificate Generator
# =============================================================================
# Generates a full PKI for mTLS between Pi agents and the MQTT broker.
#
# Usage:
#   ./generate.sh                              # Generate CA + server cert only
#   ./generate.sh client <serial>              # Generate one client cert
#   ./generate.sh client gb0001 gb0002 gb0003  # Batch generate client certs
#
# Output (all in this directory):
#   ca.crt, ca.key              — Certificate Authority
#   server.crt, server.key      — Mosquitto broker cert
#   clients/<serial>.crt/.key   — Per-device client certs
# =============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

CA_DAYS=3650       # 10 years
SERVER_DAYS=825    # ~2.25 years (Apple/browser max)
CLIENT_DAYS=3650   # 10 years

BROKER_HOSTNAME="${BROKER_HOSTNAME:-fleet.example.com}"

# --- Helpers ---

generate_ca() {
    if [[ -f ca.crt && -f ca.key ]]; then
        echo "CA already exists (ca.crt, ca.key). Skipping."
        echo "  Delete them manually to regenerate."
        return
    fi

    echo "==> Generating Certificate Authority..."
    openssl req -x509 -new -nodes \
        -newkey ec -pkeyopt ec_paramgen_curve:prime256v1 \
        -keyout ca.key \
        -out ca.crt \
        -days "$CA_DAYS" \
        -subj "/O=GoBall/CN=GoBall Fleet CA"

    chmod 600 ca.key
    echo "    Created: ca.crt, ca.key (valid ${CA_DAYS} days)"
}

generate_server() {
    if [[ -f server.crt && -f server.key ]]; then
        echo "Server cert already exists. Skipping."
        echo "  Delete server.crt + server.key to regenerate."
        return
    fi

    if [[ ! -f ca.crt || ! -f ca.key ]]; then
        echo "ERROR: CA not found. Run without arguments first." >&2
        exit 1
    fi

    echo "==> Generating server certificate (SAN: localhost, ${BROKER_HOSTNAME})..."

    # CSR
    openssl req -new -nodes \
        -newkey ec -pkeyopt ec_paramgen_curve:prime256v1 \
        -keyout server.key \
        -out server.csr \
        -subj "/O=GoBall/CN=GoBall Fleet Broker"

    # Extensions file for SAN
    cat > server_ext.cnf <<EOF
[v3_ext]
subjectAltName = DNS:localhost, DNS:${BROKER_HOSTNAME}, IP:127.0.0.1
keyUsage = digitalSignature, keyEncipherment
extendedKeyUsage = serverAuth
EOF

    # Sign with CA
    openssl x509 -req \
        -in server.csr \
        -CA ca.crt -CAkey ca.key -CAcreateserial \
        -out server.crt \
        -days "$SERVER_DAYS" \
        -extfile server_ext.cnf -extensions v3_ext

    chmod 600 server.key
    rm -f server.csr server_ext.cnf ca.srl
    echo "    Created: server.crt, server.key (valid ${SERVER_DAYS} days)"
}

generate_client() {
    local serial="$1"

    if [[ ! -f ca.crt || ! -f ca.key ]]; then
        echo "ERROR: CA not found. Run without arguments first." >&2
        exit 1
    fi

    mkdir -p clients

    if [[ -f "clients/${serial}.crt" && -f "clients/${serial}.key" ]]; then
        echo "Client cert for '${serial}' already exists. Skipping."
        return
    fi

    echo "==> Generating client certificate for device: ${serial}"

    # CSR with CN = device serial
    openssl req -new -nodes \
        -newkey ec -pkeyopt ec_paramgen_curve:prime256v1 \
        -keyout "clients/${serial}.key" \
        -out "clients/${serial}.csr" \
        -subj "/O=GoBall/CN=${serial}"

    # Extensions
    cat > "clients/${serial}_ext.cnf" <<EOF
[v3_ext]
keyUsage = digitalSignature
extendedKeyUsage = clientAuth
EOF

    # Sign with CA
    openssl x509 -req \
        -in "clients/${serial}.csr" \
        -CA ca.crt -CAkey ca.key -CAcreateserial \
        -out "clients/${serial}.crt" \
        -days "$CLIENT_DAYS" \
        -extfile "clients/${serial}_ext.cnf" -extensions v3_ext

    chmod 600 "clients/${serial}.key"
    rm -f "clients/${serial}.csr" "clients/${serial}_ext.cnf" ca.srl
    echo "    Created: clients/${serial}.crt, clients/${serial}.key (valid ${CLIENT_DAYS} days)"
}

# --- Main ---

if [[ $# -eq 0 ]]; then
    # Generate CA + server cert
    generate_ca
    generate_server
    echo ""
    echo "Done. To generate a client cert:"
    echo "  $0 client <serial>"
elif [[ "$1" == "client" ]]; then
    shift
    if [[ $# -eq 0 ]]; then
        echo "Usage: $0 client <serial> [serial2] [serial3] ..." >&2
        exit 1
    fi
    for serial in "$@"; do
        generate_client "$serial"
    done
else
    echo "Usage:"
    echo "  $0                              # Generate CA + server cert"
    echo "  $0 client <serial> [serial2]    # Generate client cert(s)"
    exit 1
fi
