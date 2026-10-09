#!/usr/bin/env bash
# Best-effort TCC pre-grant for disposable GitHub macOS runners.
# Usage: tcc-pregrant.sh <service> <bundle-id> [client-type]
set -u

SERVICE="${1:?service required}"
CLIENT="${2:?client required}"
CLIENT_TYPE="${3:-0}"
EVIDENCE_DIR="${VOLUM_EVIDENCE_DIR:?VOLUM_EVIDENCE_DIR is required}"
LOG="$EVIDENCE_DIR/tcc.log"
HELPER="$RUNNER_TEMP/volum-tcc-insert.py"
mkdir -p "$EVIDENCE_DIR"

{
  echo "===== TCC grant $SERVICE $CLIENT type=$CLIENT_TYPE ====="
  csrutil status 2>&1 || true
} >> "$LOG"

cat > "$HELPER" <<'PY'
import sqlite3
import sys
import time

db_path, service, client, client_type = sys.argv[1:5]
client_type = int(client_type)
now = int(time.time())

connection = sqlite3.connect(db_path, timeout=10)
try:
    columns = connection.execute("PRAGMA table_info(access)").fetchall()
    if not columns:
        raise RuntimeError("TCC access table is absent")
    names = [row[1] for row in columns]
    print("schema columns:", ", ".join(names))

    known = {
        "service": service,
        "client": client,
        "client_type": client_type,
        "allowed": 1,
        "prompt_count": 1,
        "auth_value": 2,
        "auth_reason": 4,
        "auth_version": 1,
        "csreq": None,
        "policy_id": None,
        "indirect_object_identifier_type": 0,
        "indirect_object_identifier": "UNUSED",
        "indirect_object_code_identity": None,
        "flags": 0,
        "last_modified": now,
        "pid": 0,
        "pid_version": 0,
        "boot_uuid": "UNUSED",
        "last_reminded": now,
    }

    insert_names = []
    values = []
    for _, name, declared_type, not_null, default, _ in columns:
        if name in known:
            value = known[name]
        elif default is not None:
            continue
        elif not_null:
            value = "" if "CHAR" in declared_type.upper() or "TEXT" in declared_type.upper() else 0
        else:
            value = None
        insert_names.append(name)
        values.append(value)

    connection.execute(
        "DELETE FROM access WHERE service=? AND client=? AND client_type=?",
        (service, client, client_type),
    )
    quoted = ", ".join(f'"{name}"' for name in insert_names)
    placeholders = ", ".join("?" for _ in insert_names)
    connection.execute(
        f"INSERT INTO access ({quoted}) VALUES ({placeholders})",
        values,
    )
    connection.commit()
    row = connection.execute(
        "SELECT service, client, client_type, "
        + ("auth_value" if "auth_value" in names else "allowed")
        + " FROM access WHERE service=? AND client=? AND client_type=?",
        (service, client, client_type),
    ).fetchone()
    print("inserted:", row)
finally:
    connection.close()
PY

grant_db() {
  local label="$1"
  local db="$2"
  shift 2
  {
    echo "$label database: $db"
    if [[ ! -f "$db" ]]; then
      echo "SKIP $label TCC database does not exist"
      return 0
    fi
    "$@" python3 "$HELPER" "$db" "$SERVICE" "$CLIENT" "$CLIENT_TYPE"
    local ec=$?
    if [[ "$ec" -eq 0 ]]; then
      echo "PASS inserted $SERVICE for $CLIENT into $label TCC database"
    else
      echo "SKIP unable to write $label TCC database (exit $ec)"
    fi
    return 0
  } >> "$LOG" 2>&1
}

grant_db user "$HOME/Library/Application Support/com.apple.TCC/TCC.db"
grant_db system "/Library/Application Support/com.apple.TCC/TCC.db" sudo
killall tccd >/dev/null 2>&1 || true
sudo killall tccd >/dev/null 2>&1 || true

cat "$LOG"
exit 0
