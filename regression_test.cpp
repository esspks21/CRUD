#ifdef _WIN32
#include <windows.h>
#endif

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <random>
#include <algorithm>
#include <numeric>
#include "json.hpp"

using namespace std;
using json = nlohmann::json;

static const string TEST_FILE = "regression_contacts.json";

// ─── 데이터 모델 (main.cpp 와 동일) ──────────────────────────────────
struct Contact {
    int    id;
    string name;
    string email;
    string phone;
    string memo;
};

json toJson(const Contact& c) {
    return {{"id", c.id}, {"name", c.name}, {"email", c.email},
            {"phone", c.phone}, {"memo", c.memo}};
}

Contact fromJson(const json& j) {
    return {j.value("id", 0), j.value("name", ""),
            j.value("email", ""), j.value("phone", ""), j.value("memo", "")};
}

// ─── 파일 I/O ─────────────────────────────────────────────────────────
vector<Contact> loadAll() {
    vector<Contact> records;
    ifstream f(TEST_FILE);
    if (!f.is_open()) return records;
    try {
        json data; f >> data;
        if (data.is_array())
            for (const auto& item : data)
                records.push_back(fromJson(item));
    } catch (...) {}
    return records;
}

void saveAll(const vector<Contact>& records) {
    json data = json::array();
    for (const auto& c : records)
        data.push_back(toJson(c));
    ofstream f(TEST_FILE);
    f << data.dump(4);
}

// ─── CRUD 연산 (main.cpp 로직과 동일하게 구현) ───────────────────────
int nextId(const vector<Contact>& records) {
    int maxId = 0;
    for (const auto& c : records)
        maxId = max(maxId, c.id);
    return maxId + 1;
}

int opCreate(const string& name, const string& email,
             const string& phone, const string& memo) {
    auto records = loadAll();
    Contact c{nextId(records), name, email, phone, memo};
    records.push_back(c);
    saveAll(records);
    return c.id;
}

bool opUpdate(int id, int field, const string& value) {
    auto records = loadAll();
    for (auto& c : records) {
        if (c.id != id) continue;
        switch (field) {
            case 1: c.name  = value; break;
            case 2: c.email = value; break;
            case 3: c.phone = value; break;
            case 4: c.memo  = value; break;
        }
        saveAll(records);
        return true;
    }
    return false;
}

bool opDelete(int id) {
    auto records = loadAll();
    auto it = find_if(records.begin(), records.end(),
                      [id](const Contact& c) { return c.id == id; });
    if (it == records.end()) return false;
    records.erase(it);
    saveAll(records);
    return true;
}

// ─── 랜덤 데이터 생성기 ───────────────────────────────────────────────
struct Gen {
    mt19937 rng;
    explicit Gen(unsigned seed) : rng(seed) {}

    int randInt(int lo, int hi) {
        return uniform_int_distribution<int>(lo, hi)(rng);
    }

    string name(int i) {
        static const char* fn[] = {"김","이","박","최","정","강","조","윤","장","임"};
        static const char* ln[] = {"민준","서연","도윤","서준","예린",
                                    "하준","소연","지호","지수","현우"};
        return string(fn[i % 10]) + ln[randInt(0, 9)];
    }
    string email(int i) {
        static const char* dom[] = {"test.com","example.com","mail.kr","dev.io"};
        return "user" + to_string(i) + "@" + dom[randInt(0, 3)];
    }
    string phone(int i) {
        return "010-" + to_string(1000 + i) + "-" + to_string(1000 + randInt(0, 8999));
    }
    string memo() {
        static const char* ms[] = {"개발팀","디자인팀","QA팀","기획팀","운영팀",""};
        return ms[randInt(0, 5)];
    }
};

// ─── 테스트 결과 집계 ─────────────────────────────────────────────────
static int gPass = 0, gFail = 0;
static vector<string> gFailures;

void check(const string& label, bool cond, const string& detail = "") {
    if (cond) {
        ++gPass;
    } else {
        ++gFail;
        gFailures.push_back("  [FAIL] " + label +
                            (detail.empty() ? "" : "  (" + detail + ")"));
    }
}

// ─────────────────────────────────────────────────────────────────────
int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    remove(TEST_FILE.c_str());

    Gen gen(12345);

    // ═══════════════════════════════════════════════════════
    // STEP 1 : 100개 Create
    // ═══════════════════════════════════════════════════════
    cout << "[STEP 1] 100개 연락처 Create\n";
    vector<int> alive;
    for (int i = 0; i < 100; i++) {
        int id = opCreate(gen.name(i), gen.email(i), gen.phone(i), gen.memo());
        alive.push_back(id);
    }

    {
        auto recs = loadAll();
        check("레코드 수 = 100",
              recs.size() == 100, "실제=" + to_string(recs.size()));

        vector<int> ids;
        for (const auto& c : recs) ids.push_back(c.id);
        sort(ids.begin(), ids.end());
        check("ID 중복 없음",
              unique(ids.begin(), ids.end()) == ids.end());
        check("ID 시퀀스 1~100",
              ids.front() == 1 && ids.back() == 100);
    }

    // ═══════════════════════════════════════════════════════
    // STEP 2 : 랜덤 Read 30회
    // ═══════════════════════════════════════════════════════
    cout << "[STEP 2] 랜덤 Read 검증 (30회)\n";
    for (int i = 0; i < 30; i++) {
        int tid = alive[gen.randInt(0, (int)alive.size() - 1)];
        auto recs = loadAll();
        bool found = any_of(recs.begin(), recs.end(),
                            [tid](const Contact& c){ return c.id == tid; });
        check("Read ID=" + to_string(tid) + " 존재", found);
    }

    // ═══════════════════════════════════════════════════════
    // STEP 3 : 랜덤 Update 20회 + 반영 확인
    // ═══════════════════════════════════════════════════════
    cout << "[STEP 3] 랜덤 Update (20회)\n";
    for (int i = 0; i < 20; i++) {
        int tid   = alive[gen.randInt(0, (int)alive.size() - 1)];
        int field = gen.randInt(1, 4);
        string nv = "upd_" + to_string(i);

        bool ok = opUpdate(tid, field, nv);
        check("Update ID=" + to_string(tid) + " field=" + to_string(field), ok);

        if (ok) {
            auto recs = loadAll();
            for (const auto& c : recs) {
                if (c.id != tid) continue;
                string actual;
                switch (field) {
                    case 1: actual = c.name;  break;
                    case 2: actual = c.email; break;
                    case 3: actual = c.phone; break;
                    case 4: actual = c.memo;  break;
                }
                check("Update 반영 ID=" + to_string(tid) + " field=" + to_string(field),
                      actual == nv, "기대=" + nv + " 실제=" + actual);
                break;
            }
        }
    }

    // ═══════════════════════════════════════════════════════
    // STEP 4 : 랜덤 Delete 25회 + 부재 확인
    // ═══════════════════════════════════════════════════════
    cout << "[STEP 4] 랜덤 Delete (25회)\n";
    shuffle(alive.begin(), alive.end(), gen.rng);
    vector<int> toDelete(alive.begin(), alive.begin() + 25);
    vector<int> remaining(alive.begin() + 25, alive.end());

    for (int tid : toDelete) {
        bool ok = opDelete(tid);
        check("Delete ID=" + to_string(tid), ok);

        if (ok) {
            auto recs = loadAll();
            bool gone = !any_of(recs.begin(), recs.end(),
                                [tid](const Contact& c){ return c.id == tid; });
            check("Delete 후 ID=" + to_string(tid) + " 부재", gone);
        }
    }

    // ═══════════════════════════════════════════════════════
    // STEP 5 : Delete 후 Create → max+1 전략
    // ═══════════════════════════════════════════════════════
    cout << "[STEP 5] Delete 후 Create - max+1 ID 전략\n";
    {
        auto now = loadAll();
        int maxIdNow = 0;
        for (const auto& c : now) maxIdNow = max(maxIdNow, c.id);

        int newId = opCreate("신규연락처", "new@test.com", "010-9999-0000", "신규");
        check("새 ID = max+1 (" + to_string(maxIdNow + 1) + ")",
              newId == maxIdNow + 1, "실제=" + to_string(newId));
        remaining.push_back(newId);
    }

    // ═══════════════════════════════════════════════════════
    // STEP 6 : 최종 무결성
    // ═══════════════════════════════════════════════════════
    cout << "[STEP 6] 최종 무결성 검증\n";
    {
        auto final = loadAll();
        int expected = 100 - 25 + 1; // 76

        check("최종 레코드 수 = " + to_string(expected),
              (int)final.size() == expected, "실제=" + to_string(final.size()));

        bool allHaveName = all_of(final.begin(), final.end(),
                                  [](const Contact& c){ return !c.name.empty(); });
        check("모든 레코드 name 필드 비어있지 않음", allHaveName);

        vector<int> fids;
        for (const auto& c : final) fids.push_back(c.id);
        sort(fids.begin(), fids.end());
        check("최종 ID 중복 없음",
              unique(fids.begin(), fids.end()) == fids.end());

        bool deletedGone = true;
        for (int did : toDelete)
            if (any_of(final.begin(), final.end(),
                       [did](const Contact& c){ return c.id == did; }))
                { deletedGone = false; break; }
        check("삭제된 25개 ID 최종 부재 확인", deletedGone);
    }

    // ─── 결과 출력 ─────────────────────────────────────────
    cout << "\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
    cout << "Regression 결과: PASS " << gPass << " / FAIL " << gFail << "\n";
    for (const auto& f : gFailures) cout << f << "\n";
    cout << (gFail == 0 ? "전체 통과\n" : "실패 항목 있음\n");
    cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";

    remove(TEST_FILE.c_str());
    return gFail > 0 ? 1 : 0;
}
