# JSON CRUD 연락처 관리 시스템 (삼성전자)

C++17로 작성된 콘솔 기반 CRUD 애플리케이션. 데이터를 `contacts.json` 파일로 영구 저장한다.

---

## 요구사항

| 항목 | 버전 |
|------|------|
| g++ (MinGW) | 6.3 이상 |
| C++ 표준 | C++17 |
| OS | Windows 10/11 |

---

## 빌드 및 실행

```powershell
# 메인 앱 빌드 및 실행
.\build.ps1
.\crud_app.exe

# Regression 테스트 빌드 및 실행
g++ -std=c++17 -O2 -Wall -o regression_test.exe regression_test.cpp
.\regression_test.exe
```

> `json.hpp`가 없으면 아래 명령으로 직접 다운로드한다.
> ```powershell
> Invoke-WebRequest -Uri "https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp" -OutFile json.hpp
> ```

---

## 데이터 모델

| 필드 | 타입 | 설명 | 고유 |
|------|------|------|------|
| `id` | string | `{이니셜}{숫자}.{성영문}` 형식 (예: `mj4156.kim`) | ✓ |
| `emp_no` | string | 8자리 사번 `YYMMXXXX` (예: `90011234`) | ✓ |
| `name` | string | 한글 이름 | |
| `email` | string | `{id}@samsung.com` | |
| `phone` | string | 전화번호 | ✓ |
| `rank` | string | CL1 / CL2 / CL3 / CL4 / Master / Fellow / 상무 / 부사장 / 사장 / 부회장 / 회장 | |
| `department` | string | 소속 부서 | |
| `memo` | string | 개인 메모 | |

---

## 메뉴 구조

```
====================================================
      JSON CRUD 연락처 관리 시스템 (삼성전자)
====================================================
  1. Create  - 연락처 추가
  2. Read    - 전체 목록 보기
  3. Search  - 검색 (ID / 사번 / 이름)
  4. Update  - 연락처 수정
  5. Delete  - 연락처 삭제
  0. 종료
```

---

## 기능별 사용법

### 1. Create — 연락처 추가

```
선택: 1

[CREATE] 새 연락처 추가
----------------------------------------
이름           : 김민준
성(영문) 자동 매핑: kim
이름 이니셜 (영문 소문자, 예: mj): mj
입사연도 (YY)  : 01          ← 2001년 입사
입사월   (MM)  : 03
사번: 01030001  /  ID: mj1234.kim
이메일         :             ← 비워두면 mj1234.kim@samsung.com 자동 설정
전화번호       : 010-1234-5678
직급 선택:
  1. CL1  2. CL2  ...  11. 회장
선택: 2
부서           : DRAM 설계팀
메모           : 박사 (KAIST)

[완료] mj1234.kim (01030001) 연락처가 추가되었습니다.
```

- `id`, `emp_no`, `phone` 은 중복 입력 시 오류 처리된다.
- 이름(필수), 이니셜(필수) 외 항목은 빈 값 허용.
- 이메일 미입력 시 `{id}@samsung.com` 자동 설정.

---

### 2. Read — 전체 목록 보기

```
선택: 2

[READ] 전체 연락처 목록 (2건)
-----------------------------------------------------------------------------------------------------------------------------------
ID                     사번        이름          이메일                       전화번호        직급      부서                   메모
-----------------------------------------------------------------------------------------------------------------------------------
mj1234.kim             01030001    김민준        mj1234.kim@samsung.com       010-1234-5678   CL2       DRAM 설계팀            박사 (KAIST)
sy5678.lee             9907xxxx    이서연        sy5678.lee@samsung.com       010-9876-5432   CL3       AP 설계팀
-----------------------------------------------------------------------------------------------------------------------------------
```

---

### 3. Search — 검색

3가지 방식으로 검색 가능하다.

#### 3-1. ID로 검색 — O(1)

```
선택: 3

[SEARCH] 검색
1. ID로 검색      (예: mj4156.kim)
2. 사번으로 검색  (예: 90011234)
3. 이름으로 검색
선택: 1
검색할 ID: mj1234.kim
```

#### 3-2. 사번으로 검색 — O(1)

```
선택: 2
검색할 사번 (8자리): 01030001
```

#### 3-3. 이름으로 검색 (부분 일치)

```
선택: 3
검색할 이름: 김
```

---

### 4. Update — 연락처 수정

ID 또는 사번으로 레코드를 찾고 필드를 수정한다.

```
선택: 4

[UPDATE] 연락처 수정
수정할 ID 또는 사번: mj1234.kim

현재 정보: (테이블 출력)

수정할 필드:
1. 이름     (김민준)
2. 이메일   (mj1234.kim@samsung.com)
3. 전화번호 (010-1234-5678)
4. 직급     (CL2)
5. 부서     (DRAM 설계팀)
6. 메모     (박사 (KAIST))
선택: 3
새 전화번호 : 010-0000-9999

[완료] mj1234.kim 연락처가 수정되었습니다.
```

- 한 번에 필드 하나만 수정된다.
- 전화번호는 다른 레코드와 중복 불가.
- `id` 와 `emp_no` 는 수정 불가.

---

### 5. Delete — 연락처 삭제

ID 또는 사번으로 레코드를 찾고 삭제한다.

```
선택: 5

[DELETE] 연락처 삭제
삭제할 ID 또는 사번: 01030001

삭제할 연락처: (테이블 출력)
정말 삭제하시겠습니까? (y/N): y

[완료] mj1234.kim 연락처가 삭제되었습니다.
```

- `y` 또는 `Y` 이외의 입력은 모두 취소로 처리된다.

---

## 데이터 파일 형식

실행 파일과 같은 디렉터리에 `contacts.json`이 자동 생성된다.

```json
[
    {
        "id": "mj1234.kim",
        "emp_no": "01030001",
        "name": "김민준",
        "email": "mj1234.kim@samsung.com",
        "phone": "010-1234-5678",
        "rank": "CL2",
        "department": "DRAM 설계팀",
        "memo": "박사 (KAIST)"
    }
]
```

> `contacts.json`을 직접 편집해도 되지만, `id` · `emp_no` · `phone` 값이 중복되지 않도록 주의한다.

---

## 테스트

### ID 중복 검증 테스트 (`test_id.ps1`)

```powershell
powershell -ExecutionPolicy Bypass -File .\test_id.ps1
```

| # | 시나리오 | 확인 내용 |
|---|----------|-----------|
| 1 | 정상 순차 추가 (A → B → C) | emp_no 순서대로 발급되는지 |
| 2 | 삭제 후 새 항목 추가 | 삭제된 emp_no가 재사용되지 않는지 |
| 3 | 빈 파일에서 첫 항목 추가 | 첫 항목이 정상 추가되는지 |
| 4 | 외부 편집으로 비연속 ID 삽입 후 추가 | 새 ID가 max+1로 발급되는지 |
| 5 | 외부 편집으로 중복 ID 삽입 후 추가 | 앱이 기존 중복을 감지하지 못하는 알려진 한계 확인 |

### Regression 테스트 (`regression_test.cpp`)

100,000개 랜덤 CRUD 연산 검증. CREATE / READ / UPDATE / DELETE 각 25,000개를 셔플하여 실행한다.

```powershell
g++ -std=c++17 -O2 -Wall -o regression_test.exe regression_test.cpp
.\regression_test.exe
```

- 실행 결과는 `TC/{타임스탬프}/` 에 저장된다 (`.gitignore` 적용).
- 자세한 내용은 [`agents/regression.md`](agents/regression.md) 참조.

---

## 파일 구조

```
CRUD/
├── main.cpp              # 소스 코드 (ContactIndex 기반 O(1) CRUD)
├── regression_test.cpp   # 100K 랜덤 CRUD Regression 테스트
├── json.hpp              # nlohmann/json 라이브러리 (헤더 전용)
├── crud_app.exe          # 빌드된 실행 파일
├── regression_test.exe   # Regression 테스트 실행 파일
├── build.ps1             # 빌드 스크립트
├── CMakeLists.txt        # CMake 설정
├── test_id.ps1           # ID 중복 테스트 (PowerShell)
├── test_id.sh            # ID 중복 테스트 (Bash)
├── contacts.json         # 데이터 저장 파일 (자동 생성)
├── agents/
│   ├── Verify.md         # Agent 검증 파이프라인 정의
│   └── regression.md     # Regression 테스트 동작 방식
└── TC/                   # Regression TC 출력 (gitignore)
    └── {YYYYMMDD_HHMMSS}/
        ├── input.json
        ├── output.json
        ├── operations_sample.json
        └── summary.txt
```
