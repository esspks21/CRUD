# Regression 테스트

CRUD 로직의 정확성을 100개 랜덤 데이터셋으로 검증하는 자동화 회귀 테스트.

---

## 목적

코드 수정 시마다 Create / Read / Update / Delete 핵심 로직이 깨지지 않았음을 자동으로 확인하고, 통과한 경우에만 GitHub에 push한다.

---

## 파일 구성

| 파일 | 역할 |
|------|------|
| `regression_test.cpp` | 랜덤 CRUD 시나리오 생성 및 검증 실행 |
| `regression_test.exe` | 빌드 산출물 (`.gitignore` 대상) |
| `regression_contacts.json` | 테스트 전용 임시 데이터 파일 (자동 생성·삭제) |

---

## 동작 방식

### 1. 랜덤 데이터 생성

`regression_test.cpp` 내 `Gen` 구조체가 **고정 시드(seed=12345)** 기반 `mt19937`으로 데이터를 생성한다.  
고정 시드를 사용하므로 실행 환경이 달라도 **동일한 입력·출력 시퀀스**가 보장된다.

생성 항목:
- `name` : 성(10종) + 이름(10종) 조합
- `email` : `user{i}@{domain}` 형식, 도메인 4종 랜덤 선택
- `phone` : `010-{1000+i}-{랜덤4자리}`
- `memo` : 팀명 5종 + 빈 문자열 중 랜덤 선택

### 2. 테스트 단계 (총 ~128개 체크)

```
STEP 1 — Create 100개
  ├─ 레코드 수 = 100 확인
  ├─ ID 중복 없음 확인
  └─ ID 시퀀스 1~100 확인

STEP 2 — 랜덤 Read 30회
  └─ 각 ID가 파일에 존재하는지 확인

STEP 3 — 랜덤 Update 20회
  ├─ 각 Update 성공 여부 확인
  └─ 변경 값이 파일에 실제 반영되었는지 확인 (20×2 = 40 체크)

STEP 4 — 랜덤 Delete 25회
  ├─ 각 Delete 성공 여부 확인
  └─ 삭제된 ID가 파일에서 실제로 사라졌는지 확인 (25×2 = 50 체크)

STEP 5 — Delete 후 Create
  └─ 새 ID = 현재 max+1 (재사용 없음) 확인

STEP 6 — 최종 무결성
  ├─ 최종 레코드 수 = 76 (100 - 25 + 1) 확인
  ├─ 모든 레코드에 name 필드 존재 확인
  ├─ 최종 ID 중복 없음 확인
  └─ 삭제된 25개 ID 완전 부재 확인
```

### 3. 임시 파일 격리

테스트는 `regression_contacts.json` 을 전용 파일로 사용하며, **실제 운영 데이터 `contacts.json` 을 건드리지 않는다**.  
테스트 종료 시 `regression_contacts.json` 은 자동 삭제된다.

### 4. 종료 코드

| 종료 코드 | 의미 |
|-----------|------|
| `0` | 전체 통과 (push 허용) |
| `1` | 1개 이상 실패 (push 차단) |

---

## 빌드 방법

### g++ (PowerShell)

```powershell
g++ -std=c++17 -O2 -Wall -o regression_test.exe regression_test.cpp
```

### CMake

```powershell
cmake --build out/build/x64-Debug --target regression_test
```

---

## 실행 방법

```powershell
.\regression_test.exe
```

출력 예시 (전체 통과):

```
[STEP 1] 100개 연락처 Create
[STEP 2] 랜덤 Read 검증 (30회)
[STEP 3] 랜덤 Update (20회)
[STEP 4] 랜덤 Delete (25회)
[STEP 5] Delete 후 Create - max+1 ID 전략
[STEP 6] 최종 무결성 검증

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
Regression 결과: PASS 128 / FAIL 0
전체 통과
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
```

---

## 코드 수정 후 push 프로세스

```
코드 수정
    ↓
[1] SubAgent1~4 검증 파이프라인 (Verify.md 참조)
    ↓ 전체 통과 시
[2] regression_test 빌드
    g++ -std=c++17 -O2 -Wall -o regression_test.exe regression_test.cpp
    ↓ 빌드 성공 시
[3] regression_test 실행
    .\regression_test.exe
    ↓ 종료 코드 0 (전체 통과) 시
[4] 사용자에게 push 방식 확인 (직접 push / PR 생성)
    ↓ 사용자 승인
[5] git add → git commit → git push
```

실패 시 동작:
- **빌드 실패**: 컴파일 오류 내용을 출력하고 push 중단
- **테스트 실패**: `[FAIL]` 항목을 출력하고 push 중단, 사용자에게 보고

---

## 알려진 한계

| 한계 | 내용 |
|------|------|
| 외부 편집 중복 ID | `test_id.ps1` STEP 5와 동일. 앱이 기존 중복 ID를 감지하지 않음 |
| 동시성 | 단일 스레드 순차 실행만 검증. 동시 파일 접근 시나리오 미포함 |
| 대용량 | 100건 기준 검증. 수만 건 이상의 성능 검증은 별도 필요 |
