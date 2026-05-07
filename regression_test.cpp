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
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <map>
#include <random>
#include <algorithm>
#include <chrono>
#include <ctime>
#include <iomanip>
#include "json.hpp"

using namespace std;
using json = nlohmann::json;

static const string TEST_FILE    = "regression_contacts.json";
static const int    WARMUP       = 1000;
static const int    TOTAL_OPS    = 100000;
static const int    OPS_PER_TYPE = TOTAL_OPS / 4;  // 25,000개씩
static const int    CHKPT_EVERY  = 10000;

// ─── 데이터 모델 ──────────────────────────────────────────────────────
struct Contact {
    string id;          // YYMMXXXX (8자리)
    string name;
    string email;       // {id}@samsung.com
    string phone;
    string rank;        // CL1~CL4 / Master / Fellow / 상무~회장
    string department;
    string memo;
};

json cToJ(const Contact& c) {
    return {{"id",c.id},{"name",c.name},{"email",c.email},{"phone",c.phone},
            {"rank",c.rank},{"department",c.department},{"memo",c.memo}};
}
Contact jToC(const json& j) {
    string id;
    if (j.contains("id") && j["id"].is_string()) id = j["id"].get<string>();
    return {id,
            j.value("name",""), j.value("email",""), j.value("phone",""),
            j.value("rank",""), j.value("department",""), j.value("memo","")};
}

// ─── 고속 인메모리 DB (O(1) 모든 연산) ───────────────────────────────
struct MemDB {
    unordered_map<string, Contact> rec;
    unordered_map<string, int>     idx;  // id → alive 인덱스
    vector<string>                 alive;

    bool add(const Contact& c) {
        if (rec.count(c.id)) return false;
        rec[c.id] = c;
        idx[c.id] = (int)alive.size();
        alive.push_back(c.id);
        return true;
    }

    const Contact* read(const string& id) const {
        auto it = rec.find(id);
        return it != rec.end() ? &it->second : nullptr;
    }

    bool update(const string& id, int field, const string& val) {
        auto it = rec.find(id);
        if (it == rec.end()) return false;
        Contact& c = it->second;
        switch (field) {
            case 1: c.name       = val; break;
            case 2: c.email      = val; break;
            case 3: c.phone      = val; break;
            case 4: c.rank       = val; break;
            case 5: c.department = val; break;
            case 6: c.memo       = val; break;
        }
        return true;
    }

    bool del(const string& id) {
        auto ii = idx.find(id);
        if (ii == idx.end()) return false;
        int i = ii->second;
        string last = alive.back();
        alive[i] = last;
        idx[last] = i;
        alive.pop_back();
        idx.erase(id);
        rec.erase(id);
        return true;
    }

    string randAlive(mt19937& rng) const {
        if (alive.empty()) return "";
        return alive[uniform_int_distribution<int>(0,(int)alive.size()-1)(rng)];
    }

    bool   empty() const { return alive.empty(); }
    int    size()  const { return (int)alive.size(); }

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
        if (d.is_array()) for (const auto& item : d) v.push_back(jToC(item));
    } catch (...) {}
    return v;
}

// ─── 랜덤 데이터 생성기 ───────────────────────────────────────────────
struct Gen {
    mt19937          rng;
    unordered_set<string> usedIds;
    explicit Gen(unsigned s) : rng(s) {}

    int ri(int lo, int hi) {
        return uniform_int_distribution<int>(lo, hi)(rng);
    }

    // 사번 생성: YYMMXXXX (1990~2026년, 2026년은 01~05월까지)
    string genId() {
        while (true) {
            // 연도 인덱스 0-36: 0~9 → YY=90~99(1990-1999), 10~36 → YY=00~26(2000-2026)
            int idx = ri(0, 36);
            int yy  = (idx < 10) ? (90 + idx) : (idx - 10);
            int mm  = ri(1, (yy == 26) ? 5 : 12);
            int sfx = ri(1, 9999);
            ostringstream o;
            o << setw(2) << setfill('0') << yy
              << setw(2) << setfill('0') << mm
              << setw(4) << setfill('0') << sfx;
            string id = o.str();
            if (!usedIds.count(id)) { usedIds.insert(id); return id; }
        }
    }

    string genName(int i) {
        static const char* fn[] = {
            "김","이","박","최","정","강","조","윤","장","임",
            "한","오","서","신","권","황","안","송","류","전"
        };
        static const char* ln[] = {
            "민준","서연","도윤","서준","예린","하준","소연","지호","지수","현우",
            "영민","수연","정우","지원","태양","나은","승현","아린","도현","유진"
        };
        return string(fn[i % 20]) + ln[ri(0, 19)];
    }

    string genEmail(const string& id) {
        return id + "@samsung.com";
    }

    string genPhone(int i) {
        return "010-" + to_string(1000 + (i % 9000)) + "-" + to_string(1000 + ri(0, 8999));
    }

    // 직급: 피라미드 분포 (CL1 가장 많음 → 회장 가장 적음)
    string genRank() {
        struct RW { const char* name; int weight; };
        static const RW R[] = {
            {"CL1",400},{"CL2",250},{"CL3",150},{"CL4",80},
            {"Master",50},{"Fellow",30},{"상무",20},
            {"부사장",10},{"사장",5},{"부회장",3},{"회장",2}
        };
        int r = ri(1, 1000), acc = 0;
        for (int i = 0; i < 11; i++) {
            acc += R[i].weight;
            if (r <= acc) return R[i].name;
        }
        return "CL1";
    }

    // 부서: 반도체 회사 기준 다양한 부서
    string genDept() {
        static const char* D[] = {
            // 메모리 DS
            "DRAM 설계팀","NAND 설계팀","DRAM 공정개발팀","NAND 공정개발팀",
            "DRAM 제조팀","NAND 제조팀","메모리 품질팀","메모리 패키지팀",
            // 시스템LSI
            "AP 설계팀","모뎀 설계팀","CIS 설계팀","DDI 설계팀",
            "PMIC 설계팀","보안칩 설계팀","AI칩 개발팀","시스템LSI 검증팀",
            // 파운드리
            "파운드리 공정개발팀","EUV 공정팀","파운드리 제조팀","파운드리 고객지원팀",
            // 연구소
            "종합기술원","AI연구센터","반도체연구소","미래기술연구소","소자연구팀",
            // 지원
            "전략기획팀","인사팀","재무팀","법무팀","구매팀","IT인프라팀",
            // 패키지·소재·장비
            "패키지개발팀","기구설계팀","소재기술팀","장비기술팀","공정통합팀",
            // 해외법인
            "미국법인 설계팀","중국법인 제조팀","유럽법인 마케팅팀","인도법인 R&D팀"
        };
        int n = (int)(sizeof(D) / sizeof(D[0]));
        return D[ri(0, n-1)];
    }

    // 메모: 직원 개인 정보 랜덤
    string genMemo() {
        static const char* M[] = {
            "경력 입사자",
            "MSFT 경력 보유",
            "Google 경력 보유",
            "Intel 경력 보유",
            "Apple 경력 보유",
            "Qualcomm 경력 보유",
            "TSMC 경력 보유",
            "HW 엔지니어",
            "SW 엔지니어",
            "기구 개발자",
            "RF 엔지니어",
            "패키지 엔지니어",
            "공정 엔지니어",
            "박사 (KAIST)",
            "박사 (서울대)",
            "박사 (MIT)",
            "박사 (스탠퍼드)",
            "MBA 졸업",
            "특허 5건 보유",
            "특허 10건 이상",
            "SCI 논문 3편",
            "실리콘밸리 파견 1년",
            "삼성 공채 출신",
            "사내 우수 개발자 표창",
            "취미: 골프 (핸디캡 12)",
            "취미: 등산",
            "육아휴직 후 복귀",
            ""
        };
        int n = (int)(sizeof(M) / sizeof(M[0]));
        return M[ri(0, n-1)];
    }

    Contact genContact(int i) {
        string id = genId();
        return {id, genName(i), genEmail(id), genPhone(i),
                genRank(), genDept(), genMemo()};
    }
};

// ─── 필드 값 추출 (Update 검증용) ────────────────────────────────────
string getField(const Contact& c, int field) {
    switch (field) {
        case 1: return c.name;
        case 2: return c.email;
        case 3: return c.phone;
        case 4: return c.rank;
        case 5: return c.department;
        case 6: return c.memo;
    }
    return "";
}

// ─── 검증 집계 ────────────────────────────────────────────────────────
static long long gPass = 0, gFail = 0;
static vector<string> gFails;

void chk(const string& label, bool ok, const string& detail = "") {
    if (ok) { ++gPass; }
    else {
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
void wJson(const string& path, const json& d) { ofstream f(path); f << d.dump(4); }
void wSummary(const string& path, const map<string,long long>& stats) {
    ofstream f(path);
    f << "Regression 결과: PASS " << gPass << " / FAIL " << gFail << "\n\n";
    f << "=== 연산 통계 ===\n";
    for (auto it = stats.begin(); it != stats.end(); ++it)
        f << "  " << it->first << ": " << it->second << "\n";
    f << "\n";
    if (gFail == 0) { f << "전체 통과\n"; }
    else {
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

    map<string,long long> stats;
    stats["create_ok"]     = 0;
    stats["read_found"]    = 0;
    stats["update_ok"]     = 0;
    stats["delete_ok"]     = 0;
    stats["skipped_empty"] = 0;

    // ═══════════════════════════════════════════════════════
    // WARMUP : 초기 1,000개 Create
    // ═══════════════════════════════════════════════════════
    cout << "[WARMUP] 초기 " << WARMUP << "개 Create\n";
    for (int i = 0; i < WARMUP; i++) db.add(gen.genContact(i));

    // input.json : 워밍업 완료 초기 상태
    {
        json inp = json::array();
        for (const auto& c : db.sorted()) inp.push_back(cToJ(c));
        wJson(tcDir + TC_SEP + "input.json", inp);
    }

    // ═══════════════════════════════════════════════════════
    // 연산 시퀀스: 각 25,000개 → 셔플
    // ═══════════════════════════════════════════════════════
    enum OT { CR=0, RD=1, UP=2, DL=3 };
    vector<OT> ops;
    ops.reserve(TOTAL_OPS);
    for (int i = 0; i < OPS_PER_TYPE; i++) ops.push_back(CR);
    for (int i = 0; i < OPS_PER_TYPE; i++) ops.push_back(RD);
    for (int i = 0; i < OPS_PER_TYPE; i++) ops.push_back(UP);
    for (int i = 0; i < OPS_PER_TYPE; i++) ops.push_back(DL);
    shuffle(ops.begin(), ops.end(), gen.rng);

    cout << "[RUN] " << TOTAL_OPS << "개 랜덤 연산 "
         << "(CREATE/READ/UPDATE/DELETE 각 " << OPS_PER_TYPE << "개)\n";

    json opSample = json::array();  // 처음 100 + 마지막 100
    int  ci = WARMUP;               // create용 index
    int  ui = 0;                    // update 값 index
    int  chkptN = 0;

    for (int i = 0; i < TOTAL_OPS; i++) {
        OT   op    = ops[i];
        bool logIt = (i < 100 || i >= TOTAL_OPS - 100);
        json logE;
        if (logIt) logE["seq"] = i;

        // DB 비면 R/U/D → CR 강제 전환
        if (db.empty() && op != CR) {
            stats["skipped_empty"]++;
            op = CR;
            if (logIt) logE["note"] = "forced_create";
        }

        switch (op) {
        case CR: {
            Contact c = gen.genContact(ci++);
            bool ok = db.add(c);
            chk("Create ID=" + c.id, ok);
            if (ok) stats["create_ok"]++;
            if (logIt) {
                logE["type"] = "CREATE";
                logE["id"]   = c.id;
                logE["ok"]   = ok;
                logE["data"] = cToJ(c);
            }
            break;
        }
        case RD: {
            string tid = db.randAlive(gen.rng);
            const Contact* c = db.read(tid);
            bool found = (c != nullptr);
            chk("Read ID=" + tid + " 존재", found);
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
            string tid   = db.randAlive(gen.rng);
            int    field = gen.ri(1, 6);          // 6개 필드 랜덤
            // 필드 형식에 맞는 값 생성
            string nv;
            switch (field) {
                case 2:  nv = "upd" + to_string(ui) + "@samsung.com"; break;
                case 3:  nv = "010-" + to_string(1000 + (ui % 9000)) + "-" + to_string(1000 + ui % 8999); break;
                case 4:  { const char* R[]={"CL1","CL2","CL3","CL4","Master","Fellow","상무"}; nv=R[ui%7]; break; }
                default: nv = "upd_" + to_string(ui); break;
            }
            ui++;

            json before;
            const Contact* bc = db.read(tid);
            if (bc) before = cToJ(*bc);

            bool ok = db.update(tid, field, nv);
            chk("Update ID=" + tid, ok);

            bool verified = false;
            json after;
            if (ok) {
                const Contact* ac = db.read(tid);
                if (ac) {
                    after    = cToJ(*ac);
                    verified = (getField(*ac, field) == nv);
                    chk("Update 반영 ID=" + tid + " field=" + to_string(field),
                        verified, "기대=" + nv + " 실제=" + getField(*ac, field));
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
            string tid = db.randAlive(gen.rng);

            json before;
            const Contact* bc = db.read(tid);
            if (bc) before = cToJ(*bc);

            bool ok = db.del(tid);
            chk("Delete ID=" + tid, ok);
            if (ok) {
                bool gone = (db.read(tid) == nullptr);
                chk("Delete 후 부재 ID=" + tid, gone);
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

        // 체크포인트: 파일 저장 후 재로드 비교
        if ((i + 1) % CHKPT_EVERY == 0) {
            chkptN++;
            saveDB(db);
            auto ld  = loadDB();
            bool szOk = ((int)ld.size() == db.size());
            chk("체크포인트#" + to_string(chkptN) + " 크기 일치", szOk,
                to_string(ld.size()) + " vs " + to_string(db.size()));
            bool dmOk = true;
            if (szOk) {
                for (const auto& lc : ld) {
                    const Contact* mc = db.read(lc.id);
                    if (!mc || mc->name != lc.name || mc->email != lc.email ||
                        mc->phone != lc.phone || mc->rank != lc.rank ||
                        mc->department != lc.department || mc->memo != lc.memo) {
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
                mc->phone != lc.phone || mc->rank != lc.rank ||
                mc->department != lc.department || mc->memo != lc.memo) {
                fdmOk = false; break;
            }
        }
        chk("최종 데이터 일치 (in-memory vs file)", fdmOk);

        vector<string> fids;
        for (const auto& c : fl) fids.push_back(c.id);
        sort(fids.begin(), fids.end());
        chk("최종 ID 중복 없음",
            unique(fids.begin(), fids.end()) == fids.end());

        // 사번 형식 검증 (8자리 숫자, @samsung.com 이메일)
        bool idFmtOk = true, emailOk = true;
        for (const auto& c : fl) {
            if (c.id.size() != 8) { idFmtOk = false; break; }
            for (char ch : c.id) if (!isdigit((unsigned char)ch)) { idFmtOk = false; break; }
        }
        for (const auto& c : fl) {
            if (c.email.find("@samsung.com") == string::npos) { emailOk = false; break; }
        }
        chk("사번 8자리 숫자 형식 유효", idFmtOk);
        chk("이메일 @samsung.com 형식", emailOk);
    }

    // ═══════════════════════════════════════════════════════
    // TC 파일 저장
    // ═══════════════════════════════════════════════════════
    {
        json od = json::array();
        for (const auto& c : fl) od.push_back(cToJ(c));
        wJson(tcDir + TC_SEP + "output.json", od);
    }
    {
        json s;
        s["description"] = "처음 100 + 마지막 100 연산 샘플 (before/after 포함)";
        s["ops"] = opSample;
        wJson(tcDir + TC_SEP + "operations_sample.json", s);
    }
    stats["final_db_size"] = (long long)db.size();
    stats["total_pass"]    = gPass;
    stats["total_fail"]    = gFail;
    wSummary(tcDir + TC_SEP + "summary.txt", stats);

    // ─── 콘솔 결과 ─────────────────────────────────────────
    cout << "\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
    cout << "Regression 결과: PASS " << gPass << " / FAIL " << gFail << "\n";
    cout << "  CREATE : " << stats["create_ok"]  << " / " << OPS_PER_TYPE << "\n";
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
