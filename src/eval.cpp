#include "eval.h"
#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <stdexcept>
#include <string>
#include <cmath>

static inline int  NIL() { return 0; }
static inline bool is_list(int n)   { return n > 0; }
static inline bool is_atom(int n)   { return n <= 0; }
static inline bool is_symbol(int n) { return n < 0; }
static inline int entry_from_index(int index){ return -index; }

static inline int  car(const NodeArray& na, int p){ return na.getTable()[p].lchild; }
static inline int  cdr(const NodeArray& na, int p){ return na.getTable()[p].rchild; }
static inline int  cons(NodeArray& na, int a, int d){
    int k = na.alloc(); if (!k) return 0;
    na.getTable()[k].lchild = a; na.getTable()[k].rchild = d; return k;
}

static const std::string* sym_name(const HashTable& ht, int sym_idx){
    return ht.nameByIndex(sym_idx);
}
static inline bool is_sym_name(const HashTable& ht, int sym, const char* name){
    if (!is_symbol(sym)) return false;
    const std::string* s = sym_name(ht, sym);
    return (s && *s == name);
}
static inline bool is_truthy(const HashTable& ht, int v){
    return !(v==NIL() || is_sym_name(ht, v, "#f"));
}
static long long read_number(const HashTable& ht, int sym){
    const std::string* s = sym_name(ht, sym);
    if (!s || s->empty()) return 0;
    size_t i = 0; bool neg = false;
    if ((*s)[0]=='+' || (*s)[0]=='-'){ neg = ((*s)[0]=='-'); i=1; }
    long long v=0;
    for (; i<s->size(); ++i){
        if (!std::isdigit((unsigned char)(*s)[i])) break;
        v = v*10 + ((*s)[i]-'0');
    }
    return neg? -v : v;
}
static double read_double(const HashTable& ht, int sym){
    const std::string* s = sym_name(ht, sym);
    if (!s || s->empty()) return 0.0;
    const char* cstr = s->c_str();
    char* endp = nullptr;
    double v = std::strtod(cstr, &endp);
    return v;
}

static int list_length(const NodeArray& na, int lst){
    int n=0; while(is_list(lst)){ ++n; lst = cdr(na, lst); } return n;
}

static int deep_copy(int n, NodeArray& na){
    if (n <= 0) return n;
    int a = deep_copy(na.getTable()[n].lchild, na);
    int d = deep_copy(na.getTable()[n].rchild, na);
    int k = na.alloc(); if (!k) return 0;
    na.getTable()[k].lchild = a;
    na.getTable()[k].rchild = d;
    return k;
}

static int reverse_list(int xs, NodeArray& na){
    int out = NIL();
    while (is_list(xs)) {
        out = cons(na, car(na, xs), out);
        xs  = cdr(na, xs);
    }
    return out;
}

int EVAL(int root, HashTable& ht, NodeArray& na){
    if (root == NIL()) return NIL();

    if (is_atom(root)) {
        if (is_symbol(root)) {
            int entry = entry_from_index(root);
            int bound = ht.getTable()[entry].rchild;
            if (bound == 0) return root;
            return bound;
        }
        return root;
    }

    int op = car(na, root);
    int args = cdr(na, root);

    if (is_sym_name(ht, op, "quote")) {
        if (list_length(na, args) != 1) throw std::runtime_error("quote: arity mismatch");
        return car(na, args);
    }

    if (is_sym_name(ht, op, "if")) {
        if (list_length(na, args) != 3) throw std::runtime_error("if: arity mismatch");
        int cnd = EVAL(car(na, args), ht, na);
        int thn = car(na, cdr(na, args));
        int els = car(na, cdr(na, cdr(na, args)));
        if (cnd == NIL() || is_sym_name(ht, cnd, "#f")) return EVAL(els, ht, na);
        return EVAL(thn, ht, na);
    }

    if (is_sym_name(ht, op, "define")) {
        if (list_length(na, args) != 2) throw std::runtime_error("define: arity mismatch");
        int lhs = car(na, args);
        int rhs = car(na, cdr(na, args));
        if (!is_symbol(lhs)) throw std::runtime_error("define: lhs must be a symbol");
        int val = EVAL(rhs, ht, na);
        int entry = entry_from_index(lhs);
        ht.getTable()[entry].rchild = (val>0 ? deep_copy(val, na) : val);
        return lhs;
    }

    if (is_sym_name(ht, op, "cons")) {
        if (list_length(na, args) != 2) throw std::runtime_error("cons: arity mismatch");
        int a = EVAL(car(na, args), ht, na);
        int d = EVAL(car(na, cdr(na, args)), ht, na);
        if (!(d == NIL() || is_list(d))) throw std::runtime_error("cons: second arg must be list");
        return cons(na, a, d);
    }

    if (is_sym_name(ht, op, "car") || is_sym_name(ht, op, "cdr")) {
        if (list_length(na, args) != 1) throw std::runtime_error("car/cdr: arity mismatch");
        int x = EVAL(car(na, args), ht, na);
        if (!is_list(x)) throw std::runtime_error("car/cdr: argument must be non-empty list");
        return is_sym_name(ht, op, "car") ? car(na, x) : cdr(na, x);
    }

    if (is_sym_name(ht, op, "cond")) {
        int clauses = args;
        while (is_list(clauses)) {
            int clause = car(na, clauses);
            if (!is_list(clause)) throw std::runtime_error("cond: clause must be list");
            int test = car(na, clause);
            int body = cdr(na, clause);

            bool take = false;
            if (is_symbol(test) && is_sym_name(ht, test, "else")) {
                take = true;
            } else {
                int tv = EVAL(test, ht, na);
                take = is_truthy(ht, tv);
            }

            if (take) {
                int result = NIL();
                if (!is_list(body)) {
                    return (is_symbol(test) && is_sym_name(ht, test, "else")) ? NIL() : EVAL(test, ht, na);
                }
                while (is_list(body)) {
                    result = EVAL(car(na, body), ht, na);
                    body   = cdr(na, body);
                }
                return result;
            }
            clauses = cdr(na, clauses);
        }
        return NIL();
    }

    if (is_sym_name(ht, op, "null?")) {
        if (list_length(na, args) != 1) throw std::runtime_error("null?: arity mismatch");
        int x = EVAL(car(na, args), ht, na);
        return ht.intern((x==NIL()) ? "#t" : "#f", &na);
    }

    if (is_sym_name(ht, op, "not")) {
        if (list_length(na, args) != 1) throw std::runtime_error("not: arity mismatch");
        int x = EVAL(car(na, args), ht, na);
        return ht.intern(is_truthy(ht, x) ? "#f" : "#t", &na);
    }

    if (is_sym_name(ht, op, "and")) {
        if (!is_list(args)) return ht.intern("#t", &na);
        int it = args;
        int last = ht.intern("#t", &na);
        while (is_list(it)) {
            last = EVAL(car(na, it), ht, na);
            if (!is_truthy(ht, last)) return last;
            it = cdr(na, it);
        }
        return last;
    }

    if (is_sym_name(ht, op, "or")) {
        if (!is_list(args)) return ht.intern("#f", &na);
        int it = args;
        while (is_list(it)) {
            int v = EVAL(car(na, it), ht, na);
            if (is_truthy(ht, v)) return v;
            it = cdr(na, it);
        }
        return ht.intern("#f", &na);
    }

    if (is_sym_name(ht, op, "list")) {
        int rev = NIL();
        int it = args;
        while (is_list(it)) {
            int v = EVAL(car(na, it), ht, na);
            rev = cons(na, v, rev);
            it  = cdr(na, it);
        }
        return reverse_list(rev, na);
    }

    if (is_sym_name(ht, op, "reverse")) {
        if (list_length(na, args) != 1) throw std::runtime_error("reverse: arity mismatch");
        int xs = EVAL(car(na, args), ht, na);
        if (!(xs==NIL() || is_list(xs))) throw std::runtime_error("reverse: argument must be list");
        return reverse_list(xs, na);
    }

    if (is_sym_name(ht, op, "length")) {
        if (list_length(na, args) != 1) throw std::runtime_error("length: arity mismatch");
        int xs = EVAL(car(na, args), ht, na);
        if (!(xs==NIL() || is_list(xs))) throw std::runtime_error("length: argument must be list");
        int n = 0;
        while (is_list(xs)) { ++n; xs = cdr(na, xs); }
        return ht.intern(std::to_string(n), &na);
    }

    if (is_sym_name(ht, op, "max")) {
        if (!is_list(args)) throw std::runtime_error("max: needs at least 1 argument");
        bool first = true;
        long long best = 0;
        int it = args;
        while (is_list(it)) {
            int v = EVAL(car(na, it), ht, na);
            long long x = read_number(ht, v);
            if (first) { best = x; first=false; }
            else if (x > best) best = x;
            it = cdr(na, it);
        }
        if (first) throw std::runtime_error("max: no arguments");
        return ht.intern(std::to_string(best), &na);
    }

    if (is_sym_name(ht, op, "min")) {
        if (!is_list(args)) throw std::runtime_error("min: needs at least 1 argument");
        bool first = true;
        long long best = 0;
        int it = args;
        while (is_list(it)) {
            int v = EVAL(car(na, it), ht, na);
            long long x = read_number(ht, v);
            if (first) { best = x; first=false; }
            else if (x < best) best = x;
            it = cdr(na, it);
        }
        if (first) throw std::runtime_error("min: no arguments");
        return ht.intern(std::to_string(best), &na);
    }

    if (is_sym_name(ht, op, "floor") || is_sym_name(ht, op, "ceiling")) {
        if (list_length(na, args) != 1) throw std::runtime_error("floor/ceiling: arity mismatch");
        int v = EVAL(car(na, args), ht, na);
        if (!is_symbol(v)) throw std::runtime_error("floor/ceiling: argument must be a number");

        double x = read_double(ht, v);
        long long r = 0;
        if (is_sym_name(ht, op, "floor")) {
            r = static_cast<long long>(std::floor(x));
        } else {
            r = static_cast<long long>(std::ceil(x));
        }
        return ht.intern(std::to_string(r), &na);
    }

    if (is_sym_name(ht, op, "display")) {
        if (list_length(na, args) != 1) throw std::runtime_error("display: arity mismatch");
        int x = EVAL(car(na, args), ht, na);
        PRINT(x, ht, na);
        std::printf("\n");
        return x;
    }

    if (is_sym_name(ht, op, "=") || is_sym_name(ht, op, "<") || is_sym_name(ht, op, ">") ||
        is_sym_name(ht, op, "<=") || is_sym_name(ht, op, ">=")) {
        if (list_length(na, args) != 2) throw std::runtime_error("compare: arity mismatch");
        int a = EVAL(car(na, args), ht, na);
        int b = EVAL(car(na, cdr(na, args)), ht, na);
        if (!(is_symbol(a) && is_symbol(b))) return ht.intern("#f", &na);
        long long x = read_number(ht, a), y = read_number(ht, b);
        bool ok = is_sym_name(ht, op, "=")  ? (x==y)
                : is_sym_name(ht, op, "<")  ? (x<y)
                : is_sym_name(ht, op, ">")  ? (x>y)
                : is_sym_name(ht, op, "<=") ? (x<=y)
                                            : (x>=y);
        return ht.intern(ok ? "#t" : "#f", &na);
    }

    if (is_sym_name(ht, op, "+") || is_sym_name(ht, op, "-") || is_sym_name(ht, op, "*") ||
        is_sym_name(ht, op, "/") || is_sym_name(ht, op, "modulo")) {
        if (list_length(na, args) != 2) throw std::runtime_error("arith: arity mismatch");
        int a = EVAL(car(na, args), ht, na);
        int b = EVAL(car(na, cdr(na, args)), ht, na);
        long long x = read_number(ht, a), y = read_number(ht, b);
        long long r=0;
        if (is_sym_name(ht, op, "+")) r = x+y;
        else if (is_sym_name(ht, op, "-")) r = x-y;
        else if (is_sym_name(ht, op, "*")) r = x*y;
        else if (is_sym_name(ht, op, "/")) { if (y==0) throw std::runtime_error("division by zero"); r = x/y; }
        else { if (y==0) throw std::runtime_error("mod by zero"); r = x%y; }
        return ht.intern(std::to_string(r), &na);
    }

    if (is_sym_name(ht, op, "lambda")) {
        return root;
    }

    if (is_list(op) && is_sym_name(ht, car(na, op), "lambda")) {
        int bound = op;
        int lam_params = car(na, cdr(na, bound));
        int body       = car(na, cdr(na, cdr(na, bound)));
        int as = args;
        int ps = lam_params;
        int saved_sym[128]; int saved_val[128]; int saved_n=0;
        while (is_list(ps) && is_list(as)) {
            int p = car(na, ps);
            int a = EVAL(car(na, as), ht, na);
            if (!is_symbol(p)) throw std::runtime_error("lambda: parameter not symbol");
            int pe = entry_from_index(p);
            saved_sym[saved_n] = pe;
            saved_val[saved_n] = ht.getTable()[pe].rchild;
            ++saved_n;
            ht.getTable()[pe].rchild = a;
            ps = cdr(na, ps);
            as = cdr(na, as);
        }
        int ret = EVAL(body, ht, na);
        for (int i=0;i<saved_n;++i) ht.getTable()[saved_sym[i]].rchild = saved_val[i];
        return ret;
    }

    if (is_symbol(op)) {
        int f_entry = entry_from_index(op);
        int bound   = ht.getTable()[f_entry].rchild;
        if (bound != 0 && is_list(bound) && is_sym_name(ht, car(na, bound), "lambda")) {
            int lam_params = car(na, cdr(na, bound));
            int body       = car(na, cdr(na, cdr(na, bound)));

            int as = args;
            int ps = lam_params;
            int saved_sym[128]; int saved_val[128]; int saved_n=0;

            while (is_list(ps) && is_list(as)) {
                int p = car(na, ps);
                int a = EVAL(car(na, as), ht, na);
                if (!is_symbol(p)) throw std::runtime_error("lambda: parameter not symbol");

                int pe = entry_from_index(p);
                saved_sym[saved_n] = pe;
                saved_val[saved_n] = ht.getTable()[pe].rchild;
                ++saved_n;

                ht.getTable()[pe].rchild = a;

                ps = cdr(na, ps);
                as = cdr(na, as);
            }

            int ret = EVAL(body, ht, na);

            for (int i=0;i<saved_n;++i) ht.getTable()[saved_sym[i]].rchild = saved_val[i];
            return ret;
        }

        std::string opname = sym_name(ht, op) ? *sym_name(ht, op) : std::string("?");
        if (bound == 0) throw std::runtime_error("attempt to call unbound symbol as procedure: " + opname);
        throw std::runtime_error("attempt to call non-procedure: " + opname);
    }

    throw std::runtime_error("attempt to call non-symbol as procedure");
}
