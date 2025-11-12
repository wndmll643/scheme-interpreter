#include "eval.h"
#include <cstdio>
#include <cstdlib>
#include <cctype>

// ------- 간단 유틸 -------
static inline int  NIL() { return 0; }
static inline bool is_list(int n)   { return n > 0; }   // cons
static inline bool is_atom(int n)   { return n <= 0; }  // () 또는 심볼
static inline bool is_symbol(int n) { return n < 0; }   // 심볼(원자)
static inline int  car(const NodeArray& na, int p){ return na.getTable()[p].lchild; }
static inline int  cdr(const NodeArray& na, int p){ return na.getTable()[p].rchild; }
static inline int  cons(NodeArray& na, int a, int d){
    int k = na.alloc(); if (!k) return 0;
    na.getTable()[k].lchild = a; na.getTable()[k].rchild = d; return k;
}

// 숫자 문자열 여부 판정
static bool is_num_string(const std::string& s){
    if (s.empty()) return false;
    size_t i = (s[0]=='+'||s[0]=='-')? 1 : 0;
    if (i>=s.size()) return false;
    for (; i<s.size(); ++i) if (!std::isdigit((unsigned char)s[i])) return false;
    return true;
}

// 심볼(음수 인덱스) → 이름
static const std::string* sym_name(const HashTable& ht, int sym_idx){
    return ht.nameByIndex(sym_idx); // sym_idx<0 전제 :contentReference[oaicite:2]{index=2}
}

// 숫자 원자 만들기: 정수 → 문자열 → intern() → 음수 인덱스
static int make_number(HashTable& ht, NodeArray& na, long long v){
    char buf[64]; std::snprintf(buf, sizeof(buf), "%lld", v);
    return ht.intern(buf, &na); // 음수 반환 :contentReference[oaicite:3]{index=3}
}

// 숫자 읽기: 음수 심볼 인덱스 → 이름 → 정수
static long long read_number(const HashTable& ht, int sym_idx){
    const std::string* s = sym_name(ht, sym_idx);
    return (s && is_num_string(*s)) ? std::atoll(s->c_str()) : 0;
}

// 바인딩 접근: HashTable 엔트리의 rchild를 값 포인터로 사용
static inline int entry_from_index(int idx){ return -idx; } // index=-entry 설계 이용 :contentReference[oaicite:4]{index=4}
static inline hash_node_t* HT(HashTable& ht){ return ht.getTable(); }

// TRUE / FALSE 심볼
static int intern_true(HashTable& ht, NodeArray& na){ return ht.intern("#t", &na); }
static int intern_false(HashTable& ht, NodeArray& na){ return ht.intern("#f", &na); }
static bool is_true(const HashTable& ht, int v){
    // Scheme 규칙 간단화: '#f'만 거짓
    const std::string* s = is_symbol(v) ? sym_name(ht, v) : nullptr;
    return !(s && *s == "#f");
}

// 내장 심볼 캐싱
struct Builtin {
    int PLUS, MINUS, MUL, DIV, MOD;
    int CONS, CAR, CDR, LIST;
    int NULLP, NOTP;
    int EQNUM, LT, GT, LE, GE; // 숫자 비교
    int DEFINE, COND, IF, AND, OR, ELSE;
    int DISPLAY, REVERSE, LENGTH, MAX, FLOOR, CEILING; // 필요시
    int QUOTE, LAMBDA; // 전처리 후에도 호환용
};
static Builtin load_bi(HashTable& ht){
    Builtin b{};
    b.PLUS   = ht.intern("+", nullptr);
    b.MINUS  = ht.intern("-", nullptr);
    b.MUL    = ht.intern("*", nullptr);
    b.DIV    = ht.intern("/", nullptr);
    b.MOD    = ht.intern("modulo", nullptr);

    b.CONS   = ht.intern("cons", nullptr);
    b.CAR    = ht.intern("car", nullptr);
    b.CDR    = ht.intern("cdr", nullptr);
    b.LIST   = ht.intern("list", nullptr);

    b.NULLP  = ht.intern("null?", nullptr);
    b.NOTP   = ht.intern("not", nullptr);

    b.EQNUM  = ht.intern("=", nullptr);
    b.LT     = ht.intern("<", nullptr);
    b.GT     = ht.intern(">", nullptr);
    b.LE     = ht.intern("<=", nullptr);
    b.GE     = ht.intern(">=", nullptr);

    b.DEFINE = ht.intern("define", nullptr);
    b.COND   = ht.intern("cond", nullptr);
    b.IF     = ht.intern("if", nullptr);
    b.AND    = ht.intern("and", nullptr);
    b.OR     = ht.intern("or", nullptr);
    b.ELSE   = ht.intern("else", nullptr);

    b.DISPLAY= ht.intern("display", nullptr);
    b.REVERSE= ht.intern("reverse", nullptr);
    b.LENGTH = ht.intern("length", nullptr);
    b.MAX    = ht.intern("max", nullptr);
    b.FLOOR  = ht.intern("floor", nullptr);
    b.CEILING= ht.intern("ceiling", nullptr);

    b.QUOTE  = ht.intern("quote", nullptr);
    b.LAMBDA = ht.intern("lambda", nullptr);
    return b;
}

// ------- 핵심: Eval -------
int EVAL(int root, HashTable& ht, NodeArray& na){
    // 원자: () 또는 심볼
    if (!is_list(root)) {
        if (root == NIL()) return NIL();
        if (is_symbol(root)) {
            // 심볼 바인딩 조회: table[-entry].rchild 를 값으로 사용
            int entry = entry_from_index(root);
            hash_node_t* T = HT(ht);
            int bound = T[entry].rchild;         // 없으면 0
            return (bound != 0) ? bound : root;  // 바인딩 없으면 자기 자신
        }
        return root; // 방어
    }

    // 리스트: (op . args)
    const Builtin bi = load_bi(ht);
    int op  = car(na, root);    // 심볼(음수) 기대
    int args= cdr(na, root);

    // QUOTE: (quote x) → x
    if (op == bi.QUOTE) {
        return car(na, args);
    }

    // DEFINE: (define name expr)  또는  (define f (lambda ...))
    if (op == bi.DEFINE) {
        int lhs = car(na, args);
        int rhs = car(na, cdr(na, args));
        int val = EVAL(rhs, ht, na);
        if (!is_symbol(lhs)) return lhs; // 전처리 가정
        int entry = entry_from_index(lhs);
        HT(ht)[entry].rchild = val;      // 바인딩 저장
        return lhs;
    }

    // IF: (if test then else)
    if (op == bi.IF) {
        int t  = car(na, args);
        int th = car(na, cdr(na, args));
        int el = car(na, cdr(na, cdr(na, args)));
        int tv = EVAL(t, ht, na);
        return is_true(ht, tv) ? EVAL(th, ht, na) : EVAL(el, ht, na);
    }

    // COND: ((c1 e1) (c2 e2) ... (else e)) 형태
    if (op == bi.COND) {
        int it = args;
        while (it != NIL()) {
            int pair = car(na, it);         // (cond expr)
            int c = car(na, pair);
            int e = car(na, cdr(na, pair));
            if (is_symbol(c) && c == bi.ELSE) {
                return EVAL(e, ht, na);
            } else {
                int cv = EVAL(c, ht, na);
                if (is_true(ht, cv)) return EVAL(e, ht, na);
            }
            it = cdr(na, it);
        }
        return NIL();
    }

    // AND / OR (단축 평가)
    if (op == bi.AND) {
        int it = args, last = intern_true(ht, na);
        while (it != NIL()) {
            last = EVAL(car(na, it), ht, na);
            if (!is_true(ht, last)) return last;
            it = cdr(na, it);
        }
        return last;
    }
    if (op == bi.OR) {
        int it = args;
        while (it != NIL()) {
            int v = EVAL(car(na, it), ht, na);
            if (is_true(ht, v)) return v;
            it = cdr(na, it);
        }
        return intern_false(ht, na);
    }

    // null? / not
    if (op == bi.NULLP) {
        int v = EVAL(car(na, args), ht, na);
        return (v == NIL()) ? intern_true(ht, na) : intern_false(ht, na);
    }
    if (op == bi.NOTP) {
        int v = EVAL(car(na, args), ht, na);
        return is_true(ht, v) ? intern_false(ht, na) : intern_true(ht, na);
    }

    // cons / car / cdr / list
    if (op == bi.CONS) {
        int a = EVAL(car(na, args), ht, na);
        int d = EVAL(car(na, cdr(na, args)), ht, na);
        return cons(na, a, d);
    }
    if (op == bi.CAR) {
        int lst = EVAL(car(na, args), ht, na);
        return is_list(lst) ? car(na, lst) : NIL();
    }
    if (op == bi.CDR) {
        int lst = EVAL(car(na, args), ht, na);
        return is_list(lst) ? cdr(na, lst) : NIL();
    }
    if (op == bi.LIST) {
        int it = args, head = NIL(), tail = NIL();
        while (it != NIL()) {
            int v = EVAL(car(na, it), ht, na);
            int cell = cons(na, v, NIL());
            if (head == NIL()) head = tail = cell;
            else { na.getTable()[tail].rchild = cell; tail = cell; }
            it = cdr(na, it);
        }
        return head;
    }

    // 숫자 비교 (= < > <= >=) — 인자 2개 가정
    if (op==bi.EQNUM || op==bi.LT || op==bi.GT || op==bi.LE || op==bi.GE) {
        int a = EVAL(car(na, args), ht, na);
        int b = EVAL(car(na, cdr(na, args)), ht, na);
        if (!is_symbol(a) || !is_symbol(b)) return intern_false(ht, na);
        long long x = read_number(ht, a), y = read_number(ht, b);
        bool ok = (op==bi.EQNUM)?(x==y)
                :(op==bi.LT)?(x<y)
                :(op==bi.GT)?(x>y)
                :(op==bi.LE)?(x<=y)
                            :(x>=y);
        return ok ? intern_true(ht, na) : intern_false(ht, na);
    }

    // 사칙연산 + modulo — 인자 2개 가정
    if (op==bi.PLUS || op==bi.MINUS || op==bi.MUL || op==bi.DIV || op==bi.MOD) {
        int a = EVAL(car(na, args), ht, na);
        int b = EVAL(car(na, cdr(na, args)), ht, na);
        long long x = read_number(ht, a), y = read_number(ht, b);
        long long r = (op==bi.PLUS)? x+y :
                      (op==bi.MINUS)? x-y :
                      (op==bi.MUL)? x*y :
                      (op==bi.DIV)? (y? x/y : 0) :
                      (y? x%y : 0);
        return make_number(ht, na, r);
    }

    // 함수 호출: (f arg...)  —  f 가 심볼이고, 그 바인딩이 (lambda (params) body)
    if (is_symbol(op)) {
        int f_entry = entry_from_index(op);
        int lam = HT(ht)[f_entry].rchild;
        if (is_list(lam) && is_symbol(car(na, lam)) && car(na, lam)==bi.LAMBDA) {
            int pb = cdr(na, lam);           // (params . body)
            int params = car(na, pb);        // (p1 p2 ...)
            int body   = car(na, cdr(na, pb)); // body 1식 가정

            // 파라미터 바인딩 저장해 두기(동적 스코프 간단 구현)
            int saved_sym[32]; int saved_val[32]; int saved_n=0;

            int ps = params, as = args;
            while (ps != NIL() && as != NIL() && saved_n < 32) {
                int p = car(na, ps);                // 심볼
                int a = EVAL(car(na, as), ht, na);  // 인자 값
                int pe = entry_from_index(p);
                saved_sym[saved_n] = pe;
                saved_val[saved_n] = HT(ht)[pe].rchild; // 기존 값 보관
                HT(ht)[pe].rchild = a;                 // 바인딩
                ++saved_n;
                ps = cdr(na, ps);
                as = cdr(na, as);
            }

            int ret = EVAL(body, ht, na);

            // 바인딩 복원
            for (int i = 0; i < saved_n; ++i) HT(ht)[saved_sym[i]].rchild = saved_val[i];
            return ret;
        }
    }

    // 그 외: (op ...) 형태인데 op가 바인딩도 lambda도 아니면, 그 자체를 리스트로 취급
    return root;
}
