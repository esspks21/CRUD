# CRUD 프로젝트 검증 Agent 구성

코드 수정 시마다 아래 4개의 agent를 순서에 따라 실행하여 품질을 보장한다.

---

## 실행 순서

```
SubAgent1 (doc-consistency-validator)
    ↓
SubAgent2 (subagent2-ai-action)
    ↓
SubAgent3 (subagent3-test-verify) ─┐  ← 병렬 실행
SubAgent4 (compliance-verifier)  ─┘
```

---

## Agent 역할

### SubAgent1 — `doc-consistency-validator` (문서 정합성 검증)

코드 변경 후 `README.md` 문서가 실제 구현과 일치하는지 검증한다.

**검증 항목:**
- `README.md`에 기술된 메뉴 구조가 `main.cpp`의 실제 메뉴와 일치하는지
- `README.md`의 기능 설명(Create/Read/Search/Update/Delete)이 구현과 다르지 않은지
- 파일 구조 섹션이 현재 디렉터리 구성과 맞는지
- 데이터 파일 형식(`contacts.json`) 설명이 실제 JSON 스키마와 일치하는지
- 테스트 항목 표가 `test_id.ps1` / `test_id.sh`의 실제 테스트 케이스와 맞는지
- 빌드 요구사항(C++17, g++ 버전 등)이 `CMakeLists.txt`와 일치하는지

**출력:** 불일치 항목 목록 또는 "정합성 확인 완료" 메시지

---

### SubAgent2 — `subagent2-ai-action` (AI Action)

SubAgent1 검증 결과를 토대로 실제 코드 또는 문서를 수정·개선한다.

**처리 내용:**
- SubAgent1이 발견한 문서-코드 불일치를 수정 (README 업데이트 또는 코드 보완)
- 요청된 기능 추가·버그 수정 등 변경 사항을 `main.cpp`에 반영
- 변경 후 영향받는 파일(`README.md`, `CMakeLists.txt` 등) 동기화

**출력:** 변경된 파일 목록과 변경 요약

---

### SubAgent3 — `subagent3-test-verify` (테스트 검증)

*SubAgent4와 병렬 실행 가능*

변경된 코드가 기존 테스트를 통과하는지 확인한다.

**검증 항목:**
- `test_id.ps1` (Windows PowerShell) 5개 테스트 케이스 실행 및 결과 확인
  1. 정상 순차 추가 — ID 1, 2, 3 순서 발급
  2. ID 삭제 후 추가 — 삭제 ID 재사용 없음 (max+1 전략)
  3. 빈 파일에서 첫 항목 추가 — 첫 ID = 1
  4. 비연속 ID 외부 편집 후 추가 — 새 ID = max+1
  5. 중복 ID 외부 편집 후 추가 — 알려진 한계 동작 확인
- 빌드 성공 여부 확인 (`build.ps1` 실행)

**출력:** PASS/FAIL 카운트 및 실패 케이스 상세 내용

---

### SubAgent4 — `compliance-verifier` (컴플라이언스 검증)

*SubAgent3와 병렬 실행 가능*

코드가 프로젝트 규칙 및 C++ 코딩 표준을 준수하는지 검증한다.

**검증 항목:**
- C++17 표준 준수 (`CMakeLists.txt`의 `CMAKE_CXX_STANDARD 17` 설정과 일치)
- 입력 처리: `cin` 실패 시 `clearInput()` 호출 여부
- 파일 I/O: 예외 처리(`try/catch`) 유무
- 데이터 모델: `Contact` 구조체 필드(`id`, `name`, `email`, `phone`, `memo`) 완전성
- JSON 직렬화/역직렬화: `toJson` / `fromJson` 대칭 여부
- 리소스 관리: `ifstream` / `ofstream` 정상 닫힘 여부
- Windows UTF-8 설정 (`SetConsoleOutputCP(CP_UTF8)` 존재 여부)
- 보안: 사용자 입력값이 파일 경로에 직접 삽입되지 않는지

**출력:** 위반 항목 목록 또는 "컴플라이언스 통과" 메시지

---

## 코드 수정 시 실행 규칙

1. `main.cpp`, `CMakeLists.txt`, `README.md`, `test_id.ps1`, `test_id.sh`, `build.ps1` 중 하나라도 수정되면 전체 검증 파이프라인을 실행한다.
2. SubAgent1 → SubAgent2 순서는 항상 직렬로 실행한다.
3. SubAgent2 완료 후 SubAgent3, SubAgent4는 병렬로 실행한다.
4. SubAgent3 또는 SubAgent4 중 하나라도 실패하면 수정 사항을 적용하지 않고 사용자에게 보고한다.

---

## 대상 프로젝트

- **경로:** `C:\reviewer\CRUD`
- **언어:** C++17
- **빌드 시스템:** CMake 3.14+ / PowerShell (`build.ps1`)
- **주요 파일:**
  - `main.cpp` — 핵심 소스 (CRUD 로직)
  - `json.hpp` — nlohmann/json 헤더 전용 라이브러리
  - `contacts.json` — 런타임 데이터 파일 (자동 생성)
  - `README.md` — 사용자 문서
  - `test_id.ps1` / `test_id.sh` — ID 중복 검증 테스트
