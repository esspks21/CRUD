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
static const int    OPS_PER_TYPE = TOTAL_OPS / 4;
static const int    CHKPT_EVERY  = 10000;

// ─── 이름 테이블 ──────────────────────────────────────────────────────
static const char* SURNAMES_KR[] = {
    "김","이","박","최","정","강","조","윤","장","임",
    "한","오","서","신","권","황","안","송","류","전"
};
static const char* SURNAMES_EN[] = {
    "kim","lee","park","choi","jung","kang","cho","yoon","jang","lim",
    "han","oh","seo","shin","kwon","hwang","ahn","song","ryu","jeon"
};
static const char* GIVEN_KR[] = {
    "민준","서연","도윤","서준","예린","하준","소연","지호","지수","현우",
    "영민","수연","정우","지원","태양","나은","승현","아린","도현","유진"
};
// 주어진 이름의 로마자 이니셜 (영문 소문자 2자)
static const char* GIVEN_INIT[] = {
    "mj","sy","dy","sj","yr","hj","so","jh","js","hu",
    "ym","su","jw","ji","ty","ne","sh","ar","dh","yj"
};

// ─── 데이터 모델 ──────────────────────────────────────────────────────
// id     : {이니셜}{숫자}.{성영문}  예) mj4156.kim
// emp_no : YYMMXXXX (8자리 사번)   예) 90011234
struct Contact {
    string id;
    string emp_no;
    string name;
    string email;
    string phone;
    string rank;
    string department;
    string memo;
};

json cToJ(const Contact& c) {
    return {{"id",c.id},{"emp_no",c.emp_no},{"name",c.name},{"email",c.email},
            {"phone",c.phone},{"rank",c.rank},{"department",c.department},{"memo",c.memo}};
}
Contact jToC(const json& j) {
    return {j.value("id",""), j.value("emp_no",""), j.value("name",""),
            j.value("email",""), j.value("phone",""), j.value("rank",""),
            j.value("department",""), j.value("memo","")};
}

// ─── 고속 인메모리 DB ─────────────────────────────────────────────────
struct MemDB {
    unordered_map<string,Contact> rec;
    unordered_map<string,int>     idx;
    vector<string>                alive;

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
        alive[i] = last; idx[last] = i;
        alive.pop_back(); idx.erase(id); rec.erase(id);
        return true;
    }
    string randAlive(mt19937& rng) const {
        if (alive.empty()) return "";
        return alive[uniform_int_distribution<int>(0,(int)alive.size()-1)(rng)];
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
    ofstream f(TEST_FILE); f << data.dump(4);
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
    mt19937            rng;
    unordered_set<string> usedEmpNos;  // YYMMXXXX 중복 방지
    unordered_set<string> usedIds;     // {init}{num}.{surname} 중복 방지
    explicit Gen(unsigned s) : rng(s) {}

    int ri(int lo, int hi) { return uniform_int_distribution<int>(lo,hi)(rng); }

    // 사번 생성: YYMMXXXX (1990~2026-05)
    string genEmpNo() {
        while (true) {
            int idx = ri(0, 36);
            int yy  = (idx < 10) ? (90 + idx) : (idx - 10);
            int mm  = ri(1, (yy == 26) ? 5 : 12);
            int sfx = ri(1, 9999);
            ostringstream o;
            o << setw(2) << setfill('0') << yy
              << setw(2) << setfill('0') << mm
              << setw(4) << setfill('0') << sfx;
            string en = o.str();
            if (!usedEmpNos.count(en)) { usedEmpNos.insert(en); return en; }
        }
    }

    // ID 생성: {이니셜}{1000-9999}.{성영문}  예) mj4156.kim
    string genUserId(const string& init, const string& surnameEn) {
        while (true) {
            int num = ri(1000, 9999);
            ostringstream o;
            o << init << num << "." << surnameEn;
            string id = o.str();
            if (!usedIds.count(id)) { usedIds.insert(id); return id; }
        }
    }

    string genPhone(int i) {
        return "010-" + to_string(1000+(i%9000)) + "-" + to_string(1000+ri(0,8999));
    }

    // 직급: 피라미드 분포
    string genRank() {
        struct RW { const char* name; int w; };
        static const RW R[] = {
            {"CL1",400},{"CL2",250},{"CL3",150},{"CL4",80},
            {"Master",50},{"Fellow",30},{"상무",20},
            {"부사장",10},{"사장",5},{"부회장",3},{"회장",2}
        };
        int r = ri(1,1000), acc = 0;
        for (int i = 0; i < 11; i++) {
            acc += R[i].w;
            if (r <= acc) return R[i].name;
        }
        return "CL1";
    }

    // 부서: 반도체 회사 40개+
    string genDept() {
        static const char* D[] = {
            "DRAM 설계팀","NAND 설계팀","DRAM 공정개발팀","NAND 공정개발팀",
            "DRAM 제조팀","NAND 제조팀","메모리 품질팀","메모리 패키지팀",
            "AP 설계팀","모뎀 설계팀","CIS 설계팀","DDI 설계팀",
            "PMIC 설계팀","보안칩 설계팀","AI칩 개발팀","시스템LSI 검증팀",
            "파운드리 공정개발팀","EUV 공정팀","파운드리 제조팀","파운드리 고객지원팀",
            "종합기술원","AI연구센터","반도체연구소","미래기술연구소","소자연구팀",
            "전략기획팀","인사팀","재무팀","법무팀","구매팀","IT인프라팀",
            "패키지개발팀","기구설계팀","소재기술팀","장비기술팀","공정통합팀",
            "미국법인 설계팀","중국법인 제조팀","유럽법인 마케팅팀","인도법인 R&D팀"
        };
        return D[ri(0, (int)(sizeof(D)/sizeof(D[0]))-1)];
    }

    // 메모: 개인 정보 28종
    string genMemo() {
        static const char* M[] = {
            "경력 입사자","MSFT 경력 보유","Google 경력 보유","Intel 경력 보유",
            "Apple 경력 보유","Qualcomm 경력 보유","TSMC 경력 보유",
            "HW 엔지니어","SW 엔지니어","기구 개발자","RF 엔지니어",
            "패키지 엔지니어","공정 엔지니어",
            "박사 (KAIST)","박사 (서울대)","박사 (MIT)","박사 (스탠퍼드)",
            "MBA 졸업","특허 5건 보유","특허 10건 이상","SCI 논문 3편",
            "실리콘밸리 파견 1년","삼성 공채 출신","사내 우수 개발자 표창",
            "취미: 골프 (핸디캡 12)","취미: 등산","육아휴직 후 복귀",""
        };
        return M[ri(0, (int)(sizeof(M)/sizeof(M[0]))-1)];
    }

    // 완전한 Contact 생성
    Contact genContact(int i) {
        int   si      = i % 20;
        int   gi      = ri(0, 19);
        string empNo  = genEmpNo();
        string init   = GIVEN_INIT[gi];
        string surEn  = SURNAMES_EN[si];
        string userId = genUserId(init, surEn);
        string name   = string(SURNAMES_KR[si]) + GIVEN_KR[gi];
        string email  = userId + "@samsung.com";
        return {userId, empNo, name, email, genPhone(i),
                genRank(), genDept(), genMemo()};
    }
};

// 필드 값 추출 (Update 검증용)
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
    ostringstream o; o << put_time(&tb, "%Y%m%d_%H%M%S");
    return o.str();
}
string makeTCDir(const string& ts) {
    string base="TC", dir=base+TC_SEP+ts;
    TC_MKDIR(base.c_str()); TC_MKDIR(dir.c_str());
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
    SetConsoleOutputCP(CP_UTF8); SetConsoleCP(CP_UTF8);
#endif
    remove(TEST_FILE.c_str());

    string ts = makeTS(), tcDir = makeTCDir(ts);
    Gen gen(12345);
    MemDB db;

    map<string,long long> stats;
    stats["create_ok"]=0; stats["read_found"]=0;
    stats["update_ok"]=0; stats["delete_ok"]=0;
    stats["skipped_empty"]=0;

    // ══════════════════════════════════════════════════════
    // WARMUP : 초기 1,000개 Create
    // ══════════════════════════════════════════════════════
    cout << "[WARMUP] 초기 " << WARMUP << "개 Create\n";
    for (int i = 0; i < WARMUP; i++) db.add(gen.genContact(i));

    // input.json
    {
        json inp = json::array();
        for (const auto& c : db.sorted()) inp.push_back(cToJ(c));
        wJson(tcDir + TC_SEP + "input.json", inp);
    }

    // ══════════════════════════════════════════════════════
    // 연산 시퀀스: 각 25,000개 → 셔플
    // ══════════════════════════════════════════════════════
    enum OT { CR=0, RD=1, UP=2, DL=3 };
    vector<OT> ops; ops.reserve(TOTAL_OPS);
    for (int i=0;i<OPS_PER_TYPE;i++) ops.push_back(CR);
    for (int i=0;i<OPS_PER_TYPE;i++) ops.push_back(RD);
    for (int i=0;i<OPS_PER_TYPE;i++) ops.push_back(UP);
    for (int i=0;i<OPS_PER_TYPE;i++) ops.push_back(DL);
    shuffle(ops.begin(), ops.end(), gen.rng);

    cout << "[RUN] " << TOTAL_OPS << "개 랜덤 연산 "
         << "(CREATE/READ/UPDATE/DELETE 각 " << OPS_PER_TYPE << "개)\n";

    json opSample = json::array();
    int ci = WARMUP, ui = 0, chkptN = 0;

    for (int i = 0; i < TOTAL_OPS; i++) {
        OT   op    = ops[i];
        bool logIt = (i < 100 || i >= TOTAL_OPS - 100);
        json logE; if (logIt) logE["seq"] = i;

        if (db.empty() && op != CR) {
            stats["skipped_empty"]++; op = CR;
            if (logIt) logE["note"] = "forced_create";
        }

        switch (op) {
        case CR: {
            Contact c = gen.genContact(ci++);
            bool ok = db.add(c);
            chk("Create id=" + c.id, ok);
            if (ok) stats["create_ok"]++;
            if (logIt) { logE["type"]="CREATE"; logE["id"]=c.id;
                         logE["emp_no"]=c.emp_no; logE["ok"]=ok; logE["data"]=cToJ(c); }
            break;
        }
        case RD: {
            string tid = db.randAlive(gen.rng);
            const Contact* c = db.read(tid);
            bool found = (c != nullptr);
            chk("Read id=" + tid + " 존재", found);
            if (found) stats["read_found"]++;
            if (logIt) { logE["type"]="READ"; logE["id"]=tid; logE["found"]=found;
                         if (c) logE["data"]=cToJ(*c); }
            break;
        }
        case UP: {
            string tid   = db.randAlive(gen.rng);
            int    field = gen.ri(1, 6);
            string nv;
            switch (field) {
                case 2: nv = "upd" + to_string(ui) + "@samsung.com"; break;
                case 3: nv = "010-" + to_string(1000+(ui%9000)) + "-" + to_string(1000+ui%8999); break;
                case 4: { const char* R[]={"CL1","CL2","CL3","CL4","Master","Fellow","상무"};
                           nv = R[ui%7]; break; }
                default: nv = "upd_" + to_string(ui); break;
            }
            ui++;

            json before; const Contact* bc = db.read(tid);
            if (bc) before = cToJ(*bc);

            bool ok = db.update(tid, field, nv);
            chk("Update id=" + tid, ok);
            bool verified = false;
            json after;
            if (ok) {
                const Contact* ac = db.read(tid);
                if (ac) {
                    after = cToJ(*ac);
                    verified = (getField(*ac, field) == nv);
                    chk("Update 반영 id=" + tid + " field=" + to_string(field),
                        verified, "기대=" + nv + " 실제=" + getField(*ac, field));
                    if (verified) stats["update_ok"]++;
                }
            }
            if (logIt) { logE["type"]="UPDATE"; logE["id"]=tid; logE["field"]=field;
                         logE["val"]=nv; logE["ok"]=ok;
                         logE["before"]=before; logE["after"]=after; }
            break;
        }
        case DL: {
            string tid = db.randAlive(gen.rng);
            json before; const Contact* bc = db.read(tid);
            if (bc) before = cToJ(*bc);
            bool ok = db.del(tid);
            chk("Delete id=" + tid, ok);
            if (ok) {
                bool gone = (db.read(tid) == nullptr);
                chk("Delete 후 부재 id=" + tid, gone);
                if (gone) stats["delete_ok"]++;
            }
            if (logIt) { logE["type"]="DELETE"; logE["id"]=tid;
                         logE["ok"]=ok; logE["before"]=before; }
            break;
        }
        }
        if (logIt) opSample.push_back(logE);

        // 체크포인트
        if ((i+1) % CHKPT_EVERY == 0) {
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
                    if (!mc || mc->emp_no!=lc.emp_no || mc->name!=lc.name ||
                        mc->email!=lc.email || mc->phone!=lc.phone ||
                        mc->rank!=lc.rank || mc->department!=lc.department ||
                        mc->memo!=lc.memo) { dmOk=false; break; }
                }
                chk("체크포인트#" + to_string(chkptN) + " 데이터 일치", dmOk);
            }
            cout << "  체크포인트 " << chkptN
                 << " (" << (i+1) << "/" << TOTAL_OPS
                 << ") DB=" << db.size() << "건 "
                 << (szOk&&dmOk ? "✓" : "✗") << "\n";
        }
    }

    // ══════════════════════════════════════════════════════
    // 최종 무결성
    // ══════════════════════════════════════════════════════
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
            if (!mc || mc->emp_no!=lc.emp_no || mc->name!=lc.name ||
                mc->email!=lc.email || mc->phone!=lc.phone ||
                mc->rank!=lc.rank || mc->department!=lc.department ||
                mc->memo!=lc.memo) { fdmOk=false; break; }
        }
        chk("최종 데이터 일치 (in-memory vs file)", fdmOk);

        // ID 중복 없음
        vector<string> fids;
        for (const auto& c : fl) fids.push_back(c.id);
        sort(fids.begin(), fids.end());
        chk("최종 ID 중복 없음",
            unique(fids.begin(), fids.end()) == fids.end());

        // 사번 8자리 숫자 형식
        bool empNoFmt = true;
        for (const auto& c : fl) {
            if (c.emp_no.size() != 8) { empNoFmt=false; break; }
            for (char ch : c.emp_no)
                if (!isdigit((unsigned char)ch)) { empNoFmt=false; break; }
        }
        chk("사번 8자리 숫자 형식 유효", empNoFmt);

        // ID 형식: {영문소문자}.{영문소문자} 포함
        bool idFmt = true;
        for (const auto& c : fl) {
            if (c.id.find('.') == string::npos) { idFmt=false; break; }
        }
        chk("ID {이니셜숫자}.{성영문} 형식 유효 (. 포함)", idFmt);

        // 이메일 @samsung.com
        bool emailOk = true;
        for (const auto& c : fl)
            if (c.email.find("@samsung.com") == string::npos) { emailOk=false; break; }
        chk("이메일 @samsung.com 형식", emailOk);
    }

    // TC 저장
    { json od=json::array();
      for (const auto& c:fl) od.push_back(cToJ(c));
      wJson(tcDir+TC_SEP+"output.json", od); }
    { json s;
      s["description"]="처음 100 + 마지막 100 연산 샘플 (before/after 포함)";
      s["ops"]=opSample;
      wJson(tcDir+TC_SEP+"operations_sample.json", s); }
    stats["final_db_size"]=(long long)db.size();
    stats["total_pass"]=gPass; stats["total_fail"]=gFail;
    wSummary(tcDir+TC_SEP+"summary.txt", stats);

    // 콘솔 결과
    cout << "\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
    cout << "Regression 결과: PASS " << gPass << " / FAIL " << gFail << "\n";
    cout << "  CREATE : " << stats["create_ok"]  << " / " << OPS_PER_TYPE << "\n";
    cout << "  READ   : " << stats["read_found"] << " / " << OPS_PER_TYPE << "\n";
    cout << "  UPDATE : " << stats["update_ok"]  << " / " << OPS_PER_TYPE << "\n";
    cout << "  DELETE : " << stats["delete_ok"]  << " / " << OPS_PER_TYPE << "\n";
    cout << "  DB 최종 : " << db.size() << "건\n";
    for (const auto& s : gFails) cout << s << "\n";
    cout << (gFail==0 ? "전체 통과\n" : "실패 항목 있음\n");
    cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
    cout << "TC 출력 경로: " << tcDir << "\n";

    remove(TEST_FILE.c_str());
    return gFail > 0 ? 1 : 0;
}
