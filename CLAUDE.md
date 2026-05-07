# CRUD 프로젝트

C++17 기반 콘솔 CRUD 연락처 관리 시스템.

## 프로젝트 개요

- **언어:** C++17
- **빌드:** CMake 3.14+ (`CMakeLists.txt`) 또는 PowerShell (`build.ps1`)
- **데이터:** `contacts.json` (실행 디렉터리에 자동 생성)
- **JSON 라이브러리:** nlohmann/json (`json.hpp`, 헤더 전용)
- **플랫폼:** Windows 10/11 (UTF-8 콘솔 설정 포함)

## 주요 파일

| 파일 | 역할 |
|------|------|
| `main.cpp` | CRUD 전체 로직 |
| `json.hpp` | nlohmann/json 라이브러리 |
| `CMakeLists.txt` | CMake 빌드 설정 |
| `build.ps1` | PowerShell 빌드 스크립트 |
| `test_id.ps1` | ID 중복 검증 테스트 (Windows) |
| `test_id.sh` | ID 중복 검증 테스트 (Bash) |
| `README.md` | 사용자 문서 |
| `agents/Verify.md` | Agent 검증 파이프라인 정의 |

## 코드 수정 시 필수 검증 절차

**`main.cpp`, `CMakeLists.txt`, `README.md`, `test_id.ps1`, `test_id.sh`, `build.ps1` 중 하나라도 수정하면 아래 4개 agent를 반드시 실행한다.**

자세한 내용 → [`agents/Verify.md`](agents/Verify.md)

### 실행 순서

```
1. doc-consistency-validator   ← 문서-코드 정합성 검증 (직렬)
2. subagent2-ai-action         ← AI 수정/개선 적용 (직렬)
3. subagent3-test-verify  ┐   ← 테스트 검증 (병렬)
   compliance-verifier    ┘   ← 컴플라이언스 검증 (병렬)
```

### Agent 요약

| Agent | 역할 |
|-------|------|
| `doc-consistency-validator` | README와 코드 간 불일치 탐지 |
| `subagent2-ai-action` | 불일치 수정 및 코드 변경 적용 |
| `subagent3-test-verify` | `test_id.ps1` 테스트 실행 및 빌드 확인 |
| `compliance-verifier` | C++17 표준·보안·코딩 규칙 준수 확인 |

## 데이터 모델

```cpp
struct Contact {
    int    id;
    string name;   // 필수
    string email;
    string phone;
    string memo;
};
```

- ID는 `max(existing_ids) + 1` 전략으로 자동 채번 (재사용 없음)
- 이름만 필수 항목, 나머지는 빈 문자열 허용

## 빌드 및 테스트

```powershell
# 빌드
.\build.ps1

# 테스트 실행
powershell -ExecutionPolicy Bypass -File .\test_id.ps1
```
