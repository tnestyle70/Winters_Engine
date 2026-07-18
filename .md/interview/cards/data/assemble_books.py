# -*- coding: utf-8 -*-
# Assembles two printable HTML books from the master-answer-book workflow output:
#   05_좌표_답안집_4프로젝트.html  (coordinate cards, 5-step, code-grounded)
#   06_고아노드_답안집_D1-D11.html (orphan nodes: D1~D3 base carried from 04 + supplements + D4~D11)
import json, re, io, sys, os

TASK_OUT = r"C:\Users\user\AppData\Local\Temp\claude\C--Users-user-Desktop-Winters\90fc721d-b601-4c0f-b014-aa0b6d0066a7\tasks\wha40twgv.output"
BOOK04   = r"C:\Users\user\Downloads\04_고아노드_답안집.html"
OUT05    = r"C:\Users\user\Downloads\05_좌표_답안집_4프로젝트.html"
OUT06    = r"C:\Users\user\Downloads\06_고아노드_답안집_D1-D11.html"

with io.open(TASK_OUT, "r", encoding="utf-8") as f:
    data = json.load(f)["result"]

# ---------- carry over D1~D3 base nodes from 04 ----------
base_entries = []
try:
    with io.open(BOOK04, "r", encoding="utf-8") as f:
        t04 = f.read()
    i0 = t04.index("const D = [") + len("const D = ")
    i1 = t04.index("];", i0)
    base_entries = json.loads(t04[i0:i1+1])
    print("04 base entries parsed:", len(base_entries))
except Exception as e:
    print("WARN: could not parse 04 base nodes:", e)

AXDEF = {
 "C1":("기준계","절대 표현은 없다","어떤 연산 ↔ 다른 연산","지배 연산이 바뀌는 순간 (조회가 순회를 이기면 SoA가 진다)"),
 "C2":("이동 > 계산","대역폭은 유한","지역성 ↔ 유연성","워킹셋이 캐시에 다 들어가는 순간"),
 "C3":("공유는 비싸다","코히런스는 비싸다","동기화 ↔ 중복·불균형","코어가 1개이거나 경합이 0인 순간"),
 "C4":("수명은 선언된다","관찰은 비싸다","예측가능성 ↔ 편의","수명이 진짜로 비결정적인 레이어 (스크립트·툴)"),
 "C5":("이산화와 오차","연속은 예산 밖","정확도 ↔ 예산","오차 허용치가 샘플링 한계보다 커지는 순간"),
 "C6":("가지치기","공간 > 예산","안전 ↔ 속도","공간이 예산 안에 다 들어가는 순간 (전수 탐색 가능)"),
 "C7":("권위와 정합성","진실은 하나","정합성 ↔ 지연·확장","사본이 하나뿐인 순간 (싱글플레이)"),
 "C8":("검증이 병목","확신 대역폭도 유한하다","변경 속도 ↔ 회귀 안전","실험이 공짜로 되돌려지는 순간 (프로토타입)"),
}
K = list(AXDEF.keys())

def dip(on):
    cells = "".join('<i class="%s">%s</i>' % ("on" if k in on else "", k[1]) for k in K)
    return '<span class="dip">%s</span>' % cells

CSS = """
:root{--ink:#14161a;--hair:#c8ccd2;--faint:#f0f2f4;--grey:#5c636c;
 --mono:"SFMono-Regular","Cascadia Mono","JetBrains Mono",Consolas,monospace;
 --kr:"Pretendard",-apple-system,"Apple SD Gothic Neo","Malgun Gothic","Noto Sans KR",sans-serif;}
*{box-sizing:border-box;margin:0;padding:0}
body{background:#8a8f96;font-family:var(--kr);color:var(--ink);padding:16px}
.bar{max-width:210mm;margin:0 auto 16px;background:#fff;border:1px solid var(--ink);padding:10px 14px;
 font:12px/1.5 var(--mono);display:flex;gap:14px;flex-wrap:wrap;align-items:center;justify-content:space-between}
.bar button{font:11px var(--mono);padding:5px 12px;border:1px solid var(--ink);background:var(--ink);color:#fff;cursor:pointer}
.page{width:210mm;min-height:297mm;margin:0 auto 16px;background:#fff;padding:11mm 10mm}
.top{border-bottom:2.4px solid var(--ink);padding-bottom:3mm;margin-bottom:4mm}
.top h1{font:800 17px/1.2 var(--kr)} .top p{font:9px/1.5 var(--mono);color:var(--grey);margin-top:1.4mm}
.dip{display:inline-flex;gap:1px;border:1px solid var(--ink);padding:1px;background:#fff;vertical-align:middle}
.dip i{width:4.6mm;height:4.6mm;display:inline-flex;align-items:center;justify-content:center;
 font:700 6.5px/1 var(--mono);background:#fff;color:var(--hair);font-style:normal}
.dip i.on{background:var(--ink);color:#fff}
.ax{width:100%;border-collapse:collapse;font-size:8.6px;margin-bottom:4mm}
.ax td,.ax th{border-bottom:.4px solid var(--faint);padding:1.2mm 1mm;vertical-align:top;text-align:left;line-height:1.4}
.ax th{font:700 8px var(--mono);border-bottom:1.2px solid var(--ink)}
.ax td:first-child{font:700 9px var(--mono);white-space:nowrap}
.rulebox{border:1.2px solid var(--ink);padding:2.6mm 3mm;margin-bottom:4mm;font:8.8px/1.55 var(--kr)}
.rulebox b{font-weight:700}
.pre{font:8.4px/1.6 var(--mono);white-space:pre-wrap;background:var(--faint);padding:2mm;margin:1.2mm 0}
.spgrid{display:grid;grid-template-columns:1fr 1fr;gap:3mm;margin-bottom:4mm}
.sp{border:.5px solid var(--hair);padding:2mm 2.4mm}
.sp h3{font:700 9px var(--mono);display:flex;justify-content:space-between;align-items:center;margin-bottom:1mm}
.sp pre{font:7.8px/1.6 var(--mono);white-space:pre-wrap}
.card{break-inside:avoid;border:1.4px solid var(--ink);margin-bottom:3.4mm;padding:2.6mm 3mm}
.chd{display:flex;justify-content:space-between;align-items:flex-start;gap:6px;border-bottom:1.2px solid var(--ink);
 padding-bottom:1.6mm;margin-bottom:1.6mm}
.cid{font:800 13px/1 var(--mono)} .cttl{font:700 10.5px/1.35 var(--kr);margin-top:1mm}
.cmeta{font:8px/1.4 var(--mono);color:var(--grey);margin-top:.8mm}
.ask{font:8.6px/1.5 var(--kr);border-left:2px solid var(--ink);padding-left:2mm;margin-bottom:1.6mm}
.ask i{font-style:normal;font:700 7.5px var(--mono);margin-right:1.2mm}
.slot{margin-bottom:1.3mm;font:8.6px/1.5 var(--kr)}
.slot b.tag{font:700 8px var(--mono);margin-right:1.4mm}
.anch{font:7.2px/1.5 var(--mono);color:var(--grey);border-top:.4px solid var(--faint);padding-top:1.2mm;margin-top:1.4mm;word-break:break-all}
.cf{font:7.8px/1.5 var(--kr);color:var(--grey);margin-top:.8mm}
.cf i{font-style:normal;font-weight:700;color:var(--ink)}
.sec{background:var(--ink);color:#fff;padding:2mm 2.6mm;margin:4mm 0 2.6mm;font:700 11px/1.3 var(--mono);
 display:flex;justify-content:space-between;align-items:center}
.sec s{font:400 8px var(--kr);text-decoration:none;opacity:.75;text-align:right;max-width:120mm}
.cols{column-count:2;column-gap:7mm;column-rule:.4px solid var(--faint)}
.dh{break-inside:avoid;break-after:avoid;background:var(--ink);color:#fff;padding:1.6mm 2.4mm;margin:0 0 2.6mm;
 font:700 10px/1.3 var(--mono);display:flex;justify-content:space-between;align-items:center}
.dh s{font:400 8px var(--kr);text-decoration:none;opacity:.7;text-align:right;max-width:70mm}
.dh:not(:first-child){margin-top:4mm}
.n{break-inside:avoid;margin-bottom:3.4mm;padding-bottom:2.6mm;border-bottom:.4px solid var(--faint)}
.nh{display:flex;justify-content:space-between;align-items:baseline;gap:4px;margin-bottom:1.2mm}
.nm{font:700 9.6px/1.3 var(--kr)} .nc{font:700 7.5px/1 var(--mono);color:var(--grey);white-space:nowrap}
.core{font:8.6px/1.5 var(--kr);border-left:1.8px solid var(--ink);padding-left:2mm;margin-bottom:1.4mm}
.num{font:8px/1.45 var(--mono);background:var(--faint);padding:1.2mm 2mm;margin-bottom:1.4mm;white-space:pre-wrap}
.qa{font:8.2px/1.45 var(--kr);margin-bottom:1mm}
.qa i{font-style:normal;font:700 7.5px var(--mono);color:var(--grey);margin-right:1.4mm}
.trap{font:7.8px/1.4 var(--kr);color:var(--grey)} .trap i{font-style:normal;font-weight:700;color:var(--ink)}
.end{border-top:2.4px solid var(--ink);margin-top:5mm;padding-top:3mm;font:8.6px/1.6 var(--kr)}
@page{size:A4;margin:0}
@media print{body{background:#fff;padding:0}.bar{display:none}.page{margin:0;page-break-after:always}}
"""

SLOT_LABELS = [("①","문제 · 제약과 숫자","s1"),("②","순진한 해법의 실패","s2"),("③","메커니즘 · 숫자","s3"),
               ("④","대조 — 수렴/발산/부분","s4"),("⑤","대가 · 언제 틀리나","s5")]

def render_card(c):
    axes = c.get("axes", [])
    h  = '<div class="card"><div class="chd"><div>'
    h += '<span class="cid">%s</span><div class="cttl">%s</div><div class="cmeta">%s · 축 %s</div></div>%s</div>' % (
        c["id"], c["title"], c.get("meta",""), " ".join(axes), dip(axes))
    h += '<div class="ask"><i>묻는 것</i>%s</div>' % c.get("ask","")
    for sym, lab, key in SLOT_LABELS:
        h += '<div class="slot"><b class="tag">%s %s</b>%s</div>' % (sym, lab, c.get(key,""))
    if c.get("anchors"):
        h += '<div class="anch">%s</div>' % " · ".join(c["anchors"])
    for cf in c.get("confirm", []) or []:
        h += '<div class="cf"><i>CONFIRM_NEEDED</i> %s</div>' % cf
    return h + '</div>'

SPINES = [
 ("D1","컴퓨터구조",["C2","C3","C5"],"전자는 유한 속도로 움직인다\n → 계산은 싸지는데 이동은 안 싸진다 (Dennard 종료 + memory wall)\n → 계층으로 숨긴다 → 메모리 계층 / 병렬로 우회한다 → SIMD·멀티코어·GPU\n → 일관성 문제 → MESI·메모리모델 / 표현 문제 → IEEE754·엔디안\n → 명령 하나가 어떻게 도나 → 실행 모델 / 측정 없으면 추측 → 관측"),
 ("D2","운영체제",["C3","C4"],"하드웨어는 하나, 프로그램은 여럿\n → 시간을 나눈다 → 스케줄링·실행 단위 / 공간을 나눈다 → 가상 메모리\n → 보호한다 → 커널 경계 / 나누면 공유가 생긴다 → 동기화\n → 나누면 대화가 필요하다 → IPC / I/O는 느리다 → I/O 모델"),
 ("D3","윈도우 시스템 프로그래밍",["C4","C1"],"Windows 커널은 객체 지향이다\n → 모든 게 오브젝트, 접근은 핸들 → 핸들 / 핸들은 수명이 있다 [C4] → RAII\n → 코드는 PE, 로더가 붙인다 → 모듈 / 호출은 스택 위에서 → 스택·규약\n → 데이터는 힙에서 산다 → 힙·할당자 / 그리고 언젠가 죽는다 → 크래시"),
 ("D4","C++",["C4","C1"],"수명을 런타임에 관찰하지 않고 프로그램 구조에 묶는다\n → 이 공리 하나에서 전부 유도: RAII·소유권·이동·예외 안전·zero-overhead\n → GC는 이 공리를 거부한 것 — 그래서 그 대가(히칭)를 낸다"),
 ("D5","컴파일러 · 링킹",["C1","C4"],"텍스트 → 기계어, 5단계: 전처리·컴파일·어셈블·링크·로드\n → 에러의 종류가 단계를 알려준다 → UB = 최적화 라이선스 → 링킹이 면접 최빈출"),
 ("D6","자료구조 · 알고리즘",["C2","C6"],"점근이 아니라 캐시가 이긴다 [C2]\n탐색 공간은 항상 예산보다 크다 [C6]\n → 두 문장이 전 챕터를 생성한다 ※ Sparse Set은 여기가 아니라 W02"),
 ("D7","수학 · 물리",["C1","C5"],"게임 엔진은 거대한 미분·적분기다 [C5]\n모든 좌표는 어떤 기준계에 상대적 [C1]\n → 변환·회전·적분기·충돌 → 오차 통제가 곧 설계"),
 ("D8","그래픽스",["C1","C2","C5","C6"],"상태를 픽셀로 투사한다\n → 기준계를 옮기고 [C1] / 대역폭을 아끼고 [C2] / 샘플링하고 [C5] / 안 보이는 걸 버린다 [C6]\n ※ Winters 대조 시 전제: 우리는 아직 포워드다 (W07/W08/W10/W11)"),
 ("D9","네트워크 · 서버 · DB",["C7","C3","C2"],"여기 상태와 저기 상태를 일치시킨다\n → 유한 채널로 [C2] / 하나의 진실로 [C7] / 공유는 최소로 [C3]\n 게임 넷코드도 DB도 같은 문제"),
 ("D10","게임 AI · RL",["C6","C5"],"유한 예산에서 \"지금 뭘 할까\"를 푼다\n → 안 볼 걸 버린다 [C6] / 못 푸는 건 추정한다 [C5]\n ※ IL(행동복제)은 이제 실재한다 — 67피처 .wbc, SHADOW_ONLY. 빈 노드는 RL뿐"),
 ("D11","게임 엔진 종합",["C1","C2","C3","C4","C5","C6","C7","C8"],"위 전부의 교집합 — 새 지식이 아니라 통합 좌표계\n → 7층 · 루프 · ECS · Job · 리소스 · 툴/협업 · 그리고 게이트 [C8]"),
]

def header_block():
    h = '<table class="ax"><tr><th>축</th><th>이름 · 제약</th><th>tradeoff (⑤의 형태)</th><th>틀려지는 순간 (⑤의 \"언제\")</th></tr>'
    for k,(nm,con,tr,off) in AXDEF.items():
        h += '<tr><td>%s</td><td><b>%s</b> — %s</td><td>%s</td><td>%s</td></tr>' % (k,nm,con,tr,off)
    h += '</table>'
    h += ('<div class="rulebox"><b>문제 정의 규칙 (①=⑤).</b> [제약·숫자] 안에 [목표]를 해야 하는데 [순진한 해법]으로 하면 [측정값] → 예산의 [%]. '
          '숫자가 없으면 문제가 아니라 취향이다. ⑤가 안 나오면 ⑤를 고민하지 말고 ①로 돌아간다 — 예산 초과로 정의된 문제만 대가를 낳는다. '
          '<b>⑤의 \"언제 틀리나\" = 그 축의 제약이 소멸하는 파라미터 영역.</b><br>'
          '<b>축 오인 경고.</b> 채점 전에 \"묻는 것\" 줄과 네 답의 축을 대조하라. W01 사례: 수명(C4)을 묻는 좌표에 캐시(C2)로 답하면 '
          '내용이 맞아도 실패다 — 그 답은 W02의 것이다.</div>')
    h += '<div class="spgrid">'
    for sid, st, ax, sp in SPINES:
        h += '<div class="sp"><h3><span>%s · %s</span>%s</h3><pre>%s</pre></div>' % (sid, st, dip(ax), sp)
    h += '</div>'
    return h

def page(title, sub, body):
    return ('<div class="page"><div class="top"><h1>%s</h1><p>%s</p></div>%s</div>' % (title, sub, body))

# ---------- Book 05: coordinates ----------
sections = [
 ("WINTERS · 엔진 코어/렌더 (W01~W22)", "coordW", "레포 실사 앵커 기반 · 상태(구현|계획|비활성) 명시"),
 ("WINTERS · 서버/결정론/백엔드 (S01~S20)", "coordS", "30Hz 결정론 계약과 그 청구서"),
 ("NYPC · 버섯게임 + NEXT NATION(전장) (N01~N15)", "coordN", "제출 리플레이 문서 + 레포 이식 문서 기반"),
 ("스타크래프트 WinAPI · 마크던전스 · Liberation (SC/MD/LB)", "coordExp", "이력서 weapons 원장 기반 — 성장 서사의 ④는 Winters"),
]
body05 = header_block()
count05 = 0
for sec_title, key, sec_sub in sections:
    blk = data.get(key) or {}
    cards = blk.get("cards", [])
    count05 += len(cards)
    body05 += '<div class="sec">%s<s>%s</s></div>' % (sec_title, sec_sub)
    for c in cards:
        body05 += render_card(c)
body05 += ('<div class="end"><b>사용법.</b> 이 책은 채점표다 — 처음부터 읽으면 그게 재독이다. '
           '빈 문서에 축 8 → 스파인 → 좌표 id만 보고 5칸 구두 재현 → 이 책으로 숫자와 앵커를 대조한다. '
           '숫자만 정답지, 사슬은 반례로 검증. 성공 ×2.5 간격 / 실패 1일 리셋 + 막힌 축 기록. '
           '좌표 실패 = 몰랐다가 아니라 <b>근거 없이 결정했다</b>. CONFIRM_NEEDED 항목은 너만 채울 수 있다 — 지어내는 순간 첫 꼬리질문에서 무너진다.</div>')

html05 = ('<!DOCTYPE html><html lang="ko"><head><meta charset="utf-8"><title>좌표 답안집 · 4프로젝트</title>'
          '<style>%s</style></head><body><div class="bar"><span><b>05 · 좌표 답안집</b> · Winters/NYPC/스타/던전스 · 5단계 · 코드 실사판</span>'
          '<button onclick="window.print()">인쇄</button></div>%s</body></html>') % (CSS, page(
            "05 · 좌표 답안집 — 4프로젝트 × 5단계 (코드 실사판)",
            "축 C1~C8 · 스파인 11 · 좌표 전개 — 웹 세션의 빈칸 카드(03)를 레포 ground truth로 채운 판. 03 카드는 인출 연습용으로 계속 사용.",
            body05))

with io.open(OUT05, "w", encoding="utf-8") as f:
    f.write(html05)
print("05 written:", count05, "cards,", len(html05), "bytes")

# ---------- Book 06: orphan nodes ----------
def render_node(e):
    if len(e) == 2:  # chapter header from base 04 format
        return '<div class="dh">%s<s>%s</s></div>' % (e[0], e[1])
    ch, nm, fl, core, num, q, a, trap = (e + [""]*8)[:8]
    h  = '<div class="n"><div class="nh"><div class="nm">%s</div><div class="nc">%s%s</div></div>' % (nm, ch, (" "+fl) if fl else "")
    h += '<div class="core">%s</div>' % core
    if num: h += '<div class="num">%s</div>' % num
    h += '<div class="qa"><i>Q</i>%s</div><div class="qa"><i>A</i>%s</div>' % (q, a)
    if trap: h += '<div class="trap"><i>함정 </i>%s</div>' % trap
    return h + '</div>'

def node_to_row(n, supplement=False):
    name = ("✚ " if supplement else "") + n["name"]
    return [n.get("ch",""), name, n.get("flag",""), n.get("core",""), n.get("num",""),
            n.get("q",""), n.get("a",""), n.get("trap","")]

# organize: D1~D3 base + supplements, then D4~D11 from agents
supp_nodes = (data.get("nodesSupp") or {}).get("nodes", [])
supp_by_domain = {}
for n in supp_nodes:
    dom = n.get("ch","")[:2] if n.get("ch","").startswith("D") else "D?"
    dom = n.get("ch","").split("-")[0]
    supp_by_domain.setdefault(dom, []).append(n)

rows = []
count06 = 0
if base_entries:
    cur_dom = None
    for e in base_entries:
        if len(e) == 2:  # header like "§ D1 · 컴퓨터구조"
            # flush previous domain's supplements before new header
            if cur_dom and cur_dom in supp_by_domain:
                for n in sorted(supp_by_domain.pop(cur_dom), key=lambda x: x.get("ch","")):
                    rows.append(node_to_row(n, supplement=True)); count06 += 1
            m = re.search(r"D(\d+)", e[0])
            cur_dom = "D%s" % m.group(1) if m else None
            rows.append(e)
        else:
            rows.append(e); count06 += 1
    if cur_dom and cur_dom in supp_by_domain:
        for n in sorted(supp_by_domain.pop(cur_dom), key=lambda x: x.get("ch","")):
            rows.append(node_to_row(n, supplement=True)); count06 += 1
    # any leftover supplements (unmatched domain)
    for dom, ns in supp_by_domain.items():
        for n in ns:
            rows.append(node_to_row(n, supplement=True)); count06 += 1
else:
    rows.append(["§ D1~D3", "원본 04 답안집을 함께 사용 (파싱 실패로 본판에 미포함)"])
    for n in supp_nodes:
        rows.append(node_to_row(n, supplement=True)); count06 += 1

SPINE_MAP = {sid: sp for sid, _, _, sp in [(s[0], s[1], s[2], s[3]) for s in SPINES]}
TITLE_MAP = {s[0]: s[1] for s in SPINES}
order = ["nodesD45", "nodesD67", "nodesD811", "nodesD910"]
dom_order = {"nodesD45":["D4","D5"], "nodesD67":["D6","D7"], "nodesD811":["D8","D11"], "nodesD910":["D9","D10"]}
for key in order:
    blk = data.get(key) or {}
    nodes = blk.get("nodes", [])
    by_dom = {}
    for n in nodes:
        dom = n.get("ch","").split("-")[0]
        by_dom.setdefault(dom, []).append(n)
    for dom in dom_order[key]:
        ns = by_dom.pop(dom, [])
        spine_line = SPINE_MAP.get(dom, "")
        first = spine_line.split("\n")[0] if spine_line else ""
        rows.append(["§ %s · %s" % (dom, TITLE_MAP.get(dom, dom)), first])
        for n in sorted(ns, key=lambda x: x.get("ch","")):
            rows.append(node_to_row(n)); count06 += 1
    for dom, ns in by_dom.items():  # stray chapter codes
        for n in ns:
            rows.append(node_to_row(n)); count06 += 1

blocks = "".join(render_node(e) for e in rows)
body06 = '<div class="cols">%s</div>' % blocks
body06 += ('<div class="end"><b>사용법.</b> 스파인에서 챕터를 유도 → 노드 이름만 보고 90초 구두 → 이 페이지로 채점. '
           '✚ 표시는 이번 확장에서 보충된 노드. D1~D3 원본 노드는 04와 동일 — 이 책 한 권으로 D1~D11 전체를 커버한다. '
           '실패하면 02 정답지에 항목 추가 + 막힌 축 기록 + 1일 리셋. 성공하면 아무것도 쓰지 않는다. 간격 ×2.5.</div>')

html06 = ('<!DOCTYPE html><html lang="ko"><head><meta charset="utf-8"><title>고아 노드 답안집 · D1–D11</title>'
          '<style>%s</style></head><body><div class="bar"><span><b>06 · 고아 노드 답안집 확장판</b> · D1~D11 전 도메인 · 3-B 포맷</span>'
          '<button onclick="window.print()">인쇄</button></div>%s</body></html>') % (CSS, page(
            "06 · 고아 노드 답안집 — D1 컴퓨터구조 ~ D11 엔진 종합 (확장판)",
            "좌표가 부르지 않는 노드 전부 · 핵심 → 숫자 → Q/A → 함정 · 이건 외운다 · 소환 노드는 05 좌표 답안집에",
            body06))

with io.open(OUT06, "w", encoding="utf-8") as f:
    f.write(html06)
print("06 written:", count06, "nodes,", len(html06), "bytes")
