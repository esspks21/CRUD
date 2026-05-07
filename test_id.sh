#!/bin/bash
cd "$(dirname "$0")"
EXE="./crud_app.exe"
DATA="contacts.json"
PASS=0
FAIL=0

run_create() { printf "1\n%s\n\n\n\n0\n" "$1" | "$EXE" > /dev/null 2>&1; }
run_delete() { printf "5\n%s\ny\n0\n"    "$1" | "$EXE" > /dev/null 2>&1; }

get_ids() { python -c "import json; d=json.load(open('$DATA')); print(' '.join(str(c['id']) for c in d))" 2>/dev/null; }

has_dup() {
    local ids=($@)
    local sorted=($(printf '%s\n' "${ids[@]}" | sort -n))
    local uniq=($(printf '%s\n' "${ids[@]}" | sort -nu))
    [ "${#sorted[@]}" -ne "${#uniq[@]}" ]
}

check() {
    local label="$1" cond="$2" detail="$3"
    if [ "$cond" = "true" ]; then
        echo "  [PASS] $label"
        PASS=$((PASS+1))
    else
        echo "  [FAIL] $label $detail"
        FAIL=$((FAIL+1))
    fi
}

# ─── TEST 1: 정상 순차 추가 ─────────────────────────────────────────
echo ""
echo "[TEST 1] 정상 순차 추가 (A → B → C)"
rm -f "$DATA"
run_create "A"; run_create "B"; run_create "C"
IDS=($(get_ids))
has_dup "${IDS[@]}" && DUP=true || DUP=false
check "ID 중복 없음"    "$( [ "$DUP" = "false" ] && echo true || echo false )" "(IDs: ${IDS[*]})"
check "ID = [1, 2, 3]" "$( [ "${IDS[*]}" = "1 2 3" ]         && echo true || echo false )" "(실제: ${IDS[*]})"

# ─── TEST 2: 삭제 후 추가 (삭제된 ID 재사용 여부) ───────────────────
echo ""
echo "[TEST 2] ID 2 삭제 후 새 항목 추가 — 재사용 여부 확인"
run_delete 2; run_create "D"
IDS=($(get_ids))
has_dup "${IDS[@]}" && DUP=true || DUP=false
check "ID 중복 없음"             "$( [ "$DUP" = "false" ]          && echo true || echo false )" "(IDs: ${IDS[*]})"
check "삭제된 ID 2 재사용 안 함" "$( printf '%s\n' "${IDS[@]}" | grep -qx '2' && echo false || echo true )" "(IDs: ${IDS[*]})"
check "새 항목 ID = 4 (max+1)"   "$( [ "${IDS[-1]}" = "4" ]        && echo true || echo false )" "(마지막 ID: ${IDS[-1]})"

# ─── TEST 3: 빈 파일에서 시작 ────────────────────────────────────────
echo ""
echo "[TEST 3] 빈 상태에서 첫 항목 추가"
rm -f "$DATA"
run_create "E"
IDS=($(get_ids))
check "첫 ID = 1" "$( [ "${IDS[0]}" = "1" ] && echo true || echo false )" "(실제: ${IDS[0]})"

# ─── TEST 4: 외부 편집 후 비연속 ID [1,5,10] 뒤에 추가 ─────────────
echo ""
echo "[TEST 4] 외부 편집으로 비연속 ID [1, 5, 10] 후 추가"
cat > "$DATA" << 'EOF'
[
  {"id":1,"name":"X","email":"","phone":"","memo":""},
  {"id":5,"name":"Y","email":"","phone":"","memo":""},
  {"id":10,"name":"Z","email":"","phone":"","memo":""}
]
EOF
run_create "NewItem"
IDS=($(get_ids))
has_dup "${IDS[@]}" && DUP=true || DUP=false
check "ID 중복 없음"             "$( [ "$DUP" = "false" ]   && echo true || echo false )" "(IDs: ${IDS[*]})"
check "새 ID = 11 (max 10 + 1)" "$( [ "${IDS[-1]}" = "11" ] && echo true || echo false )" "(마지막 ID: ${IDS[-1]})"

# ─── TEST 5: 외부 편집으로 중복 ID [1,1,3] 삽입 후 앱이 감지하는지 ─
echo ""
echo "[TEST 5] 외부 편집으로 중복 ID [1,1,3] 삽입 — 앱 감지 여부"
cat > "$DATA" << 'EOF'
[
  {"id":1,"name":"Dup1","email":"","phone":"","memo":""},
  {"id":1,"name":"Dup2","email":"","phone":"","memo":""},
  {"id":3,"name":"Normal","email":"","phone":"","memo":""}
]
EOF
run_create "AfterDup"
IDS=($(get_ids))
has_dup "${IDS[@]}" && STILL_DUP=true || STILL_DUP=false
NEW_ID="${IDS[-1]}"
check "기존 중복 감지 못 함 (알려진 한계)" "$( [ "$STILL_DUP" = "true" ] && echo true || echo false )" "(IDs: ${IDS[*]})"
check "새 추가 ID는 중복 없음 (max+1=4)"   "$( [ "$NEW_ID" = "4" ]       && echo true || echo false )" "(새 ID: $NEW_ID)"

# ─── 결과 ────────────────────────────────────────────────────────────
echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "결과: PASS $PASS / FAIL $FAIL"
[ "$FAIL" -eq 0 ] && echo "전체 통과" || echo "실패 항목 있음"

rm -f "$DATA"
