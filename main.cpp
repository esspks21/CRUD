#ifdef _WIN32
#include <windows.h>
#endif

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <iomanip>
#include <algorithm>
#include <limits>
#include <stdexcept>
#include "json.hpp"

using namespace std;
using json = nlohmann::ordered_json;

const string DATA_FILE = "contacts.json";

static const map<string,string> SURNAME_EN = {
    {"김","kim"},{"이","lee"},{"박","park"},{"최","choi"},{"정","jung"},
    {"강","kang"},{"조","cho"},{"윤","yoon"},{"장","jang"},{"임","lim"},
    {"한","han"},{"오","oh"},{"서","seo"},{"신","shin"},{"권","kwon"},
    {"황","hwang"},{"안","ahn"},{"송","song"},{"류","ryu"},{"전","jeon"}
};

// ─── 데이터 모델 ──────────────────────────────────────────────────────
struct Contact {
    string id;          // {이니셜}{숫자}.{성영문}  예) mj4156.kim
    string emp_no;      // YYMMXXXX 8자리 사번       예) 90011234
    string name;
    string email;       // {id}@samsung.com
    string phone;       // 고유값
    string rank;        // CL1~CL4 / Master / Fellow / 상무~회장
    string department;
    string memo;
};

json toJson(const Contact& c) {
    json j;
    j["id"]=c.id; j["emp_no"]=c.emp_no; j["name"]=c.name;
    j["email"]=c.email; j["phone"]=c.phone; j["rank"]=c.rank;
    j["department"]=c.department; j["memo"]=c.memo;
    return j;
}
Contact fromJson(const json& j) {
    return {j.value("id",""),j.value("emp_no",""),j.value("name",""),
            j.value("email",""),j.value("phone",""),j.value("rank",""),
            j.value("department",""),j.value("memo","")};
}

// ─── 파일 I/O ─────────────────────────────────────────────────────────
vector<Contact> loadAll() {
    vector<Contact> v;
    ifstream f(DATA_FILE);
    if (!f.is_open()) return v;
    try {
        json d; f >> d;
        if (d.is_array()) for (const auto& item : d) v.push_back(fromJson(item));
    } catch (const json::exception& e) {
        cerr << "[경고] 파싱 오류: " << e.what() << "\n";
    }
    return v;
}
void saveAll(const vector<Contact>& v) {
    json d = json::array();
    for (const auto& c : v) d.push_back(toJson(c));
    ofstream f(DATA_FILE);
    if (!f.is_open()) throw runtime_error("파일을 열 수 없습니다: " + DATA_FILE);
    f << d.dump(4);
}

// ─── ContactIndex: 해시 기반 O(1) 조회 인덱스 ────────────────────────
// 레코드 집합에서 한 번만 O(n) 구성 → 이후 모든 조회 O(1)
struct ContactIndex {
    unordered_set<string>        ids;
    unordered_set<string>        empNos;
    unordered_set<string>        phones;
    unordered_map<string,size_t> idToPos;
    unordered_map<string,size_t> empNoToPos;

    void build(const vector<Contact>& v) {
        size_t n = v.size();
        ids.reserve(n); empNos.reserve(n); phones.reserve(n);
        idToPos.reserve(n); empNoToPos.reserve(n);
        for (size_t i = 0; i < n; i++) {
            ids.insert(v[i].id);
            empNos.insert(v[i].emp_no);
            if (!v[i].phone.empty()) phones.insert(v[i].phone);
            idToPos[v[i].id]       = i;
            empNoToPos[v[i].emp_no] = i;
        }
    }

    // id 또는 emp_no로 위치 반환, 미발견 시 npos 반환
    size_t findByIdOrEmpNo(const string& q, size_t npos) const {
        auto it = idToPos.find(q);
        if (it != idToPos.end()) return it->second;
        auto it2 = empNoToPos.find(q);
        if (it2 != empNoToPos.end()) return it2->second;
        return npos;
    }
};

// ─── 유틸리티 ─────────────────────────────────────────────────────────
// ContactIndex 기반 O(1) 다음 사번 생성
string nextEmpNo(const ContactIndex& idx, const string& prefix) {
    for (int sfx = 1; sfx <= 9999; sfx++) {
        ostringstream o;
        o << prefix << setw(4) << setfill('0') << sfx;
        if (!idx.empNos.count(o.str())) return o.str();
    }
    throw runtime_error("해당 연월의 사번이 모두 소진되었습니다.");
}

// ContactIndex 기반 O(1) 고유 사용자 ID 생성
string generateUserId(const ContactIndex& idx,
                      const string& initials, const string& surname) {
    for (int num = 1000; num <= 9999; num++) {
        ostringstream o; o << initials << num << "." << surname;
        if (!idx.ids.count(o.str())) return o.str();
    }
    throw runtime_error("사용 가능한 ID가 없습니다: " + initials + "xxxx." + surname);
}

void clearInput() { cin.ignore(numeric_limits<streamsize>::max(), '\n'); }
string getLine(const string& prompt) {
    cout << prompt; string s; getline(cin, s); return s;
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

pair<string,string> splitKoreanName(const string& full) {
    if (full.size() < 3) return {full, ""};
    return {full.substr(0,3), full.substr(3)};
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

    pair<string,string> np = splitKoreanName(name);
    string surnameEn = surnameToEnglish(np.first);
    cout << "성(영문) 자동 매핑: " << surnameEn << "\n";

    string initials = getLine("이름 이니셜 (영문 소문자, 예: mj): ");
    if (initials.empty()) { cout << "[오류] 이니셜은 필수입니다.\n"; return; }
    for (char ch : initials)
        if (!islower((unsigned char)ch)) { cout << "[오류] 소문자 영문자만 입력하세요.\n"; return; }

    string yy = getLine("입사연도 (YY)  : ");
    if (yy.size()!=2||!isdigit(yy[0])||!isdigit(yy[1])) {
        cout << "[오류] 2자리 숫자로 입력하세요. (예: 90~99 또는 00~26)\n"; return;
    }
    int yyInt = stoi(yy);
    if (!((yyInt>=90&&yyInt<=99)||(yyInt>=0&&yyInt<=26))) {
        cout << "[오류] 입사연도는 90~99 또는 00~26이어야 합니다.\n"; return;
    }
    string mm = getLine("입사월   (MM)  : ");
    if (mm.size()!=2||!isdigit(mm[0])||!isdigit(mm[1])) {
        cout << "[오류] 2자리 숫자로 입력하세요. (예: 01~12)\n"; return;
    }
    int mmInt = stoi(mm), maxMm = (yyInt==26)?5:12;
    if (mmInt<1||mmInt>maxMm) {
        cout << "[오류] 입사월은 01~" << setw(2)<<setfill('0')<<maxMm << "이어야 합니다.\n"; return;
    }

    // 인덱스 한 번 구성 → 이후 모든 중복 검사·ID 생성 O(1)
    auto records = loadAll();
    ContactIndex idx; idx.build(records);

    string empNo  = nextEmpNo(idx, yy + mm);
    string userId = generateUserId(idx, initials, surnameEn);
    cout << "사번: " << empNo << "  /  ID: " << userId << "\n";

    if (idx.ids.count(userId)) {
        cout << "[오류] ID '" << userId << "'가 이미 사용 중입니다.\n"; return;
    }
    if (idx.empNos.count(empNo)) {
        cout << "[오류] 사번 '" << empNo << "'이 이미 사용 중입니다.\n"; return;
    }

    string email = getLine("이메일         : ");
    if (email.empty()) email = userId + "@samsung.com";

    string phone = getLine("전화번호       : ");
    if (!phone.empty() && idx.phones.count(phone)) {
        cout << "[오류] 전화번호 '" << phone << "'이 이미 사용 중입니다.\n"; return;
    }

    cout << "직급 선택:\n";
    for (int i=0;i<RANK_COUNT;i++) cout << "  " << (i+1) << ". " << RANK_LIST[i] << "\n";
    cout << "선택: ";
    int rc;
    if (!(cin>>rc)||rc<1||rc>RANK_COUNT) { cout<<"[오류] 잘못된 선택.\n"; clearInput(); return; }
    clearInput();
    string rank = RANK_LIST[rc-1];
    string dept = getLine("부서           : ");
    string memo = getLine("메모           : ");

    records.push_back({userId, empNo, name, email, phone, rank, dept, memo});
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
    if (!(cin>>choice)||choice<1||choice>3) { cout<<"잘못된 선택.\n"; clearInput(); return; }
    clearInput();

    auto records = loadAll();
    ContactIndex idx; idx.build(records);   // O(n) 1회 구성
    vector<Contact> results;

    if (choice == 1) {                      // O(1) 해시 조회
        string q = getLine("검색할 ID: ");
        auto it = idx.idToPos.find(q);
        if (it != idx.idToPos.end()) results.push_back(records[it->second]);
    } else if (choice == 2) {               // O(1) 해시 조회
        string q = getLine("검색할 사번 (8자리): ");
        auto it = idx.empNoToPos.find(q);
        if (it != idx.empNoToPos.end()) results.push_back(records[it->second]);
    } else {                                // O(n) 부분 일치 — 불가피
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
    ContactIndex idx; idx.build(records);

    size_t pos = idx.findByIdOrEmpNo(q, records.size());
    if (pos == records.size()) { cout << "'" << q << "'를 찾을 수 없습니다.\n"; return; }
    Contact& c = records[pos];

    cout << "\n현재 정보:\n";
    printTableHeader(); printContact(c); printDivider();

    cout << "\n수정할 필드:\n"
         << "1. 이름     (" << c.name       << ")\n"
         << "2. 이메일   (" << c.email      << ")\n"
         << "3. 전화번호 (" << c.phone      << ")\n"
         << "4. 직급     (" << c.rank       << ")\n"
         << "5. 부서     (" << c.department << ")\n"
         << "6. 메모     (" << c.memo       << ")\n"
         << "선택: ";
    int field;
    if (!(cin>>field)||field<1||field>6) { cout<<"잘못된 선택.\n"; clearInput(); return; }
    clearInput();

    switch (field) {
        case 1: c.name       = getLine("새 이름     : "); break;
        case 2: c.email      = getLine("새 이메일   : "); break;
        case 3: {
            string np = getLine("새 전화번호 : ");
            // O(1) 중복 검사 (자기 자신 phone 제외)
            if (!np.empty() && idx.phones.count(np) && np != c.phone) {
                cout << "[오류] 전화번호 '" << np << "'이 이미 사용 중입니다.\n"; return;
            }
            c.phone = np;
            break;
        }
        case 4: {
            cout << "직급 선택:\n";
            for (int i=0;i<RANK_COUNT;i++) cout << "  " << (i+1) << ". " << RANK_LIST[i] << "\n";
            cout << "선택: ";
            int rc; cin>>rc; clearInput();
            if (rc>=1&&rc<=RANK_COUNT) c.rank = RANK_LIST[rc-1];
            break;
        }
        case 5: c.department = getLine("새 부서     : "); break;
        case 6: c.memo       = getLine("새 메모     : "); break;
    }

    saveAll(records);
    cout << "[완료] " << c.id << " 연락처가 수정되었습니다.\n";
}

// ─── Delete ───────────────────────────────────────────────────────────
void deleteContact() {
    cout << "\n[DELETE] 연락처 삭제\n";
    clearInput();
    string q = getLine("삭제할 ID 또는 사번: ");

    auto records = loadAll();
    ContactIndex idx; idx.build(records);

    size_t pos = idx.findByIdOrEmpNo(q, records.size());
    if (pos == records.size()) { cout << "'" << q << "'를 찾을 수 없습니다.\n"; return; }

    cout << "\n삭제할 연락처:\n";
    printTableHeader(); printContact(records[pos]); printDivider();

    cout << "정말 삭제하시겠습니까? (y/N): ";
    char confirm; cin >> confirm;
    if (confirm=='y'||confirm=='Y') {
        string del_id = records[pos].id;
        records.erase(records.begin() + pos);
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
        cout << "\n" << string(52,'=') << "\n";
        cout << "      JSON CRUD 연락처 관리 시스템 (삼성전자)\n";
        cout << string(52,'=') << "\n";
        cout << "  1. Create  - 연락처 추가\n";
        cout << "  2. Read    - 전체 목록 보기\n";
        cout << "  3. Search  - 검색 (ID / 사번 / 이름)\n";
        cout << "  4. Update  - 연락처 수정\n";
        cout << "  5. Delete  - 연락처 삭제\n";
        cout << "  0. 종료\n";
        cout << string(52,'-') << "\n";
        cout << "선택: ";

        int choice;
        if (!(cin>>choice)) { cin.clear(); clearInput(); continue; }

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
