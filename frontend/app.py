"""
CineSearch backend: loads the C++ indexer's output, builds an in-memory
inverted index, and serves a JSON search + autocomplete API and the frontend.

Run:   python app.py                 (expects lexicon.txt, forward_index.txt, docs.txt in ./data)
       CINE_DATA=/path/to/files python app.py
"""
import bisect, heapq, math, os, re, time
from collections import defaultdict
from flask import Flask, jsonify, request, send_from_directory

DATA_DIR = os.environ.get("CINE_DATA", "data")
SITE_URL = "https://movies.stackexchange.com"   # CHANGE to the StackExchange site your Posts.xml came from
K1, B, TITLE_BOOST = 1.2, 0.75, 1.5             # BM25 parameters + boost for words in title/tags
STOP_DF_RATIO = 0.4                             # words in >40% of docs are ignored (indexer keeps stopwords)
TOKEN_RE = re.compile(r"[a-z0-9]+")             # mirrors tokenize() in text_utils.h

def tokenize(text):
    return [w for w in TOKEN_RE.findall(text.lower()) if len(w) <= 30]

# ---------------------------------------------------------------- loading
class Engine:
    def __init__(self, data_dir):
        t0 = time.time()
        p = lambda n: os.path.join(data_dir, n)
        self.lexicon = {}                      # word -> wordId
        with open(p("lexicon.txt"), encoding="utf-8", errors="ignore") as f:
            for line in f:
                parts = line.split()
                if len(parts) == 2:
                    self.lexicon[parts[0]] = int(parts[1])

        self.inverted = defaultdict(list)      # wordId -> [(docId, freq, inTitle)]
        self.doclen = []
        with open(p("forward_index.txt"), encoding="utf-8") as f:
            for line in f:
                parts = line.split()
                if not parts:
                    continue
                doc, total = int(parts[0]), 0
                for item in parts[1:]:
                    w, fq, t = item.split(":")
                    fq = int(fq)
                    self.inverted[int(w)].append((doc, fq, int(t)))
                    total += fq
                self.doclen.append(total)

        self.docs, self.by_post = [], {}       # docs.txt line number == docId
        with open(p("docs.txt"), encoding="utf-8", errors="ignore") as f:
            for line in f:
                c = line.rstrip("\n").split("\t")
                c += [""] * (10 - len(c))
                d = dict(postId=c[1], type="question" if c[2] == "1" else "answer",
                         score=int(c[3] or 0), views=int(c[4] or 0), date=c[5][:10],
                         tags=[t for t in c[6].split(",") if t], title=c[7],
                         snippet=c[8], parentId=c[9])
                self.by_post[c[1]] = len(self.docs)
                self.docs.append(d)

        self.N = len(self.docs)
        self.avgdl = (sum(self.doclen) / len(self.doclen)) if self.doclen else 1
        # autocomplete vocabulary: sorted words with document frequency
        self.vocab, self.df = [], {}
        for w, wid in self.lexicon.items():
            n = len(self.inverted.get(wid, ()))
            if n >= 2 and len(w) >= 2 and not w.isdigit():   # skip typos / bare numbers
                self.df[w] = n
        self.vocab = sorted(self.df)
        print(f"Loaded {self.N} docs, {len(self.lexicon)} words in {time.time()-t0:.1f}s")

    # ------------------------------------------------------------- search
    def search(self, query, page=1, per_page=10, doc_type="all"):
        terms = [t for t in dict.fromkeys(tokenize(query)) if t in self.lexicon]
        all_terms = list(dict.fromkeys(tokenize(query)))
        active = [t for t in terms if self.df_of(t) <= STOP_DF_RATIO * self.N] or terms
        scores, matched = defaultdict(float), defaultdict(int)
        for t in active:
            plist = self.inverted[self.lexicon[t]]
            df = len(plist)
            idf = math.log(1 + (self.N - df + 0.5) / (df + 0.5))
            for doc, fq, in_title in plist:
                if doc_type != "all" and self.docs[doc]["type"] != doc_type:
                    continue
                norm = fq * (K1 + 1) / (fq + K1 * (1 - B + B * self.doclen[doc] / self.avgdl))
                scores[doc] += idf * norm * (TITLE_BOOST if in_title else 1)
                matched[doc] += 1
        # documents containing more of the query words always rank first
        ranked = sorted(scores, key=lambda d: (matched[d], scores[d] * (1 + 0.1 * math.log1p(max(self.docs[d]["score"], 0)))), reverse=True)
        total = len(ranked)
        start = (page - 1) * per_page
        results = [self.format(d, scores[d], matched[d], len(active)) for d in ranked[start:start + per_page]]
        return dict(query=query, terms=all_terms, total=total, page=page,
                    pages=max(1, math.ceil(total / per_page)), results=results)

    def df_of(self, term):
        return len(self.inverted[self.lexicon[term]])

    def format(self, doc, score, matched, nterms):
        d = self.docs[doc]
        title, kind = d["title"], d["type"]
        if kind == "answer":                    # answers have no title: borrow the question's
            parent = self.docs[self.by_post[d["parentId"]]] if d["parentId"] in self.by_post else None
            title = parent["title"] if parent else f"Answer {d['postId']}"
            if parent and not d["tags"]:
                d = {**d, "tags": parent["tags"]}
        path = "q" if kind == "question" else "a"
        return dict(id=doc, type=kind, title=title, snippet=d["snippet"], tags=d["tags"],
                    score=d["score"], views=d["views"], date=d["date"],
                    url=f"{SITE_URL}/{path}/{d['postId']}",
                    relevance=round(score, 2), matched=f"{matched}/{nterms}")

    # ------------------------------------------------------- autocomplete
    def autocomplete(self, text, limit=8):
        m = re.search(r"[a-z0-9]+$", text.lower())
        if not m:
            return []
        prefix, head = m.group(0), text[:m.start()]
        lo = bisect.bisect_left(self.vocab, prefix)
        hi = bisect.bisect_left(self.vocab, prefix + "\uffff")
        best = heapq.nlargest(limit, self.vocab[lo:hi], key=self.df.get)
        return [dict(text=head + w, word=w, count=self.df[w]) for w in best]

# -------------------------------------------------------------- web app
app = Flask(__name__, static_folder="static")
engine = Engine(DATA_DIR)

@app.get("/")
def index():
    return send_from_directory("static", "index.html")

@app.get("/api/search")
def api_search():
    q = request.args.get("q", "").strip()
    if not q:
        return jsonify(error="Missing query parameter 'q'"), 400
    page = max(1, request.args.get("page", 1, type=int))
    per_page = min(50, max(1, request.args.get("per_page", 10, type=int)))
    kind = request.args.get("type", "all")
    if kind not in ("all", "question", "answer"):
        return jsonify(error="type must be all, question or answer"), 400
    t0 = time.perf_counter()
    out = engine.search(q, page, per_page, kind)
    out["took_ms"] = round((time.perf_counter() - t0) * 1000, 1)
    return jsonify(out)

@app.get("/api/autocomplete")
def api_autocomplete():
    return jsonify(suggestions=engine.autocomplete(request.args.get("q", "")))

@app.get("/api/stats")
def api_stats():
    return jsonify(documents=engine.N, words=len(engine.lexicon))

if __name__ == "__main__":
    app.run(debug=False, port=int(os.environ.get("PORT", 5000)))
