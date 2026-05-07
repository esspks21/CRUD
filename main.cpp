#ifdef _WIN32
#include <windows.h>
#endif

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <iomanip>
#include <algorithm>
#include <limits>
#include <stdexcept>
#include "json.hpp"

using json = nlohmann::json;

// ─────────────────────────────────────────
// 데이터 모델
// ─────────────────────────────────────────
const std::string DATA_FILE = "contacts.json";

struct Contact {
    int         id;
    std::string name;
    std::string email;
    std::string phone;
    std::string memo;
};

// ─────────────────────────────────────────
// JSON 직렬화 / 역직렬화
// ─────────────────────────────────────────
json toJson(const Contact& c) {
    return {
        {"id",    c.id},
        {"name",  c.name},
        {"email", c.email},
        {"phone", c.phone},
        {"memo",  c.memo}
    };
}

Contact fromJson(const json& j) {
    return {
        j.value("id",    0),
        j.value("name",  ""),
        j.value("email", ""),
        j.value("phone", ""),
        j.value("memo",  "")
    };
}

// ─────────────────────────────────────────
// 파일 I/O
// ─────────────────────────────────────────
std::vector<Contact> loadAll() {
    std::vector<Contact> records;
    std::ifstream file(DATA_FILE);
    if (!file.is_open()) return records;

    try {
        json data;
        file >> data;
        if (data.is_array())
            for (const auto& item : data)
                records.push_back(fromJson(item));
    } catch (const json::exception& e) {
        std::cerr << "[경고] 데이터 파일 파싱 오류: " << e.what() << "\n";
    }
    return records;
}

void saveAll(const std::vector<Contact>& records) {
    json data = json::array();
    for (const auto& c : records)
        data.push_back(toJson(c));

    std::ofstream file(DATA_FILE);
    if (!file.is_open())
        throw std::runtime_error("파일을 열 수 없습니다: " + DATA_FILE);
    file << data.dump(4);
}

// ─────────────────────────────────────────
// 유틸리티
// ─────────────────────────────────────────
int nextId(const std::vector<Contact>& records) {
    int maxId = 0;
    for (const auto& c : records)
        maxId = std::max(maxId, c.id);
    return maxId + 1;
}

void clearInput() {
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

std::string getLine(const std::string& prompt) {
    std::cout << prompt;
    std::string s;
    std::getline(std::cin, s);
    return s;
}

void printDivider(int w = 72) {
    std::cout << std::string(w, '-') << "\n";
}

void printTableHeader() {
    printDivider();
    std::cout << std::left
              << std::setw(6)  << "ID"
              << std::setw(16) << "이름"
              << std::setw(28) << "이메일"
              << std::setw(14) << "전화번호"
              << "메모" << "\n";
    printDivider();
}

void printContact(const Contact& c) {
    std::cout << std::left
              << std::setw(6)  << c.id
              << std::setw(16) << c.name
              << std::setw(28) << c.email
              << std::setw(14) << c.phone
              << c.memo << "\n";
}

// ─────────────────────────────────────────
// Create
// ─────────────────────────────────────────
void createContact() {
    std::cout << "\n[CREATE] 새 연락처 추가\n";
    printDivider(40);

    clearInput();
    std::string name  = getLine("이름     : ");
    if (name.empty()) {
        std::cout << "[오류] 이름은 필수 항목입니다.\n";
        return;
    }
    std::string email = getLine("이메일   : ");
    std::string phone = getLine("전화번호 : ");
    std::string memo  = getLine("메모     : ");

    auto records = loadAll();
    Contact c{nextId(records), name, email, phone, memo};
    records.push_back(c);
    saveAll(records);

    std::cout << "[완료] ID " << c.id << " 연락처가 추가되었습니다.\n";
}

// ─────────────────────────────────────────
// Read (전체 목록)
// ─────────────────────────────────────────
void readAll() {
    auto records = loadAll();
    std::cout << "\n[READ] 전체 연락처 목록 (" << records.size() << "건)\n";

    if (records.empty()) {
        std::cout << "등록된 연락처가 없습니다.\n";
        return;
    }

    printTableHeader();
    for (const auto& c : records)
        printContact(c);
    printDivider();
}

// ─────────────────────────────────────────
// Search (ID / 이름)
// ─────────────────────────────────────────
void searchContact() {
    std::cout << "\n[SEARCH] 검색\n";
    std::cout << "1. ID로 검색\n";
    std::cout << "2. 이름으로 검색\n";
    std::cout << "선택: ";

    int choice;
    if (!(std::cin >> choice) || (choice != 1 && choice != 2)) {
        std::cout << "잘못된 선택입니다.\n";
        clearInput();
        return;
    }

    auto records = loadAll();
    std::vector<Contact> results;

    if (choice == 1) {
        int id;
        std::cout << "검색할 ID: ";
        if (!(std::cin >> id)) {
            std::cout << "올바른 ID를 입력하세요.\n";
            clearInput();
            return;
        }
        for (const auto& c : records)
            if (c.id == id) results.push_back(c);
    } else {
        clearInput();
        std::string keyword = getLine("검색할 이름: ");
        for (const auto& c : records)
            if (c.name.find(keyword) != std::string::npos)
                results.push_back(c);
    }

    if (results.empty()) {
        std::cout << "검색 결과가 없습니다.\n";
        return;
    }

    std::cout << "\n검색 결과 " << results.size() << "건:\n";
    printTableHeader();
    for (const auto& c : results)
        printContact(c);
    printDivider();
}

// ─────────────────────────────────────────
// Update
// ─────────────────────────────────────────
void updateContact() {
    std::cout << "\n[UPDATE] 연락처 수정\n";
    std::cout << "수정할 ID: ";

    int id;
    if (!(std::cin >> id)) {
        std::cout << "올바른 ID를 입력하세요.\n";
        clearInput();
        return;
    }

    auto records = loadAll();
    auto it = std::find_if(records.begin(), records.end(),
                           [id](const Contact& c) { return c.id == id; });
    if (it == records.end()) {
        std::cout << "ID " << id << "를 찾을 수 없습니다.\n";
        return;
    }

    std::cout << "\n현재 정보:\n";
    printTableHeader();
    printContact(*it);
    printDivider();

    std::cout << "\n수정할 필드:\n"
              << "1. 이름     (" << it->name  << ")\n"
              << "2. 이메일   (" << it->email << ")\n"
              << "3. 전화번호 (" << it->phone << ")\n"
              << "4. 메모     (" << it->memo  << ")\n"
              << "선택: ";

    int field;
    if (!(std::cin >> field) || field < 1 || field > 4) {
        std::cout << "잘못된 선택입니다.\n";
        clearInput();
        return;
    }

    clearInput();
    switch (field) {
        case 1: it->name  = getLine("새 이름     : "); break;
        case 2: it->email = getLine("새 이메일   : "); break;
        case 3: it->phone = getLine("새 전화번호 : "); break;
        case 4: it->memo  = getLine("새 메모     : "); break;
    }

    saveAll(records);
    std::cout << "[완료] ID " << id << " 연락처가 수정되었습니다.\n";
}

// ─────────────────────────────────────────
// Delete
// ─────────────────────────────────────────
void deleteContact() {
    std::cout << "\n[DELETE] 연락처 삭제\n";
    std::cout << "삭제할 ID: ";

    int id;
    if (!(std::cin >> id)) {
        std::cout << "올바른 ID를 입력하세요.\n";
        clearInput();
        return;
    }

    auto records = loadAll();
    auto it = std::find_if(records.begin(), records.end(),
                           [id](const Contact& c) { return c.id == id; });
    if (it == records.end()) {
        std::cout << "ID " << id << "를 찾을 수 없습니다.\n";
        return;
    }

    std::cout << "\n삭제할 연락처:\n";
    printTableHeader();
    printContact(*it);
    printDivider();

    std::cout << "정말 삭제하시겠습니까? (y/N): ";
    char confirm;
    std::cin >> confirm;

    if (confirm == 'y' || confirm == 'Y') {
        records.erase(it);
        saveAll(records);
        std::cout << "[완료] ID " << id << " 연락처가 삭제되었습니다.\n";
    } else {
        std::cout << "삭제가 취소되었습니다.\n";
    }
}

// ─────────────────────────────────────────
// 메인 메뉴
// ─────────────────────────────────────────
int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    std::cout << "데이터 저장 위치: " << DATA_FILE << "\n";

    while (true) {
        std::cout << "\n" << std::string(50, '=') << "\n";
        std::cout << "        JSON CRUD 연락처 관리 시스템\n";
        std::cout << std::string(50, '=') << "\n";
        std::cout << "  1. Create  - 연락처 추가\n";
        std::cout << "  2. Read    - 전체 목록 보기\n";
        std::cout << "  3. Search  - 검색 (ID / 이름)\n";
        std::cout << "  4. Update  - 연락처 수정\n";
        std::cout << "  5. Delete  - 연락처 삭제\n";
        std::cout << "  0. 종료\n";
        std::cout << std::string(50, '-') << "\n";
        std::cout << "선택: ";

        int choice;
        if (!(std::cin >> choice)) {
            std::cin.clear();
            clearInput();
            continue;
        }

        try {
            switch (choice) {
                case 1: createContact(); break;
                case 2: readAll();       break;
                case 3: searchContact(); break;
                case 4: updateContact(); break;
                case 5: deleteContact(); break;
                case 0:
                    std::cout << "프로그램을 종료합니다.\n";
                    return 0;
                default:
                    std::cout << "잘못된 선택입니다. 0~5 사이의 숫자를 입력하세요.\n";
            }
        } catch (const std::exception& e) {
            std::cerr << "[오류] " << e.what() << "\n";
        }
    }
}
