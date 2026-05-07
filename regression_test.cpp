#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#define TC_MKDIR(p) _mkdir(p)
#define TC_SEP "\\"
#else
#include <sys/stat.h>
#define TC_MKDIR(p) mkdir(p, 0755)
#define TC_SEP "/"
#endif

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include <random>
#include <algorithm>
#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>
#include "json.hpp"

using namespace std;
using json = nlohmann::json;

static const string TEST_FILE   = "regression_contacts.json";
static const int    WARMUP      = 1000;   // 초기 Create (메인 카운트 제외)
static const int    TOTAL_OPS   = 100000; // 메인 랜덤 연산 총 수
static const int    OPS_PER_TYPE= TOTAL_OPS / 4; // 25,000개씩
static const int    CHKPT_EVERY = 10000;  // 체크포인트 간격

// ─── 데이터 모델 ──────────────────────────────────────────────────────
struct Contact {
    int id;
    string name, email, phone, memo;
};

json cToJ(const Contact& c) {
    return {{"id",c.id},{"name",c.name},{"email",c.email},
            {"phone",c.phone},{"memo",c.memo}};
}
Contact jToC(const json& j) {
    return {j.value("id",0), j.value("name",""), j.value("email",""),
            j.value("phone",""), j.value("memo","")};
}

// ─── 고속 인메모리 DB ─────────────────────────────────────────────────
// O(1) create / read / update / delete / random selection
struct MemDB {
    unordered_map<int, Contact> rec;
    unordered_map<int, int>     idx;   // id → alive 벡터 인덱스
    vector<int>                 alive; // 랜덤 선택용 ID 풀
    int maxId = 0;

    int create(const string& n, const string& e,
               const string& p, const string& m) {
        int id = ++maxId;
        rec[id] = {id, n, e, p, m};
        idx[id] = (int)alive.size();
        alive.push_back(id);
        return id;
    }

    const Contact* read(int id) const {
        auto it = rec.find(id);
        return it != rec.end() ? &it->second : nullptr;
    }

    bool update(int id, int field, const string& val) {
        auto it = rec.find(id);
        if (it == rec.end()) return false;
        Contact& c = it->second;
        switch (field) {
            case 1: c.name  = val; break;
            case 2: c.email = val; break;
            case 3: c.phone = val; break;
            case 4: c.memo  = val; break;
        }
        return true;
    }

    bool del(int id) {
        auto ii = idx.find(id);
        if (ii == idx.end()) return false;
        // O(1) swap-and-pop
        int i = ii->second;
        int last = alive.back();
        alive[i] = last;
        idx[last] = i;
        alive.pop_back();
        idx.erase(id);
        rec.erase(id);
        return true;
    }

    int randAlive(mt19937& rng) const {
        if (alive.empty()) return -1;
        return alive[uniform_int_distribution<int>(0, (int)alive.size()-1)(rng)];
    }

    bool empty() const { return alive.empty(); }
    int  size()  const { return (int)alive.size(); }

    vector<Contact> sorted() const {
        vector<Contact> v;
        v.reserve(rec.size());
        for (auto it = rec.begin(); it != rec.end(); ++it)
            v.push_back(it->second);
        sort(v.begin(), v.end(),
             [](const Contact& a, const Contact& b){ return a.id < b.id; });
        return v;
    }
};

// ─── 파일 I/O ─────────────────────────────────────────────────────────
void saveDB(const MemDB& db) {
    json data = json::array();
    for (const auto& c : db.sorted()) data.push_back(cToJ(c));
    ofstream f(TEST_FILE);
    f << data.dump(4);
}

vector<Contact> loadDB() {
    vector<Contact> v;
    ifstream f(TEST_FILE);
    if (!f.is_open()) return v;
    try {
        json d; f >> d;
        if (d.is_array())
            for (const auto& item : d) v.push_back(jToC(item));
    } catch (...) {}
    return v;
}

// ─── 랜덤 데이터 생성기 ───────────────────────────────────────────────
struct Gen {
    mt19937 rng;
    explicit Gen(unsigned s) : rng(s) {}
    int ri(int lo, int hi) {
        return uniform_int_distribution<int>(lo, hi)(rng);
    }
    string name(int i) {
        static const char* fn[] = {"김","이","박","최","정","강","조","윤","장","임"};
        static const char* ln[] = {"민준","서연","도윤","서준","예린",
                                    "하준","소연","지호","지수","현우"};
        return string(fn[i % 10]) + ln[ri(0, 9)];
    }
    string email(int i) {
        static const char* dom[] = {"test.com","example.com","mail.kr","dev.io"};
        return "u" + to_string(i) + "@" + dom[ri(0, 3)];
    }
    string phone(int i) {
        return "010-" + to_string(1000 + (i % 9000)) + "-" + to_string(1000 + ri(0, 8999));
    }
    string memo() {
        static const char* ms[] = {"개발팀","디자인팀","QA팀","기획팀","운영팀",""};
        return ms[ri(0, 5)];
    }
};

// ─── 검증 ─────────────────────────────────────────────────────────────
static long long gPass = 0, gFail = 0;
static vector<string> gFails;

void chk(const string& label, bool ok, const string& detail = "") {
    if (ok) {
        ++gPass;
    } else {
        ++gFail;
        if ((int)gFails.size() < 200)
            gFails.push_back("[FAIL] " + label +
                             (detail.empty() ? "" : " (" + detail + ")"));
    }
}

// ─── TC 유틸 ──────────────────────────────────────────────────────────
string makeTS() {
    auto now = chrono::system_clock::now();
    time_t t = chrono::system_clock::to_time_t(now);
    tm tb = *localtime(&t);
    ostringstream o;
    o << put_time(&tb, "%Y%m%d_%H%M%S");
    return o.str();
}

string makeTCDir(const string& ts) {
    string base = "TC", dir = base + TC_SEP + ts;
    TC_MKDIR(base.c_str());
    TC_MKDIR(dir.c_str());
    return dir;
}

void wJson(const string& path, const json& d) {
    ofstream f(path);
    f << d.dump(4);
}

void wSummary(const string& path, const map<string, long long>& stats) {
    ofstream f(path);
    f << "Regression 결과: PASS " << gPass << " / FAIL " << gFail << "\n\n";
    f << "=== 연산 통계 ===\n";
    for (auto it = stats.begin(); it != stats.end(); ++it)
        f << "  " << it->first << ": " << it->second << "\n";
    f << "\n";
    if (gFail == 0) {
        f << "전체 통과\n";
    } else {
        f << "실패 항목 (최대 200건):\n";
        for (const auto& s : gFails) f << "  " << s << "\n";
    }
}

// ─────────────────────────────────────────────────────────────────────
int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    remove(TEST_FILE.c_str());

    string ts    = makeTS();
    string tcDir = makeTCDir(ts);

    Gen gen(12345);
    MemDB db;

    map<string, long long> stats;
    stats["create_ok"]     = 0;
    stats["read_found"]    = 0;
    stats["update_ok"]     = 0;
    stats["delete_ok"]     = 0;
    stats["skipped_empty"] = 0;

    // ═══════════════════════════════════════════════════════
    // WARMUP : 초기 1000개 Create (메인 카운트 제외)
    // ═══════════════════════════════════════════════════════
    cout << "[WARMUP] 초기 " << WARMUP << "개 Create\n";
    for (int i = 0; i < WARMUP; i++)
        db.create(gen.name(i), gen.email(i), gen.phone(i), gen.memo());

    // input.json : 워밍업 완료 후 초기 상태 저장
    {
        json inp = json::array();
        for (const auto& c : db.sorted()) inp.push_back(cToJ(c));
        wJson(tcDir + TC_SEP + "input.json", inp);
    }

    // ═══════════════════════════════════════════════════════
    // 연산 시퀀스 생성: CREATE/READ/UPDATE/DELETE 각 25,000개 → 셔플
    // ═══════════════════════════════════════════════════════
    enum OT { CR = 0, RD = 1, UP = 2, DL = 3 };
    vector<OT> ops;
    ops.reserve(TOTAL_OPS);
    for (int i = 0; i < OPS_PER_TYPE; i++) ops.push_back(CR);
    for (int i = 0; i < OPS_PER_TYPE; i++) ops.push_back(RD);
    for (int i = 0; i < OPS_PER_TYPE; i++) ops.push_back(UP);
    for (int i = 0; i < OPS_PER_TYPE; i++) ops.push_back(DL);
    shuffle(ops.begin(), ops.end(), gen.rng);

    cout << "[RUN] " << TOTAL_OPS << "개 랜덤 연산 "
         << "(CREATE/READ/UPDATE/DELETE 각 " << OPS_PER_TYPE << "개)\n";

    // 샘플 연산 로그 (처음 100 + 마지막 100개)
    json opSample = json::array();

    int ci = WARMUP; // create용 index (warmup 이후부터)
    int ui = 0;      // update 값 index
    int chkptN = 0;

    for (int i = 0; i < TOTAL_OPS; i++) {
        OT op = ops[i];
        bool logIt = (i < 100 || i >= TOTAL_OPS - 100);
        json logE;
        if (logIt) logE["seq"] = i;

        // DB가 비면 R/U/D → CR 강제 전환
        if (db.empty() && op != CR) {
            stats["skipped_empty"]++;
            op = CR;
            if (logIt) logE["note"] = "forced_create";
        }

        switch (op) {
        case CR: {
            int id = db.create(gen.name(ci), gen.email(ci),
                               gen.phone(ci), gen.memo());
            ci++;
            bool ok = (db.read(id) != nullptr);
            chk("Create ID=" + to_string(id), ok);
            if (ok) stats["create_ok"]++;
            if (logIt) {
                logE["type"] = "CREATE";
                logE["id"]   = id;
                logE["ok"]   = ok;
                const Contact* c = db.read(id);
                if (c) logE["data"] = cToJ(*c);
            }
            break;
        }
        case RD: {
            int tid = db.randAlive(gen.rng);
            const Contact* c = db.read(tid);
            bool found = (c != nullptr);
            chk("Read ID=" + to_string(tid) + " 존재", found);
            if (found) stats["read_found"]++;
            if (logIt) {
                logE["type"]  = "READ";
                logE["id"]    = tid;
                logE["found"] = found;
                if (c) logE["data"] = cToJ(*c);
            }
            break;
        }
        case UP: {
            int tid   = db.randAlive(gen.rng);
            int field = gen.ri(1, 4);
            string nv = "upd_" + to_string(ui++);

            json before;
            const Contact* bc = db.read(tid);
            if (bc) before = cToJ(*bc);

            bool ok = db.update(tid, field, nv);
            chk("Update ID=" + to_string(tid), ok);

            bool verified = false;
            json after;
            if (ok) {
                const Contact* ac = db.read(tid);
                if (ac) {
                    after = cToJ(*ac);
                    string actual;
                    switch (field) {
                        case 1: actual = ac->name;  break;
                        case 2: actual = ac->email; break;
                        case 3: actual = ac->phone; break;
                        case 4: actual = ac->memo;  break;
                    }
                    verified = (actual == nv);
                    chk("Update 반영 ID=" + to_string(tid), verified,
                        "기대=" + nv + " 실제=" + actual);
                    if (verified) stats["update_ok"]++;
                }
            }
            if (logIt) {
                logE["type"]   = "UPDATE";
                logE["id"]     = tid;
                logE["field"]  = field;
                logE["val"]    = nv;
                logE["ok"]     = ok;
                logE["before"] = before;
                logE["after"]  = after;
            }
            break;
        }
        case DL: {
            int tid = db.randAlive(gen.rng);

            json before;
            const Contact* bc = db.read(tid);
            if (bc) before = cToJ(*bc);

            bool ok = db.del(tid);
            chk("Delete ID=" + to_string(tid), ok);
            if (ok) {
                bool gone = (db.read(tid) == nullptr);
                chk("Delete 후 부재 ID=" + to_string(tid), gone);
                if (gone) stats["delete_ok"]++;
            }
            if (logIt) {
                logE["type"]   = "DELETE";
                logE["id"]     = tid;
                logE["ok"]     = ok;
                logE["before"] = before;
            }
            break;
        }
        }

        if (logIt) opSample.push_back(logE);

        // ─── 체크포인트: 파일 저장 후 재로드 비교 ──────────────
        if ((i + 1) % CHKPT_EVERY == 0) {
            chkptN++;
            saveDB(db);
            auto ld = loadDB();
            bool szOk = ((int)ld.size() == db.size());
            chk("체크포인트#" + to_string(chkptN) + " 크기 일치", szOk,
                to_string(ld.size()) + " vs " + to_string(db.size()));
            bool dmOk = true;
            if (szOk) {
                for (const auto& lc : ld) {
                    const Contact* mc = db.read(lc.id);
                    if (!mc || mc->name != lc.name || mc->email != lc.email ||
                        mc->phone != lc.phone || mc->memo != lc.memo) {
                        dmOk = false; break;
                    }
                }
                chk("체크포인트#" + to_string(chkptN) + " 데이터 일치", dmOk);
            }
            cout << "  체크포인트 " << chkptN
                 << " (" << (i+1) << "/" << TOTAL_OPS
                 << ") DB=" << db.size() << "건 "
                 << (szOk && dmOk ? "✓" : "✗") << "\n";
        }
    }

    // ═══════════════════════════════════════════════════════
    // 최종 무결성 검증
    // ═══════════════════════════════════════════════════════
    cout << "최종 무결성 검증 (DB=" << db.size() << "건)...\n";
    saveDB(db);
    auto fl = loadDB();

    bool fszOk = ((int)fl.size() == db.size());
    chk("최종 레코드 수 일치", fszOk,
        to_string(fl.size()) + " vs " + to_string(db.size()));

    if (fszOk) {
        bool fdmOk = true;
        for (const auto& lc : fl) {
            const Contact* mc = db.read(lc.id);
            if (!mc || mc->name != lc.name || mc->email != lc.email ||
                mc->phone != lc.phone || mc->memo != lc.memo) {
                fdmOk = false; break;
            }
        }
        chk("최종 데이터 일치 (in-memory vs file)", fdmOk);

        vector<int> fids;
        for (const auto& c : fl) fids.push_back(c.id);
        sort(fids.begin(), fids.end());
        chk("최종 ID 중복 없음",
            unique(fids.begin(), fids.end()) == fids.end());
    }

    // ═══════════════════════════════════════════════════════
    // TC 파일 저장
    // ═══════════════════════════════════════════════════════
    // output.json : 최종 DB 상태 전체
    {
        json od = json::array();
        for (const auto& c : fl) od.push_back(cToJ(c));
        wJson(tcDir + TC_SEP + "output.json", od);
    }
    // operations_sample.json : 처음 100 + 마지막 100 연산
    {
        json s;
        s["description"] = "처음 100 + 마지막 100 연산 샘플 (before/after 포함)";
        s["ops"] = opSample;
        wJson(tcDir + TC_SEP + "operations_sample.json", s);
    }
    // summary.txt
    stats["final_db_size"] = (long long)db.size();
    stats["total_pass"]    = gPass;
    stats["total_fail"]    = gFail;
    wSummary(tcDir + TC_SEP + "summary.txt", stats);

    // ─── 콘솔 결과 ─────────────────────────────────────────
    cout << "\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
    cout << "Regression 결과: PASS " << gPass << " / FAIL " << gFail << "\n";
    cout << "  CREATE : " << stats["create_ok"] << " / " << OPS_PER_TYPE << "\n";
    cout << "  READ   : " << stats["read_found"] << " / " << OPS_PER_TYPE << "\n";
    cout << "  UPDATE : " << stats["update_ok"]  << " / " << OPS_PER_TYPE << "\n";
    cout << "  DELETE : " << stats["delete_ok"]  << " / " << OPS_PER_TYPE << "\n";
    cout << "  DB 최종 : " << db.size() << "건\n";
    for (const auto& s : gFails) cout << s << "\n";
    cout << (gFail == 0 ? "전체 통과\n" : "실패 항목 있음\n");
    cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
    cout << "TC 출력 경로: " << tcDir << "\n";

    remove(TEST_FILE.c_str());
    return gFail > 0 ? 1 : 0;
}
