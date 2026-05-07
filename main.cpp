#ifdef _WIN32
#include <windows.h>
#endif

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <iomanip>
#include <algorithm>
#include <limits>
#include <stdexcept>
#include "json.hpp"

using namespace std;
using json = nlohmann::json;

const string DATA_FILE = "contacts.json";

// ─── 데이터 모델 ──────────────────────────────────────────────────────
// 사번 형식: YYMMXXXX (앞 2자리=입사연도, 다음 2자리=입사월, 뒤 4자리=순번)
struct Contact {
    string id;          // YYMMXXXX (8자리, 문자열로 저장해 앞 0 보존)
    string name;
    string email;       // xxx@samsung.com
    string phone;
    string rank;        // CL1~CL4 / Master / Fellow / 상무~회장
    string department;
    string memo;
};

// ─── JSON 직렬화 / 역직렬화 ───────────────────────────────────────────
json toJson(const Contact& c) {
    return {
        {"id",         c.id},
        {"name",       c.name},
        {"email",      c.email},
        {"phone",      c.phone},
        {"rank",       c.rank},
        {"department", c.department},
        {"memo",       c.memo}
    };
}

Contact fromJson(const json& j) {
    // id 필드: 구버전 int → 문자열 변환 지원
    string id;
    if (j.contains("id")) {
        if (j["id"].is_string()) id = j["id"].get<string>();
        else                     id = to_string(j["id"].get<int>());
    }
    return {id,
            j.value("name",       ""),
            j.value("email",      ""),
            j.value("phone",      ""),
            j.value("rank",       ""),
            j.value("department", ""),
            j.value("memo",       "")};
}

// ─── 파일 I/O ─────────────────────────────────────────────────────────
vector<Contact> loadAll() {
    vector<Contact> records;
    ifstream file(DATA_FILE);
    if (!file.is_open()) return records;
    try {
        json data;
        file >> data;
        if (data.is_array())
            for (const auto& item : data)
                records.push_back(fromJson(item));
    } catch (const json::exception& e) {
        cerr << "[경고] 데이터 파일 파싱 오류: " << e.what() << "\n";
    }
    return records;
}

void saveAll(const vector<Contact>& records) {
    json data = json::array();
    for (const auto& c : records)
        data.push_back(toJson(c));
    ofstream file(DATA_FILE);
    if (!file.is_open())
        throw runtime_error("파일을 열 수 없습니다: " + DATA_FILE);
    file << data.dump(4);
}

// ─── 유틸리티 ─────────────────────────────────────────────────────────
// YYMM 접두사 기반으로 다음 사번 생성 (YYMMXXXX)
string nextId(const vector<Contact>& records, const string& prefix) {
    int maxSuffix = 0;
    for (const auto& c : records) {
        if (c.id.size() == 8 && c.id.substr(0, 4) == prefix) {
            try {
                int s = stoi(c.id.substr(4));
                if (s > maxSuffix) maxSuffix = s;
            } catch (...) {}
        }
    }
    if (maxSuffix >= 9999)
        throw runtime_error("해당 연월의 사번이 모두 소진되었습니다.");
    ostringstream oss;
    oss << prefix << setw(4) << setfill('0') << (maxSuffix + 1);
    return oss.str();
}

void clearInput() {
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
}

string getLine(const string& prompt) {
    cout << prompt;
    string s;
    getline(cin, s);
    return s;
}

void printDivider(int w = 110) {
    cout << string(w, '-') << "\n";
}

void printTableHeader() {
    printDivider();
    cout << left
         << setw(12) << "사번"
         << setw(14) << "이름"
         << setw(28) << "이메일"
         << setw(16) << "전화번호"
         << setw(12) << "직급"
         << setw(24) << "부서"
         << "메모" << "\n";
    printDivider();
}

void printContact(const Contact& c) {
    cout << left
         << setw(12) << c.id
         << setw(14) << c.name
         << setw(28) << c.email
         << setw(16) << c.phone
         << setw(12) << c.rank
         << setw(24) << c.department
         << c.memo << "\n";
}

static const char* RANK_LIST[] = {
    "CL1","CL2","CL3","CL4","Master","Fellow",
    "상무","부사장","사장","부회장","회장"
};
static const int RANK_COUNT = 11;

// ─── Create ───────────────────────────────────────────────────────────
void createContact() {
    cout << "\n[CREATE] 새 연락처 추가\n";
    printDivider(40);

    clearInput();
    string name = getLine("이름           : ");
    if (name.empty()) { cout << "[오류] 이름은 필수 항목입니다.\n"; return; }

    // 사번 생성을 위해 입사연도·월 먼저 입력
    string yy = getLine("입사연도 (YY)  : ");
    if (yy.size() != 2 || !isdigit(yy[0]) || !isdigit(yy[1])) {
        cout << "[오류] 2자리 숫자로 입력하세요. (예: 90~99 또는 00~26)\n"; return;
    }
    int yyInt = stoi(yy);
    if (!((yyInt >= 90 && yyInt <= 99) || (yyInt >= 0 && yyInt <= 26))) {
        cout << "[오류] 입사연도는 90~99 또는 00~26 이어야 합니다.\n"; return;
    }

    string mm = getLine("입사월   (MM)  : ");
    if (mm.size() != 2 || !isdigit(mm[0]) || !isdigit(mm[1])) {
        cout << "[오류] 2자리 숫자로 입력하세요. (예: 01~12)\n"; return;
    }
    int mmInt = stoi(mm);
    int maxMm = (yyInt == 26) ? 5 : 12;
    if (mmInt < 1 || mmInt > maxMm) {
        cout << "[오류] 입사월은 01~"
             << setw(2) << setfill('0') << maxMm << " 이어야 합니다.\n";
        return;
    }

    string prefix = yy + mm;
    auto records = loadAll();
    string id = nextId(records, prefix);
    cout << "사번이 " << id << " 으로 생성됩니다.\n";

    string email = getLine("이메일         : ");
    if (email.empty()) email = id + "@samsung.com";

    string phone = getLine("전화번호       : ");

    cout << "직급 선택:\n";
    for (int i = 0; i < RANK_COUNT; i++)
        cout << "  " << (i + 1) << ". " << RANK_LIST[i] << "\n";
    cout << "선택: ";
    int rankChoice;
    if (!(cin >> rankChoice) || rankChoice < 1 || rankChoice > RANK_COUNT) {
        cout << "[오류] 잘못된 선택입니다.\n"; clearInput(); return;
    }
    clearInput();
    string rank = RANK_LIST[rankChoice - 1];

    string department = getLine("부서           : ");
    string memo       = getLine("메모           : ");

    Contact c{id, name, email, phone, rank, department, memo};
    records.push_back(c);
    saveAll(records);
    cout << "[완료] 사번 " << id << " 연락처가 추가되었습니다.\n";
}

// ─── Read (전체 목록) ─────────────────────────────────────────────────
void readAll() {
    auto records = loadAll();
    cout << "\n[READ] 전체 연락처 목록 (" << records.size() << "건)\n";
    if (records.empty()) { cout << "등록된 연락처가 없습니다.\n"; return; }
    printTableHeader();
    for (const auto& c : records) printContact(c);
    printDivider();
}

// ─── Search (사번 / 이름) ─────────────────────────────────────────────
void searchContact() {
    cout << "\n[SEARCH] 검색\n";
    cout << "1. 사번으로 검색\n";
    cout << "2. 이름으로 검색\n";
    cout << "선택: ";

    int choice;
    if (!(cin >> choice) || (choice != 1 && choice != 2)) {
        cout << "잘못된 선택입니다.\n"; clearInput(); return;
    }
    clearInput();

    auto records = loadAll();
    vector<Contact> results;

    if (choice == 1) {
        string idStr = getLine("검색할 사번 (8자리): ");
        for (const auto& c : records)
            if (c.id == idStr) results.push_back(c);
    } else {
        string keyword = getLine("검색할 이름: ");
        for (const auto& c : records)
            if (c.name.find(keyword) != string::npos)
                results.push_back(c);
    }

    if (results.empty()) { cout << "검색 결과가 없습니다.\n"; return; }
    cout << "\n검색 결과 " << results.size() << "건:\n";
    printTableHeader();
    for (const auto& c : results) printContact(c);
    printDivider();
}

// ─── Update ───────────────────────────────────────────────────────────
void updateContact() {
    cout << "\n[UPDATE] 연락처 수정\n";
    string idStr = getLine("수정할 사번 (8자리): ");

    auto records = loadAll();
    auto it = find_if(records.begin(), records.end(),
                      [&idStr](const Contact& c){ return c.id == idStr; });
    if (it == records.end()) {
        cout << "사번 " << idStr << "를 찾을 수 없습니다.\n"; return;
    }

    cout << "\n현재 정보:\n";
    printTableHeader();
    printContact(*it);
    printDivider();

    cout << "\n수정할 필드:\n"
         << "1. 이름     (" << it->name       << ")\n"
         << "2. 이메일   (" << it->email      << ")\n"
         << "3. 전화번호 (" << it->phone      << ")\n"
         << "4. 직급     (" << it->rank       << ")\n"
         << "5. 부서     (" << it->department << ")\n"
         << "6. 메모     (" << it->memo       << ")\n"
         << "선택: ";

    int field;
    if (!(cin >> field) || field < 1 || field > 6) {
        cout << "잘못된 선택입니다.\n"; clearInput(); return;
    }
    clearInput();

    switch (field) {
        case 1: it->name       = getLine("새 이름     : "); break;
        case 2: it->email      = getLine("새 이메일   : "); break;
        case 3: it->phone      = getLine("새 전화번호 : "); break;
        case 4: {
            cout << "직급 선택:\n";
            for (int i = 0; i < RANK_COUNT; i++)
                cout << "  " << (i+1) << ". " << RANK_LIST[i] << "\n";
            cout << "선택: ";
            int rc; cin >> rc; clearInput();
            if (rc >= 1 && rc <= RANK_COUNT) it->rank = RANK_LIST[rc-1];
            break;
        }
        case 5: it->department = getLine("새 부서     : "); break;
        case 6: it->memo       = getLine("새 메모     : "); break;
    }

    saveAll(records);
    cout << "[완료] 사번 " << idStr << " 연락처가 수정되었습니다.\n";
}

// ─── Delete ───────────────────────────────────────────────────────────
void deleteContact() {
    cout << "\n[DELETE] 연락처 삭제\n";
    string idStr = getLine("삭제할 사번 (8자리): ");

    auto records = loadAll();
    auto it = find_if(records.begin(), records.end(),
                      [&idStr](const Contact& c){ return c.id == idStr; });
    if (it == records.end()) {
        cout << "사번 " << idStr << "를 찾을 수 없습니다.\n"; return;
    }

    cout << "\n삭제할 연락처:\n";
    printTableHeader();
    printContact(*it);
    printDivider();

    cout << "정말 삭제하시겠습니까? (y/N): ";
    char confirm;
    cin >> confirm;

    if (confirm == 'y' || confirm == 'Y') {
        records.erase(it);
        saveAll(records);
        cout << "[완료] 사번 " << idStr << " 연락처가 삭제되었습니다.\n";
    } else {
        cout << "삭제가 취소되었습니다.\n";
    }
}

// ─── 메인 메뉴 ────────────────────────────────────────────────────────
int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    cout << "데이터 저장 위치: " << DATA_FILE << "\n";

    while (true) {
        cout << "\n" << string(52, '=') << "\n";
        cout << "      JSON CRUD 연락처 관리 시스템 (삼성전자)\n";
        cout << string(52, '=') << "\n";
        cout << "  1. Create  - 연락처 추가\n";
        cout << "  2. Read    - 전체 목록 보기\n";
        cout << "  3. Search  - 검색 (사번 / 이름)\n";
        cout << "  4. Update  - 연락처 수정\n";
        cout << "  5. Delete  - 연락처 삭제\n";
        cout << "  0. 종료\n";
        cout << string(52, '-') << "\n";
        cout << "선택: ";

        int choice;
        if (!(cin >> choice)) {
            cin.clear(); clearInput(); continue;
        }

        try {
            switch (choice) {
                case 1: createContact(); break;
                case 2: readAll();       break;
                case 3: searchContact(); break;
                case 4: updateContact(); break;
                case 5: deleteContact(); break;
                case 0: cout << "프로그램을 종료합니다.\n"; return 0;
                default: cout << "잘못된 선택입니다. 0~5 사이의 숫자를 입력하세요.\n";
            }
        } catch (const exception& e) {
            cerr << "[오류] " << e.what() << "\n";
        }
    }
}
