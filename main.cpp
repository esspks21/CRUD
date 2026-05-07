#ifdef _WIN32
#include <windows.h>
#endif

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <iomanip>
#include <algorithm>
#include <limits>
#include <stdexcept>
#include "json.hpp"

using namespace std;
using json = nlohmann::json;

const string DATA_FILE = "contacts.json";

// ─── 성(姓) 한글 → 영문 매핑 ─────────────────────────────────────────
static const map<string,string> SURNAME_EN = {
    {"김","kim"},{"이","lee"},{"박","park"},{"최","choi"},{"정","jung"},
    {"강","kang"},{"조","cho"},{"윤","yoon"},{"장","jang"},{"임","lim"},
    {"한","han"},{"오","oh"},{"서","seo"},{"신","shin"},{"권","kwon"},
    {"황","hwang"},{"안","ahn"},{"송","song"},{"류","ryu"},{"전","jeon"}
};

// ─── 데이터 모델 ──────────────────────────────────────────────────────
// id     : {이니셜}{숫자}.{성영문}   예) mj4156.kim
// emp_no : YYMMXXXX (8자리 사번)    예) 90011234
struct Contact {
    string id;          // mj4156.kim
    string emp_no;      // 90011234
    string name;
    string email;       // {id}@samsung.com
    string phone;
    string rank;
    string department;
    string memo;
};

// ─── JSON 직렬화 / 역직렬화 ───────────────────────────────────────────
json toJson(const Contact& c) {
    return {{"id",c.id},{"emp_no",c.emp_no},{"name",c.name},
            {"email",c.email},{"phone",c.phone},{"rank",c.rank},
            {"department",c.department},{"memo",c.memo}};
}

Contact fromJson(const json& j) {
    return {j.value("id",""), j.value("emp_no",""), j.value("name",""),
            j.value("email",""), j.value("phone",""), j.value("rank",""),
            j.value("department",""), j.value("memo","")};
}

// ─── 파일 I/O ─────────────────────────────────────────────────────────
vector<Contact> loadAll() {
    vector<Contact> records;
    ifstream file(DATA_FILE);
    if (!file.is_open()) return records;
    try {
        json data; file >> data;
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
    for (const auto& c : records) data.push_back(toJson(c));
    ofstream file(DATA_FILE);
    if (!file.is_open())
        throw runtime_error("파일을 열 수 없습니다: " + DATA_FILE);
    file << data.dump(4);
}

// ─── 유틸리티 ─────────────────────────────────────────────────────────
void clearInput() { cin.ignore(numeric_limits<streamsize>::max(), '\n'); }

string getLine(const string& prompt) {
    cout << prompt;
    string s; getline(cin, s); return s;
}

void printDivider(int w = 120) { cout << string(w, '-') << "\n"; }

void printTableHeader() {
    printDivider();
    cout << left
         << setw(22) << "ID"
         << setw(12) << "사번"
         << setw(14) << "이름"
         << setw(28) << "이메일"
         << setw(16) << "전화번호"
         << setw(10) << "직급"
         << setw(22) << "부서"
         << "메모" << "\n";
    printDivider();
}

void printContact(const Contact& c) {
    cout << left
         << setw(22) << c.id
         << setw(12) << c.emp_no
         << setw(14) << c.name
         << setw(28) << c.email
         << setw(16) << c.phone
         << setw(10) << c.rank
         << setw(22) << c.department
         << c.memo << "\n";
}

// YYMM 접두사 기반 다음 사번(YYMMXXXX) 생성
string nextEmpNo(const vector<Contact>& records, const string& prefix) {
    int maxSuffix = 0;
    for (const auto& c : records) {
        if (c.emp_no.size() == 8 && c.emp_no.substr(0, 4) == prefix) {
            try {
                int s = stoi(c.emp_no.substr(4));
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

// 이니셜+숫자.성영문 형식의 고유 ID 생성 (초기값 1000부터 순차 탐색)
string generateUserId(const vector<Contact>& records,
                      const string& initials, const string& surname) {
    for (int num = 1000; num <= 9999; num++) {
        ostringstream oss;
        oss << initials << num << "." << surname;
        string candidate = oss.str();
        bool exists = false;
        for (const auto& c : records)
            if (c.id == candidate) { exists = true; break; }
        if (!exists) return candidate;
    }
    throw runtime_error("사용 가능한 ID가 없습니다: " + initials + "xxxx." + surname);
}

// UTF-8 기준 첫 3바이트(한글 1자) = 성, 나머지 = 이름
pair<string,string> splitKoreanName(const string& full) {
    if (full.size() < 3) return {full, ""};
    return {full.substr(0, 3), full.substr(3)};
}

string surnameToEnglish(const string& kr) {
    auto it = SURNAME_EN.find(kr);
    return it != SURNAME_EN.end() ? it->second : "unknown";
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

    pair<string,string> nameParts = splitKoreanName(name);
    string surnameEn = surnameToEnglish(nameParts.first);
    cout << "성(영문) 자동 매핑: " << surnameEn << "\n";

    string initials = getLine("이름 이니셜 (영문 소문자, 예: mj): ");
    if (initials.empty()) { cout << "[오류] 이니셜은 필수입니다.\n"; return; }
    for (char ch : initials)
        if (!islower((unsigned char)ch)) {
            cout << "[오류] 소문자 영문자만 입력하세요.\n"; return;
        }

    // 사번 생성
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
        cout << "[오류] 2자리 숫자로 입력하세요.\n"; return;
    }
    int mmInt = stoi(mm);
    int maxMm = (yyInt == 26) ? 5 : 12;
    if (mmInt < 1 || mmInt > maxMm) {
        cout << "[오류] 입사월은 01~"
             << setw(2) << setfill('0') << maxMm << " 이어야 합니다.\n"; return;
    }

    auto records = loadAll();
    string empNo = nextEmpNo(records, yy + mm);
    string userId = generateUserId(records, initials, surnameEn);
    cout << "사번: " << empNo << "  /  ID: " << userId << "\n";

    string email = getLine("이메일         : ");
    if (email.empty()) email = userId + "@samsung.com";
    string phone = getLine("전화번호       : ");

    cout << "직급 선택:\n";
    for (int i = 0; i < RANK_COUNT; i++)
        cout << "  " << (i+1) << ". " << RANK_LIST[i] << "\n";
    cout << "선택: ";
    int rc;
    if (!(cin >> rc) || rc < 1 || rc > RANK_COUNT) {
        cout << "[오류] 잘못된 선택입니다.\n"; clearInput(); return;
    }
    clearInput();
    string rank = RANK_LIST[rc-1];
    string dept = getLine("부서           : ");
    string memo = getLine("메모           : ");

    Contact c{userId, empNo, name, email, phone, rank, dept, memo};
    records.push_back(c);
    saveAll(records);
    cout << "[완료] " << userId << " (" << empNo << ") 연락처가 추가되었습니다.\n";
}

// ─── Read ─────────────────────────────────────────────────────────────
void readAll() {
    auto records = loadAll();
    cout << "\n[READ] 전체 연락처 목록 (" << records.size() << "건)\n";
    if (records.empty()) { cout << "등록된 연락처가 없습니다.\n"; return; }
    printTableHeader();
    for (const auto& c : records) printContact(c);
    printDivider();
}

// ─── Search (ID / 사번 / 이름) ────────────────────────────────────────
void searchContact() {
    cout << "\n[SEARCH] 검색\n"
         << "1. ID로 검색      (예: mj4156.kim)\n"
         << "2. 사번으로 검색  (예: 90011234)\n"
         << "3. 이름으로 검색\n"
         << "선택: ";
    int choice;
    if (!(cin >> choice) || choice < 1 || choice > 3) {
        cout << "잘못된 선택입니다.\n"; clearInput(); return;
    }
    clearInput();

    auto records = loadAll();
    vector<Contact> results;

    if (choice == 1) {
        string q = getLine("검색할 ID: ");
        for (const auto& c : records)
            if (c.id == q) results.push_back(c);
    } else if (choice == 2) {
        string q = getLine("검색할 사번 (8자리): ");
        for (const auto& c : records)
            if (c.emp_no == q) results.push_back(c);
    } else {
        string q = getLine("검색할 이름: ");
        for (const auto& c : records)
            if (c.name.find(q) != string::npos) results.push_back(c);
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
    clearInput();
    string q = getLine("수정할 ID 또는 사번: ");

    auto records = loadAll();
    auto it = find_if(records.begin(), records.end(),
                      [&q](const Contact& c){ return c.id==q || c.emp_no==q; });
    if (it == records.end()) {
        cout << "'" << q << "'를 찾을 수 없습니다.\n"; return;
    }

    cout << "\n현재 정보:\n";
    printTableHeader(); printContact(*it); printDivider();

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
    cout << "[완료] " << it->id << " 연락처가 수정되었습니다.\n";
}

// ─── Delete ───────────────────────────────────────────────────────────
void deleteContact() {
    cout << "\n[DELETE] 연락처 삭제\n";
    clearInput();
    string q = getLine("삭제할 ID 또는 사번: ");

    auto records = loadAll();
    auto it = find_if(records.begin(), records.end(),
                      [&q](const Contact& c){ return c.id==q || c.emp_no==q; });
    if (it == records.end()) {
        cout << "'" << q << "'를 찾을 수 없습니다.\n"; return;
    }

    cout << "\n삭제할 연락처:\n";
    printTableHeader(); printContact(*it); printDivider();

    cout << "정말 삭제하시겠습니까? (y/N): ";
    char confirm; cin >> confirm;
    if (confirm == 'y' || confirm == 'Y') {
        string del_id = it->id;
        records.erase(it);
        saveAll(records);
        cout << "[완료] " << del_id << " 연락처가 삭제되었습니다.\n";
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
        cout << "  3. Search  - 검색 (ID / 사번 / 이름)\n";
        cout << "  4. Update  - 연락처 수정\n";
        cout << "  5. Delete  - 연락처 삭제\n";
        cout << "  0. 종료\n";
        cout << string(52, '-') << "\n";
        cout << "선택: ";

        int choice;
        if (!(cin >> choice)) { cin.clear(); clearInput(); continue; }

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
