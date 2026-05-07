$exe = ".\crud_app.exe"
$dataFile = "contacts.json"
$pass = 0
$fail = 0

function Run-Create {
    param([string]$name)
    # 메뉴 1 선택 → 이름 입력 → 나머지 엔터 → 메뉴 0 종료
    $input = "1`n$name`n`n`n`n0`n"
    $result = $input | & $exe 2>&1
    return $result
}

function Get-Ids {
    if (-not (Test-Path $dataFile)) { return @() }
    $json = Get-Content $dataFile -Raw | ConvertFrom-Json
    return $json | ForEach-Object { $_.id }
}

function Check {
    param([string]$label, [bool]$cond, [string]$detail = "")
    if ($cond) {
        Write-Host "  [PASS] $label" -ForegroundColor Green
        $script:pass++
    } else {
        Write-Host "  [FAIL] $label $detail" -ForegroundColor Red
        $script:fail++
    }
}

function Has-Duplicates {
    param([int[]]$ids)
    $unique = $ids | Sort-Object -Unique
    return $ids.Count -ne $unique.Count
}

# ─── 테스트 1: 정상 순차 추가 ───────────────────────────────────────
Write-Host "`n[TEST 1] 정상 순차 추가 (A, B, C)" -ForegroundColor Cyan
Remove-Item $dataFile -ErrorAction SilentlyContinue
Run-Create "A" | Out-Null
Run-Create "B" | Out-Null
Run-Create "C" | Out-Null
$ids = Get-Ids
Check "ID 중복 없음" (-not (Has-Duplicates $ids)) "IDs: $ids"
Check "ID = [1,2,3]" ("$ids" -eq "1 2 3") "실제: $ids"

# ─── 테스트 2: 삭제 후 추가 (ID 재사용 여부) ───────────────────────
Write-Host "`n[TEST 2] 삭제 후 추가 — ID 2 삭제 뒤 새 항목 추가" -ForegroundColor Cyan
# ID 2 삭제: 메뉴 5 → id=2 → y → 메뉴 0
$input = "5`n2`ny`n0`n"
$input | & $exe 2>&1 | Out-Null
# 새 항목 추가
Run-Create "D" | Out-Null
$ids = Get-Ids
Check "ID 중복 없음" (-not (Has-Duplicates $ids)) "IDs: $ids"
Check "새 ID가 4 (재사용 안 함)" ($ids -contains 4) "실제: $ids"
Check "삭제된 ID 2 재사용 안 함" (-not ($ids -contains 2)) "실제: $ids"

# ─── 테스트 3: 빈 파일에서 시작 ────────────────────────────────────
Write-Host "`n[TEST 3] 빈 파일에서 첫 항목 추가" -ForegroundColor Cyan
Remove-Item $dataFile -ErrorAction SilentlyContinue
Run-Create "E" | Out-Null
$ids = Get-Ids
Check "첫 ID = 1" ($ids[0] -eq 1) "실제: $ids"

# ─── 테스트 4: 비연속 ID (외부 편집) 후 추가 ───────────────────────
Write-Host "`n[TEST 4] 외부 편집으로 비연속 ID [1,5,10] 후 추가" -ForegroundColor Cyan
@'
[
  {"id":1,"name":"X","email":"","phone":"","memo":""},
  {"id":5,"name":"Y","email":"","phone":"","memo":""},
  {"id":10,"name":"Z","email":"","phone":"","memo":""}
]
'@ | Set-Content $dataFile -Encoding utf8
Run-Create "NewAfterGap" | Out-Null
$ids = Get-Ids
Check "ID 중복 없음" (-not (Has-Duplicates $ids)) "IDs: $ids"
Check "새 ID = 11 (max+1)" ($ids[-1] -eq 11) "실제 마지막: $($ids[-1])"

# ─── 테스트 5: 외부 편집으로 중복 ID 삽입 후 추가 ──────────────────
Write-Host "`n[TEST 5] 외부 편집으로 중복 ID [1,1,3] 후 추가" -ForegroundColor Cyan
@'
[
  {"id":1,"name":"Dup1","email":"","phone":"","memo":""},
  {"id":1,"name":"Dup2","email":"","phone":"","memo":""},
  {"id":3,"name":"Normal","email":"","phone":"","memo":""}
]
'@ | Set-Content $dataFile -Encoding utf8
Run-Create "AfterDup" | Out-Null
$ids = Get-Ids
$beforeAdd = @(1, 1, 3)
$dupExists = Has-Duplicates $ids
Check "앱이 기존 중복을 감지하지 못함 (알려진 한계)" $dupExists "IDs: $ids"
$newId = $ids[-1]
Check "새 추가 ID는 중복 없음 (max+1=4)" ($newId -eq 4) "새 ID: $newId"

# ─── 결과 ───────────────────────────────────────────────────────────
Write-Host "`n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
Write-Host "결과: PASS $pass / FAIL $fail"
if ($fail -eq 0) {
    Write-Host "전체 통과" -ForegroundColor Green
} else {
    Write-Host "실패 항목 있음" -ForegroundColor Red
}

Remove-Item $dataFile -ErrorAction SilentlyContinue
