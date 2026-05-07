# Regression 테스트

100,000개 랜덤 CRUD 연산으로 핵심 로직을 검증하는 자동화 회귀 테스트.

---

## 목적

코드 수정 시마다 Create / Read / Update / Delete 로직이 깨지지 않았음을 확인하고, 통과한 경우에만 GitHub에 push한다.

---

## 파일 구성

| 파일 | 역할 |
|------|------|
| `regression_test.cpp` | 랜덤 CRUD 시나리오 생성 및 검증 실행 |
| `regression_test.exe` | 빌드 산출물 (`.gitignore` 대상) |
| `regression_contacts.json` | 테스트 전용 임시 파일 (자동 생성·삭제) |
| `TC/{타임스탬프}/` | 실행별 TC 출력 디렉터리 (`.gitignore` 대상) |

---

## 동작 방식

### 1. 워밍업 (1,000개 Create)

메인 연산 전 초기 DB를 구성한다.  
이 시점의 상태가 `TC/{ts}/input.json` 으로 저장된다.

### 2. 고속 인메모리 DB

파일 I/O 없이 100,000번의 연산을 처리하기 위해 인메모리 DB를 사용한다.

| 연산 | 복잡도 | 구조 |
|------|--------|------|
| Create | O(1) | `unordered_map` + `vector` |
| Read | O(1) | `unordered_map` 직접 조회 |
| Update | O(1) | `unordered_map` 직접 수정 |
| Delete | O(1) | swap-and-pop 기법 |
| 랜덤 선택 | O(1) | 인덱스 풀 벡터 직접 접근 |

### 3. 연산 시퀀스 구성 및 셔플

```
CREATE  × 25,000 ┐
READ    × 25,000 ├─ 총 100,000개 → shuffle(seed=12345)
UPDATE  × 25,000 │    → 완전 랜덤 순서로 1개씩 실행
DELETE  × 25,000 ┘
```

- 고정 시드(`seed=12345`)로 실행 환경이 달라도 동일한 순서 보장
- DB가 비어있을 때 Read/Update/Delete 시도 → Create로 자동 전환 (빈 DB 방지)

### 4. 검증 항목 (총 ~150,023 체크)

| 단계 | 검증 내용 | 체크 수 |
|------|-----------|---------|
| CREATE × 25K | 생성된 ID가 DB에 존재 | 25,000 |
| READ × 25K | 선택한 ID 조회 성공 | 25,000 |
| UPDATE × 25K | 변경 성공 + 값 반영 확인 | 50,000 |
| DELETE × 25K | 삭제 성공 + 부재 확인 | 50,000 |
| 체크포인트 × 10 | in-memory ↔ file 크기·데이터 일치 | 20 |
| 최종 무결성 | 레코드 수·데이터·ID 중복 없음 | 3 |

### 5. 체크포인트 (10,000 연산마다)

매 10,000번째 연산 후 DB 상태를 파일로 저장하고 다시 읽어 인메모리 상태와 비교한다.  
파일 직렬화·역직렬화 정확성을 검증한다.

### 6. 임시 파일 격리

- 테스트 전용 파일 `regression_contacts.json` 사용 → `contacts.json` 불변
- 테스트 종료 시 자동 삭제

---

## TC 출력 파일

실행마다 `TC/{YYYYMMDD_HHMMSS}/` 디렉터리가 생성된다.

| 파일 | 내용 | 크기(참고) |
|------|------|------------|
| `input.json` | 워밍업 완료 후 초기 1,000개 레코드 | ~160 KB |
| `operations_sample.json` | 처음 100 + 마지막 100 연산 (before/after 포함) | ~85 KB |
| `output.json` | 최종 DB 상태 전체 | ~160 KB |
| `summary.txt` | PASS/FAIL 카운트 및 연산 통계 | < 1 KB |

`TC/` 는 `.gitignore` 에 등록되어 로컬 확인 전용이다.

---

## 빌드 및 실행

```powershell
# 빌드
g++ -std=c++17 -O2 -Wall -o regression_test.exe regression_test.cpp

# 실행
.\regression_test.exe
```

출력 예시:

```
[WARMUP] 초기 1000개 Create
[RUN] 100000개 랜덤 연산 (CREATE/READ/UPDATE/DELETE 각 25000개)
  체크포인트 1 (10000/100000) DB=1046건 ✓
  ...
  체크포인트 10 (100000/100000) DB=1000건 ✓
최종 무결성 검증 (DB=1000건)...

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
Regression 결과: PASS 150023 / FAIL 0
  CREATE : 25000 / 25000
  READ   : 25000 / 25000
  UPDATE : 25000 / 25000
  DELETE : 25000 / 25000
  DB 최종 : 1000건
전체 통과
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
TC 출력 경로: TC\20260507_154301
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
    ↓ 종료 코드 0 (FAIL 0) 시
[4] git add → git commit → git push origin main
```

실패 시: `[FAIL]` 항목을 출력하고 push 중단, 사용자에게 보고.

---

## 알려진 한계

| 한계 | 내용 |
|------|------|
| 파일 I/O 테스트 범위 | 연산 자체는 인메모리. 파일 정확성은 체크포인트(10회) + 최종(1회)에서만 검증 |
| 동시성 | 단일 스레드 순차 실행만 검증 |
| 외부 편집 중복 ID | 앱이 기존 중복 ID를 감지하지 않는 알려진 한계 (test_id.ps1 STEP 5 참조) |
