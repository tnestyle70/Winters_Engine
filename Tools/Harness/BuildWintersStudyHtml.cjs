#!/usr/bin/env node

const fs = require('fs');
const path = require('path');
const crypto = require('crypto');
const { Marked } = require('marked');

const ROOT = path.resolve(__dirname, '..', '..');
const OUTPUT_DIR = path.join(ROOT, '.md', '이력서', 'html');

const primaryDocs = [
  {
    id: 'easy-guide',
    group: 'START · 쉬운 설명',
    label: 'EASY',
    title: '이력서 기술 도메인 쉬운설명서',
    source: '.md/이력서/2026-07-19_이력서_기술_도메인_쉬운설명서.md',
    note: '처음부터 읽는 메인 교재 · 쉬운 말 → 실제 구조 → 수치 → 이력서',
  },
  {
    id: 'engine-index',
    group: 'START · 쉬운 설명',
    label: 'MAP',
    title: 'Winters Engine 학습 지도',
    source: '.md/interview/engine/00_INDEX.md',
    note: '전체 문서 지도와 권장 학습 순서',
  },
];

const conquestTitles = [
  'Frame · Tick · Game Loop',
  '성능 숫자를 읽는 법',
  'Engine 구조 · 계층 경계',
  'ECS · EntityHandle',
  '객체 수명 · 소유권 · 메모리',
  'JobSystem · Scheduler',
  'Renderer · RHI · GPU',
  'Resource · Asset Pipeline',
  'UI · Tool · Editor',
  'Server Authority · GameSim',
  'Network · Replication',
  'Determinism · Replay',
  'AI · Navigation · Movement',
];

const conquestFiles = [
  'CONQUEST_01_FRAME_TICK_GAME_LOOP.md',
  'CONQUEST_02_PERFORMANCE_NUMBERS.md',
  'CONQUEST_03_ENGINE_ARCHITECTURE_BOUNDARY.md',
  'CONQUEST_04_ECS_ENTITY_HANDLE.md',
  'CONQUEST_05_LIFETIME_OWNERSHIP_MEMORY.md',
  'CONQUEST_06_JOBSYSTEM_SCHEDULER.md',
  'CONQUEST_07_RENDERER_RHI_GPU_PIPELINE.md',
  'CONQUEST_08_RESOURCE_ASSET_PIPELINE.md',
  'CONQUEST_09_UI_TOOLS_EDITOR.md',
  'CONQUEST_10_SERVER_AUTHORITY_GAMESIM.md',
  'CONQUEST_11_NETWORK_REPLICATION.md',
  'CONQUEST_12_DETERMINISM_REPLAY.md',
  'CONQUEST_13_AI_NAVIGATION_MOVEMENT.md',
];

const conquestDocs = conquestFiles.map((file, index) => ({
  id: `conquest-${String(index + 1).padStart(2, '0')}`,
  group: 'CORE · 13개 도메인 정복',
  label: String(index + 1).padStart(2, '0'),
  title: conquestTitles[index],
  source: `.md/interview/engine/${file}`,
  note: '본질 · 코드 흐름 · 실측 · 과장 금지 · 면접 답변',
}));

const interviewDocs = [
  {
    id: 'final-playbook',
    group: 'INTERVIEW · 면접 환전',
    label: 'FINAL',
    title: '최종 면접 플레이북',
    source: '.md/interview/engine/CONQUEST_FINAL_INTERVIEW_PLAYBOOK.md',
    note: '13개 도메인 통합 · 숫자 덱 · War Story · 직무별 4불릿',
  },
  {
    id: 'oral-scorecard',
    group: 'INTERVIEW · 면접 환전',
    label: 'ORAL',
    title: '26문항 구두 검증 스코어카드',
    source: '.md/interview/engine/CONQUEST_ORAL_VERIFICATION_SCORECARD.md',
    note: '본질 · 흐름 · 수치 · 경계의 4점 채점표',
  },
  {
    id: 'oral-answer-key',
    group: 'INTERVIEW · 면접 환전',
    label: 'KEY',
    title: '26문항 구두 검증 답안 키',
    source: '.md/interview/engine/CONQUEST_ORAL_VERIFICATION_ANSWER_KEY.md',
    note: '답변 후 확인하는 60초 모범답안과 압박 경계',
  },
];

const deepReferenceFiles = [
  ['01_engine_overview.md', '엔진 전체 개관'],
  ['02_architecture_layers.md', '레이어 아키텍처 · 의존성 경계'],
  ['03_rendering_pipeline.md', '렌더링 파이프라인'],
  ['04_ecs_gameobject.md', 'ECS · 게임 오브젝트 모델'],
  ['05_resource_asset_pipeline.md', '리소스 · 에셋 파이프라인'],
  ['06_scene_gameloop.md', '씬 시스템 · 게임 루프'],
  ['07_network_replication.md', '네트워크 · 서버 권위 · 복제'],
  ['08_gamesim_champions.md', 'GameSim · 챔피언 시뮬레이션'],
  ['09_ai_navigation.md', 'AI · 내비게이션 · 이동'],
  ['10_concurrency_jobsystem.md', '동시성 구조 · JobSystem'],
  ['11_ui_tools_editor.md', 'UI · Tool · Editor'],
  ['12_error_resilience.md', '구조적 에러 처리 · 복원력'],
  ['13_collaboration_process.md', '협업 · 프로세스 · 개발 문화'],
  ['14_hard_problems_war_stories.md', '어려웠던 문제 · War Story'],
  ['15_future_roadmap_techdebt.md', '향후 구조 · 기술 부채'],
  ['16_interview_qa_bank_engine.md', '엔진 면접 질문 은행 64문항'],
];

const deepReferenceDocs = deepReferenceFiles.map(([file, title], index) => ({
  id: `deep-${String(index + 1).padStart(2, '0')}`,
  group: 'REFERENCE · 심화 각론',
  label: `R${String(index + 1).padStart(2, '0')}`,
  title,
  source: `.md/interview/engine/${file}`,
  note: '심화 참고 · 현재 사실은 같은 도메인의 CONQUEST 정정을 우선',
}));

const resumeDocs = [
  {
    id: 'resume-bank',
    group: 'RESUME · 이력서 환전',
    label: 'BANK',
    title: '구현 · 실측 · 판단 이력서 문장 은행',
    source: '.md/이력서/2026-07-19_구현_실측_판단_이력서_문장은행.md',
    note: 'Winters · Minecraft · Starcraft · NYPC 문장과 증거 경계',
  },
  {
    id: 'resume-master',
    group: 'RESUME · 이력서 환전',
    label: 'MASTER',
    title: '이력서 MASTER',
    source: '.md/이력서/이력서_MASTER.md',
    note: '현재 이력서 전체 구성',
  },
  {
    id: 'resume-selection',
    group: 'RESUME · 이력서 환전',
    label: 'SELECT',
    title: '이력서 MASTER 선별장',
    source: '.md/이력서/이력서_MASTER_선별장.md',
    note: '헤드라인 후보 · 증거 강도 · 채택 여부',
  },
  {
    id: 'portfolio-master',
    group: 'RESUME · 이력서 환전',
    label: 'PORT',
    title: '포트폴리오 MASTER 설계',
    source: '.md/이력서/포트폴리오_MASTER_설계.md',
    note: '포트폴리오 페이지 구조와 전달 순서',
  },
  {
    id: 'notion-deep-dive',
    group: 'RESUME · 이력서 환전',
    label: 'DEEP',
    title: 'Notion 제출본 · Winters 딥다이브',
    source: '.md/이력서/notion_제출본/01_Winters_딥다이브.md',
    note: '제출용 Winters 기술 서사',
  },
  {
    id: 'notion-final',
    group: 'RESUME · 이력서 환전',
    label: 'SUBMIT',
    title: 'Notion 제출본 · SUBMIT FINAL',
    source: '.md/이력서/notion_제출본/SUBMIT_FINAL.md',
    note: '최종 제출본 전체',
  },
  {
    id: 'resume-interview-index',
    group: 'RESUME · 이력서 환전',
    label: 'Q-MAP',
    title: '이력서 면접 자료 INDEX',
    source: '.md/이력서/면접/00_INDEX.md',
    note: '기존 면접 자료와 질문 지도',
  },
];

const portalManifest = [
  ...primaryDocs,
  ...conquestDocs,
  ...interviewDocs,
  ...deepReferenceDocs,
  ...resumeDocs,
];

function escapeHtml(value) {
  return String(value)
    .replaceAll('&', '&amp;')
    .replaceAll('<', '&lt;')
    .replaceAll('>', '&gt;')
    .replaceAll('"', '&quot;')
    .replaceAll("'", '&#39;');
}

function escapeAttribute(value) {
  return escapeHtml(value).replaceAll('`', '&#96;');
}

function sha256(text) {
  return crypto.createHash('sha256').update(text, 'utf8').digest('hex').toUpperCase();
}

function normalizePath(value) {
  return path.normalize(value).toLowerCase();
}

function slugify(value) {
  const normalized = String(value)
    .replace(/<[^>]+>/g, '')
    .replace(/`/g, '')
    .normalize('NFKC')
    .toLowerCase()
    .replace(/[^\p{Letter}\p{Number}]+/gu, '-')
    .replace(/^-+|-+$/g, '');
  return normalized || 'section';
}

function safeJson(value) {
  return JSON.stringify(value).replace(/<\//g, '<\\/');
}

function countWords(source) {
  return source
    .replace(/```[\s\S]*?```/g, ' ')
    .replace(/[`#>*_\-|\[\]()]/g, ' ')
    .split(/\s+/)
    .filter(Boolean).length;
}

function loadDocs(manifest) {
  return manifest.map((entry, index) => {
    const abs = path.resolve(ROOT, entry.source);
    if (!fs.existsSync(abs)) {
      throw new Error(`Missing source: ${entry.source}`);
    }
    const sourceText = fs.readFileSync(abs, 'utf8').replace(/^\uFEFF/, '');
    const stat = fs.statSync(abs);
    return {
      ...entry,
      order: index,
      abs,
      sourcePath: entry.source,
      sourceText,
      bytes: Buffer.byteLength(sourceText, 'utf8'),
      lines: sourceText.split(/\r?\n/).length,
      words: countWords(sourceText),
      sha: sha256(sourceText),
      modified: stat.mtime.toISOString().slice(0, 10),
    };
  });
}

function extractHeadingTokens(source, docId) {
  const lexer = new Marked({ gfm: true }).lexer(source);
  const counts = new Map();
  return lexer
    .filter((token) => token.type === 'heading')
    .map((token) => {
      const base = slugify(token.text);
      const count = counts.get(base) || 0;
      counts.set(base, count + 1);
      return {
        depth: token.depth,
        text: token.text.replace(/`/g, ''),
        id: `doc-${docId}--${base}${count ? `-${count + 1}` : ''}`,
      };
    });
}

function wrapTopicCards(html) {
  const h2Pattern = /<h2\b[^>]*>[\s\S]*?<\/h2>\s*/gi;
  const h3Pattern = /<h3\b[^>]*>[\s\S]*?<\/h3>\s*/gi;

  function collectMatches(pattern, source) {
    pattern.lastIndex = 0;
    return [...source.matchAll(pattern)];
  }

  function wrapH3Cards(source) {
    const matches = collectMatches(h3Pattern, source);
    if (!matches.length) {
      return source.trim() ? `<div class="topic-card">${source}</div>` : '';
    }

    const cards = [];
    const intro = source.slice(0, matches[0].index).trim();
    if (intro) cards.push(`<div class="topic-card topic-overview">${intro}</div>`);

    matches.forEach((match, index) => {
      const end = index + 1 < matches.length ? matches[index + 1].index : source.length;
      cards.push(`<div class="topic-card">${source.slice(match.index, end).trim()}</div>`);
    });
    return cards.join('\n');
  }

  const sections = [];
  const h2Matches = collectMatches(h2Pattern, html);
  if (!h2Matches.length) {
    return `<section class="topic-section topic-section-root"><div class="topic-grid">${wrapH3Cards(html)}</div></section>`;
  }

  const prelude = html.slice(0, h2Matches[0].index).trim();
  if (prelude) {
    sections.push(`<section class="topic-section topic-section-root"><div class="topic-grid"><div class="topic-card topic-intro">${prelude}</div></div></section>`);
  }

  h2Matches.forEach((match, index) => {
    const end = index + 1 < h2Matches.length ? h2Matches[index + 1].index : html.length;
    const body = html.slice(match.index + match[0].length, end).trim();
    const hasH3 = /<h3\b/i.test(body);
    if (hasH3) {
      sections.push(`<section class="topic-section">${match[0].trim()}<div class="topic-grid">${wrapH3Cards(body)}</div></section>`);
    } else {
      sections.push(`<section class="topic-section topic-section-single"><div class="topic-grid"><div class="topic-card topic-card-h2">${match[0].trim()}${body}</div></div></section>`);
    }
  });

  return sections.join('\n');
}

function createMarkdownRenderer(doc, docsByPath) {
  const headings = extractHeadingTokens(doc.sourceText, doc.id);
  let headingCursor = 0;
  const marked = new Marked({ gfm: true, breaks: false });

  marked.use({
    renderer: {
      heading(token) {
        const heading = headings[headingCursor++];
        const text = this.parser.parseInline(token.tokens);
        const level = token.depth;
        return `<h${level} id="${escapeAttribute(heading.id)}"><a class="heading-anchor" href="#${escapeAttribute(heading.id)}" aria-label="이 제목으로 이동">#</a>${text}</h${level}>\n`;
      },
      code(token) {
        const language = (token.lang || '').trim();
        const label = language || 'TEXT';
        const cls = language ? ` language-${escapeAttribute(language)}` : '';
        const mermaid = language.toLowerCase() === 'mermaid' ? ' mermaid-source' : '';
        return `<figure class="code-block${mermaid}"><figcaption>${escapeHtml(label.toUpperCase())}</figcaption><pre><code class="${cls.trim()}">${escapeHtml(token.text)}</code></pre></figure>\n`;
      },
      link(token) {
        const href = token.href || '';
        const title = token.title ? ` title="${escapeAttribute(token.title)}"` : '';
        const label = this.parser.parseInline(token.tokens);
        if (/^(https?:|mailto:|tel:)/i.test(href)) {
          return `<a href="${escapeAttribute(href)}"${title} target="_blank" rel="noreferrer">${label}</a>`;
        }

        if (href.startsWith('#')) {
          const target = `doc-${doc.id}--${slugify(href.slice(1))}`;
          return `<a href="#${escapeAttribute(target)}"${title}>${label}</a>`;
        }

        const [linkPath, fragment = ''] = href.split('#', 2);
        const resolved = path.resolve(path.dirname(doc.abs), decodeURI(linkPath));
        const embedded = docsByPath.get(normalizePath(resolved));
        if (embedded) {
          const suffix = fragment ? `--${slugify(fragment)}` : '';
          const target = `doc-${embedded.id}${suffix}`;
          return `<a href="#${escapeAttribute(target)}" data-doc-link="${escapeAttribute(embedded.id)}"${title}>${label}</a>`;
        }

        const relative = path.relative(OUTPUT_DIR, resolved).replaceAll('\\', '/');
        const encoded = encodeURI(relative) + (fragment ? `#${encodeURIComponent(fragment)}` : '');
        return `<a href="${escapeAttribute(encoded)}"${title}>${label}</a>`;
      },
    },
  });

  return { html: wrapTopicCards(marked.parse(doc.sourceText)), headings };
}

function groupDocs(docs) {
  const groups = [];
  const byName = new Map();
  for (const doc of docs) {
    if (!byName.has(doc.group)) {
      const group = { name: doc.group, docs: [] };
      groups.push(group);
      byName.set(doc.group, group);
    }
    byName.get(doc.group).docs.push(doc);
  }
  return groups;
}

const STYLES = String.raw`
:root {
  color-scheme: light;
  --ink: #16181c;
  --muted: #606872;
  --hair: #c9cdd3;
  --hair-strong: #8d949d;
  --paper: #fcfcfb;
  --paper-soft: #f2f3f4;
  --canvas: #858b93;
  --canvas-soft: #dfe2e6;
  --accent: #15171b;
  --accent-ink: #ffffff;
  --good: #19704a;
  --warn: #996515;
  --danger: #a13434;
  --shadow: 0 18px 55px rgba(14, 18, 24, .18);
  --mono: "SFMono-Regular", "Cascadia Mono", "JetBrains Mono", Consolas, monospace;
  --kr: Pretendard, -apple-system, BlinkMacSystemFont, "Apple SD Gothic Neo", "Malgun Gothic", "Noto Sans KR", sans-serif;
  --reading: 210mm;
  --font-scale: 1;
  --topbar: 62px;
  --sidebar: 330px;
}

:root[data-theme="dark"] {
  color-scheme: dark;
  --ink: #eceef1;
  --muted: #a9b0b9;
  --hair: #3c424a;
  --hair-strong: #69717c;
  --paper: #17191d;
  --paper-soft: #22252a;
  --canvas: #0c0e11;
  --canvas-soft: #121419;
  --accent: #f2f3f5;
  --accent-ink: #14161a;
  --good: #6ed5a7;
  --warn: #e1b866;
  --danger: #e27c7c;
  --shadow: 0 18px 55px rgba(0, 0, 0, .34);
}

* { box-sizing: border-box; }
html { scroll-behavior: smooth; scroll-padding-top: calc(var(--topbar) + 24px); }
body {
  margin: 0;
  background: var(--canvas);
  color: var(--ink);
  font-family: var(--kr);
  font-size: calc(12px * var(--font-scale));
  line-height: 1.55;
  text-rendering: optimizeLegibility;
}
button, input { font: inherit; }
button { color: inherit; }
a { color: inherit; text-decoration-thickness: .08em; text-underline-offset: .18em; }
a:hover { text-decoration-thickness: .16em; }

.topbar {
  position: fixed;
  inset: 0 0 auto 0;
  z-index: 50;
  height: var(--topbar);
  display: grid;
  grid-template-columns: auto minmax(220px, 720px) auto;
  align-items: center;
  gap: 16px;
  padding: 9px 16px;
  background: color-mix(in srgb, var(--paper) 94%, transparent);
  border-bottom: 1px solid var(--hair-strong);
  backdrop-filter: blur(14px);
}
.brand { display: flex; align-items: center; gap: 11px; min-width: 0; }
.brand-mark {
  display: inline-grid;
  place-items: center;
  width: 36px;
  height: 36px;
  background: var(--accent);
  color: var(--accent-ink);
  font: 800 12px/1 var(--mono);
  letter-spacing: -.04em;
}
.brand-copy { min-width: 0; }
.brand-copy strong { display: block; font: 800 13px/1.25 var(--kr); white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
.brand-copy small { display: block; color: var(--muted); font: 10px/1.35 var(--mono); letter-spacing: .05em; }
.menu-toggle { display: none; }

.search-wrap { position: relative; }
.search-wrap input {
  width: 100%;
  height: 42px;
  padding: 0 48px 0 42px;
  border: 1px solid var(--hair-strong);
  border-radius: 0;
  outline: none;
  background: var(--paper-soft);
  color: var(--ink);
  font-size: 14px;
}
.search-wrap input:focus { border-color: var(--ink); box-shadow: 0 0 0 2px color-mix(in srgb, var(--ink) 16%, transparent); }
.search-icon { position: absolute; left: 14px; top: 10px; color: var(--muted); font: 16px var(--mono); }
.search-kbd { position: absolute; right: 12px; top: 10px; color: var(--muted); border: 1px solid var(--hair); padding: 1px 6px; font: 10px/1.6 var(--mono); }
.search-results {
  position: absolute;
  top: 48px;
  left: 0;
  right: 0;
  max-height: min(60vh, 520px);
  overflow: auto;
  display: none;
  background: var(--paper);
  border: 1px solid var(--hair-strong);
  box-shadow: var(--shadow);
}
.search-results.open { display: block; }
.search-result { display: block; width: 100%; padding: 11px 13px; border: 0; border-bottom: 1px solid var(--hair); background: transparent; text-align: left; cursor: pointer; }
.search-result:hover, .search-result:focus { background: var(--paper-soft); }
.search-result b { display: block; font-size: 13px; }
.search-result span { display: block; color: var(--muted); font: 10px/1.45 var(--mono); margin-top: 3px; }
.search-empty { padding: 18px; color: var(--muted); font-size: 13px; }

.toolbar { display: flex; justify-content: flex-end; gap: 6px; }
.tool-btn, .menu-toggle, .doc-action, .route-btn {
  border: 1px solid var(--hair-strong);
  background: var(--paper);
  min-width: 36px;
  min-height: 36px;
  padding: 6px 10px;
  cursor: pointer;
  font: 700 11px/1 var(--mono);
}
.tool-btn:hover, .menu-toggle:hover, .doc-action:hover, .route-btn:hover { background: var(--accent); color: var(--accent-ink); }

.sidebar {
  position: fixed;
  z-index: 40;
  top: var(--topbar);
  bottom: 0;
  left: 0;
  width: var(--sidebar);
  background: var(--paper);
  border-right: 1px solid var(--hair-strong);
  overflow: auto;
  overscroll-behavior: contain;
}
.sidebar-section { padding: 17px 15px; border-bottom: 1px solid var(--hair); }
.sidebar-kicker { color: var(--muted); font: 700 10px/1.4 var(--mono); letter-spacing: .08em; }
.progress-track { height: 5px; margin-top: 10px; background: var(--paper-soft); border: 1px solid var(--hair); }
.progress-value { display: block; height: 100%; width: 0; background: var(--good); transition: width .2s ease; }
.progress-copy { margin-top: 7px; display: flex; justify-content: space-between; color: var(--muted); font: 10px/1.3 var(--mono); }
.home-link, .doc-nav {
  display: grid;
  grid-template-columns: 42px 1fr 15px;
  align-items: center;
  gap: 8px;
  width: 100%;
  padding: 8px 10px;
  border: 0;
  border-left: 3px solid transparent;
  background: transparent;
  text-align: left;
  cursor: pointer;
  color: var(--ink);
}
.home-link:hover, .doc-nav:hover { background: var(--paper-soft); }
.home-link.active, .doc-nav.active { background: var(--accent); color: var(--accent-ink); border-left-color: var(--good); }
.doc-code { font: 800 9px/1.15 var(--mono); letter-spacing: .02em; }
.doc-name { font: 650 12px/1.35 var(--kr); }
.doc-state { width: 8px; height: 8px; border: 1px solid currentColor; border-radius: 50%; opacity: .5; }
.doc-nav.done .doc-state { background: var(--good); border-color: var(--good); opacity: 1; }
.nav-group { border-bottom: 1px solid var(--hair); }
.nav-group summary { padding: 12px 15px 9px; cursor: pointer; color: var(--muted); font: 800 10px/1.3 var(--mono); letter-spacing: .06em; list-style: none; }
.nav-group summary::-webkit-details-marker { display: none; }
.nav-group summary::before { content: '+ '; }
.nav-group[open] summary::before { content: '− '; }
.nav-list { padding: 0 8px 11px; }
.section-toc { padding: 0 15px 20px; }
.section-toc a { display: block; padding: 5px 0 5px 10px; border-left: 1px solid var(--hair); color: var(--muted); text-decoration: none; font-size: 11px; line-height: 1.35; }
.section-toc a.toc-h3 { padding-left: 22px; font-size: 10px; }
.section-toc a:hover, .section-toc a.active { color: var(--ink); border-left-color: var(--ink); }

.shell { min-height: 100vh; padding-top: var(--topbar); padding-left: var(--sidebar); }
.main { padding: 28px clamp(16px, 3.5vw, 54px) 90px; }
.home, .document { width: 210mm; max-width: 100%; margin: 0 auto; }
.home[hidden], .document[hidden] { display: none !important; }

.hero {
  background: var(--paper);
  border: 1px solid var(--ink);
  box-shadow: var(--shadow);
  padding: 9mm 10mm;
  margin-bottom: 12px;
}
.eyebrow { color: var(--muted); font: 800 8px/1.4 var(--mono); letter-spacing: .1em; }
.hero h1 { max-width: 850px; margin: 3mm 0; font-size: 26px; line-height: 1.06; letter-spacing: -.045em; }
.hero p { max-width: 760px; margin: 0; color: var(--muted); font-size: 10px; line-height: 1.55; }
.hero-rule { margin: 5mm 0 0; padding-top: 3mm; border-top: 2px solid var(--ink); font: 8px/1.55 var(--mono); }
.hero-actions { display: flex; flex-wrap: wrap; gap: 6px; margin-top: 4mm; }
.route-btn.primary { background: var(--accent); color: var(--accent-ink); }

.metric-grid, .route-grid, .number-grid, .group-grid { display: grid; gap: 12px; margin: 0 0 22px; }
.metric-grid { grid-template-columns: repeat(4, minmax(0, 1fr)); }
.route-grid { grid-template-columns: repeat(5, minmax(0, 1fr)); }
.number-grid { grid-template-columns: repeat(3, minmax(0, 1fr)); }
.group-grid { grid-template-columns: repeat(2, minmax(0, 1fr)); }
.panel, .metric, .route-card, .number-card, .group-card {
  background: var(--paper);
  border: 1px solid var(--hair-strong);
  padding: 18px;
}
.metric strong { display: block; font: 850 clamp(25px, 4vw, 42px)/1 var(--mono); letter-spacing: -.05em; }
.metric span { display: block; margin-top: 9px; color: var(--muted); font: 10px/1.4 var(--mono); }
.section-head { display: flex; justify-content: space-between; align-items: baseline; gap: 16px; margin: 28px 0 12px; }
.section-head h2 { margin: 0; font-size: 22px; }
.section-head span { color: var(--muted); font: 10px/1.4 var(--mono); }
.route-card b, .number-card b, .group-card b { display: block; margin-bottom: 8px; font-size: 14px; }
.route-card small, .number-card small, .group-card small { color: var(--muted); font-size: 12px; line-height: 1.55; }
.route-index { display: inline-grid; place-items: center; width: 25px; height: 25px; margin-bottom: 12px; background: var(--accent); color: var(--accent-ink); font: 800 10px var(--mono); }
.number-card b { font: 800 18px/1.2 var(--mono); }
.number-card em { display: block; margin-top: 8px; color: var(--danger); font-size: 11px; font-style: normal; }
.panel.notice { border-width: 2px; }
.panel.notice b { font-size: 15px; }
.panel.notice p { margin: 8px 0 0; color: var(--muted); font-size: 13px; }

.document { background: var(--paper); border: 1px solid var(--ink); box-shadow: var(--shadow); }
.doc-header { padding: 7mm 10mm 4mm; border-bottom: 2.4px solid var(--ink); }
.doc-header-top { display: flex; justify-content: space-between; align-items: flex-start; gap: 20px; }
.doc-label { display: inline-flex; padding: 1mm 1.5mm; background: var(--accent); color: var(--accent-ink); font: 800 7.5px/1 var(--mono); }
.doc-header h1 { margin: 2.5mm 0 1.5mm; font-size: 17px; line-height: 1.15; letter-spacing: -.025em; }
.doc-note { margin: 0; color: var(--muted); max-width: 165mm; font-size: 9px; line-height: 1.5; }
.doc-actions { display: flex; flex-wrap: wrap; gap: 6px; justify-content: flex-end; }
.doc-action.complete.done { background: var(--good); border-color: var(--good); color: white; }
.doc-meta { display: flex; flex-wrap: wrap; gap: 1mm 3mm; margin-top: 3mm; padding-top: 2mm; border-top: 1px solid var(--hair); color: var(--muted); font: 7.2px/1.4 var(--mono); }
.doc-meta code { color: var(--ink); word-break: break-all; }

.doc-body { max-width: var(--reading); margin: 0 auto; padding: 4mm 10mm 11mm; font-size: calc(8.6px * var(--font-scale)); line-height: 1.5; }
.doc-body h1, .doc-body h2, .doc-body h3, .doc-body h4 { position: relative; line-height: 1.3; letter-spacing: -.012em; }
.topic-section { margin: 4mm 0 0; }
.topic-section-root { margin-top: 0; }
.topic-section > h2 { margin: 0 0 2.6mm; padding: 2mm 2.6mm; background: var(--accent); color: var(--accent-ink); font: 700 11px/1.3 var(--mono); print-color-adjust: exact; -webkit-print-color-adjust: exact; }
.topic-grid { display: grid; grid-template-columns: minmax(0, 1fr); gap: 3.4mm; }
.topic-card { min-width: 0; padding: 2.6mm 3mm; border: 1.4px solid var(--ink); break-inside: avoid; page-break-inside: avoid; }
.topic-card > :first-child { margin-top: 0 !important; }
.topic-card > :last-child { margin-bottom: 0 !important; }
.topic-card h1 { margin: 0 0 2.4mm; padding-bottom: 1.8mm; border-bottom: 2px solid var(--ink); font-size: 17px; }
.topic-card h2, .topic-card h3 { margin: 0 0 1.8mm; padding-bottom: 1.6mm; border-bottom: 1.2px solid var(--ink); background: transparent; color: var(--ink); font-family: var(--kr); font-size: 10.5px; font-weight: 750; }
.topic-card h4 { margin: 2.8mm 0 1.2mm; font-size: 9.4px; }
.topic-overview { border-style: double; }
.heading-anchor { position: absolute; right: calc(100% + 1.4mm); color: var(--hair-strong); text-decoration: none; opacity: 0; font: 700 7px var(--mono); }
h1:hover > .heading-anchor, h2:hover > .heading-anchor, h3:hover > .heading-anchor, h4:hover > .heading-anchor { opacity: 1; }
.topic-section > h2 .heading-anchor { right: auto; left: -2mm; color: var(--muted); }
.doc-body p, .doc-body li { word-break: keep-all; overflow-wrap: anywhere; }
.doc-body p { margin: 1.3mm 0; }
.doc-body ul, .doc-body ol { padding-left: 1.55em; }
.doc-body li + li { margin-top: .8mm; }
.doc-body hr { margin: 3mm 0; border: 0; border-top: 1px solid var(--hair-strong); }
.doc-body blockquote { margin: 1.6mm 0; padding: 1.8mm 2.2mm; border-left: 2px solid var(--ink); background: var(--paper-soft); }
.doc-body blockquote > :first-child { margin-top: 0; }
.doc-body blockquote > :last-child { margin-bottom: 0; }
.doc-body code { padding: .1em .34em; background: var(--paper-soft); border: 1px solid var(--hair); font: .88em/1.5 var(--mono); word-break: break-word; }
.code-block { position: relative; margin: 1.8mm 0; border: 1px solid var(--hair-strong); background: var(--paper-soft); overflow: hidden; }
.code-block figcaption { padding: 1mm 1.5mm; border-bottom: 1px solid var(--hair); color: var(--muted); font: 800 7px/1.2 var(--mono); letter-spacing: .08em; }
.code-block pre { margin: 0; padding: 2mm; overflow: auto; }
.code-block code { padding: 0; border: 0; background: transparent; font: calc(7.8px * var(--font-scale))/1.5 var(--mono); white-space: pre; word-break: normal; }
.mermaid-source::after { content: "DIAGRAM SOURCE"; position: absolute; right: 9px; top: 7px; color: var(--warn); font: 800 8px var(--mono); }
.doc-body table { display: table; width: 100%; margin: 1.8mm 0; border-collapse: collapse; font-size: 8px; }
.doc-body th, .doc-body td { padding: 1.2mm 1.4mm; border: 1px solid var(--hair); vertical-align: top; text-align: left; }
.doc-body th { background: var(--accent); color: var(--accent-ink); font-size: .86em; }
.doc-body tbody tr:nth-child(even) { background: color-mix(in srgb, var(--paper-soft) 62%, transparent); }
.doc-body img { max-width: 100%; height: auto; border: 1px solid var(--hair); }
.doc-body input[type="checkbox"] { width: 1em; height: 1em; accent-color: var(--good); }
.doc-body strong { font-weight: 800; }
.doc-body mark { background: #ffe75c; color: #111; }

.doc-footer { padding: 3mm 10mm 4mm; border-top: 1px solid var(--hair-strong); display: flex; justify-content: space-between; gap: 12px; color: var(--muted); font: 7px/1.5 var(--mono); }
.floating { position: fixed; z-index: 45; right: 17px; bottom: 17px; display: grid; gap: 7px; }
.floating button { width: 42px; height: 42px; border: 1px solid var(--hair-strong); background: var(--paper); color: var(--ink); cursor: pointer; font: 800 11px var(--mono); box-shadow: 0 5px 18px rgba(0,0,0,.12); }
.floating button:hover { background: var(--accent); color: var(--accent-ink); }
.backdrop { display: none; }

@media (max-width: 1050px) {
  :root { --sidebar: 300px; }
  .metric-grid { grid-template-columns: repeat(2, minmax(0, 1fr)); }
  .route-grid { grid-template-columns: repeat(3, minmax(0, 1fr)); }
  .number-grid { grid-template-columns: repeat(2, minmax(0, 1fr)); }
}

@media (max-width: 780px) {
  .topbar { grid-template-columns: auto 1fr auto; gap: 8px; padding: 8px; }
  .brand-copy { display: none; }
  .menu-toggle { display: inline-block; }
  .toolbar .font-btn, .toolbar .print-btn { display: none; }
  .search-kbd { display: none; }
  .search-wrap input { padding-right: 12px; }
  .sidebar { transform: translateX(-102%); transition: transform .2s ease; width: min(90vw, 360px); box-shadow: var(--shadow); }
  body.nav-open .sidebar { transform: translateX(0); }
  .backdrop { position: fixed; z-index: 35; inset: var(--topbar) 0 0; background: rgba(0,0,0,.45); }
  body.nav-open .backdrop { display: block; }
  .shell { padding-left: 0; }
  .main { padding: 15px 10px 74px; }
  .hero { padding: 20px 16px; }
  .hero h1 { font-size: 24px; }
  .metric-grid, .route-grid, .number-grid, .group-grid { grid-template-columns: 1fr; }
  .doc-header { padding: 18px 14px 14px; }
  .doc-header-top { display: block; }
  .doc-actions { justify-content: flex-start; margin-top: 12px; }
  .doc-body { padding: 14px; font-size: calc(10.5px * var(--font-scale)); }
  .topic-section { margin-top: 14px; }
  .topic-section > h2 { margin-bottom: 9px; padding: 7px 9px; font-size: 12px; }
  .topic-grid { gap: 10px; }
  .topic-card { padding: 10px 11px; }
  .topic-card h1 { font-size: 18px; }
  .topic-card h2, .topic-card h3 { font-size: 12px; }
  .topic-card h4 { font-size: 11px; }
  .doc-body table { display: block; overflow-x: auto; }
  .heading-anchor { display: none; }
  .doc-footer { display: block; }
}

@media print {
  :root { color-scheme: light; --font-scale: 1; --ink: #111; --muted: #555; --hair: #ccc; --hair-strong: #777; --paper: #fff; --paper-soft: #f3f3f3; --accent: #111; --accent-ink: #fff; }
  @page { size: A4; margin: 11mm 10mm; }
  body { background: #fff; font-size: 8.6px; line-height: 1.5; }
  .topbar, .sidebar, .floating, .backdrop, .home, .doc-actions { display: none !important; }
  .shell { padding: 0; }
  .main { padding: 0; }
  .document { display: none !important; max-width: none; border: 0; box-shadow: none; }
  body[data-print-mode="all"] .document,
  body[data-print-mode="all"] .document[hidden],
  body:not([data-print-mode]) .document,
  body:not([data-print-mode]) .document[hidden] { display: block !important; }
  body[data-print-mode="one"] .document.active-document { display: block !important; }
  body[data-print-mode="all"] .document,
  body:not([data-print-mode]) .document { break-after: page; page-break-after: always; }
  body[data-print-mode="all"] .document:last-of-type,
  body:not([data-print-mode]) .document:last-of-type { break-after: auto; page-break-after: auto; }
  .doc-header { padding: 0 0 4mm; }
  .doc-label { padding: 1mm 1.5mm; font-size: 7.5px; }
  .doc-header h1 { margin: 2.5mm 0 1.5mm; font-size: 17px; line-height: 1.12; }
  .doc-note { font-size: 8.6px; line-height: 1.5; }
  .doc-meta { gap: 1mm 3mm; margin-top: 3mm; padding-top: 2mm; font-size: 7.2px; line-height: 1.4; }
  .doc-body { max-width: none; padding: 4mm 0 0; font-size: 8.6px; line-height: 1.5; }
  .topic-section { margin-top: 4mm; }
  .topic-section > h2 { margin: 0 0 2.6mm; padding: 2mm 2.6mm; font-size: 11px; }
  .topic-grid { gap: 3.4mm; }
  .topic-card { padding: 2.6mm 3mm; }
  .topic-card h1 { margin: 0 0 2.4mm; padding-bottom: 1.8mm; font-size: 17px; }
  .topic-card h2, .topic-card h3 { margin: 0 0 1.8mm; padding-bottom: 1.6mm; font-size: 10.5px; }
  .topic-card h4 { margin: 2.8mm 0 1.2mm; font-size: 9.4px; }
  .doc-body p, .doc-body ul, .doc-body ol, .doc-body blockquote { margin-top: 1.3mm; margin-bottom: 1.3mm; }
  .doc-body table { font-size: 8px; }
  .doc-body th, .doc-body td { padding: 1.2mm 1.4mm; }
  .code-block figcaption { padding: 1mm 1.5mm; font-size: 7px; }
  .code-block pre { padding: 2mm; }
  .code-block code { font-size: 7.8px; line-height: 1.45; }
  .doc-body h1, .doc-body h2, .doc-body h3, .topic-section > h2 { break-after: avoid; }
  .topic-card, .doc-body table, .doc-body blockquote, .code-block { break-inside: avoid; page-break-inside: avoid; }
  .doc-footer { padding: 4mm 0 0; font-size: 7px; }
  a { text-decoration: none; }
}
`;

const SCRIPTS = String.raw`
(() => {
  const config = JSON.parse(document.getElementById('portal-config').textContent);
  const docElements = [...document.querySelectorAll('.document')];
  const navButtons = [...document.querySelectorAll('[data-doc-id]')];
  const home = document.getElementById('home');
  const search = document.getElementById('globalSearch');
  const results = document.getElementById('searchResults');
  const sectionToc = document.getElementById('sectionToc');
  const progressKey = 'winters-engine-study-progress-v1';
  const prefsKey = 'winters-engine-study-prefs-v1';
  let activeDocId = null;
  let progress = loadJson(progressKey, {});
  let prefs = loadJson(prefsKey, { theme: 'paper', font: 1 });

  function loadJson(key, fallback) {
    try { return JSON.parse(localStorage.getItem(key)) || fallback; }
    catch (_) { return fallback; }
  }

  function saveJson(key, value) {
    try { localStorage.setItem(key, JSON.stringify(value)); }
    catch (_) { /* file:// private mode can reject storage */ }
  }

  function applyPrefs() {
    document.documentElement.dataset.theme = prefs.theme === 'dark' ? 'dark' : 'paper';
    document.documentElement.style.setProperty('--font-scale', String(prefs.font || 1));
    const theme = document.getElementById('themeToggle');
    if (theme) theme.textContent = prefs.theme === 'dark' ? 'LIGHT' : 'DARK';
  }

  function updateProgressUi() {
    const done = config.docs.filter((doc) => progress[doc.id]).length;
    const total = config.docs.length;
    document.querySelectorAll('[data-progress-count]').forEach((el) => { el.textContent = done + ' / ' + total; });
    document.querySelectorAll('[data-progress-value]').forEach((el) => { el.style.width = (total ? done / total * 100 : 0) + '%'; });
    navButtons.forEach((button) => button.classList.toggle('done', Boolean(progress[button.dataset.docId])));
    docElements.forEach((article) => {
      const button = article.querySelector('.complete');
      const isDone = Boolean(progress[article.dataset.doc]);
      if (button) {
        button.classList.toggle('done', isDone);
        button.textContent = isDone ? '완료됨 ✓' : '학습 완료';
      }
    });
  }

  function closeNav() { document.body.classList.remove('nav-open'); }

  function showHome(push = true) {
    activeDocId = null;
    home.hidden = false;
    docElements.forEach((doc) => { doc.hidden = true; doc.classList.remove('active-document'); });
    navButtons.forEach((button) => button.classList.toggle('active', button.dataset.docId === 'home'));
    sectionToc.innerHTML = '<p class="sidebar-kicker">문서를 열면 이곳에 내부 목차가 표시됩니다.</p>';
    if (push) history.replaceState(null, '', '#home');
    document.title = config.siteTitle;
    window.scrollTo({ top: 0, behavior: 'instant' });
    closeNav();
  }

  function buildToc(article) {
    const headings = [...article.querySelectorAll('.doc-body h2, .doc-body h3')];
    sectionToc.innerHTML = headings.length
      ? headings.map((heading) => '<a class="toc-h' + heading.tagName.slice(1) + '" href="#' + heading.id + '">' + heading.textContent.replace(/^#/, '') + '</a>').join('')
      : '<p class="sidebar-kicker">이 문서에는 2·3단계 제목이 없습니다.</p>';
  }

  function showDoc(id, anchor = '', push = true) {
    const article = document.querySelector('.document[data-doc="' + CSS.escape(id) + '"]');
    if (!article) return showHome(push);
    activeDocId = id;
    home.hidden = true;
    docElements.forEach((doc) => {
      const active = doc === article;
      doc.hidden = !active;
      doc.classList.toggle('active-document', active);
    });
    navButtons.forEach((button) => button.classList.toggle('active', button.dataset.docId === id));
    const meta = config.docs.find((doc) => doc.id === id);
    document.title = meta.title + ' · ' + config.siteTitle;
    buildToc(article);
    if (push) history.replaceState(null, '', anchor ? '#' + anchor : '#doc-' + id);
    requestAnimationFrame(() => {
      const target = anchor ? document.getElementById(anchor) : article;
      if (target) target.scrollIntoView({ block: 'start', behavior: 'instant' });
    });
    closeNav();
  }

  function navigateHash() {
    const hash = decodeURIComponent(location.hash.slice(1));
    if (!hash || hash === 'home') return showHome(false);
    const matched = config.docs.find((doc) => hash === 'doc-' + doc.id || hash.startsWith('doc-' + doc.id + '--'));
    if (matched) showDoc(matched.id, hash, false);
    else showHome(false);
  }

  function setFont(delta) {
    prefs.font = Math.max(.84, Math.min(1.28, Math.round(((prefs.font || 1) + delta) * 100) / 100));
    saveJson(prefsKey, prefs);
    applyPrefs();
  }

  function printWithMode(mode) {
    if (mode === 'one' && !activeDocId) return;
    document.body.dataset.printMode = mode;
    requestAnimationFrame(() => window.print());
  }

  function runSearch(query) {
    const q = query.trim().toLocaleLowerCase('ko');
    if (q.length < 2) {
      results.classList.remove('open');
      results.innerHTML = '';
      return;
    }
    const matches = [];
    for (const article of docElements) {
      const body = article.querySelector('.doc-body');
      const text = body.textContent || '';
      const lower = text.toLocaleLowerCase('ko');
      const index = lower.indexOf(q);
      if (index < 0) continue;
      const doc = config.docs.find((item) => item.id === article.dataset.doc);
      const before = Math.max(0, index - 48);
      const after = Math.min(text.length, index + q.length + 84);
      const snippet = (before ? '…' : '') + text.slice(before, after).replace(/\s+/g, ' ') + (after < text.length ? '…' : '');
      matches.push({ doc, snippet });
      if (matches.length >= 50) break;
    }
    results.innerHTML = matches.length
      ? matches.map(({ doc, snippet }) => '<button class="search-result" data-search-doc="' + doc.id + '"><b>' + doc.label + ' · ' + doc.title + '</b><span>' + escapeForHtml(snippet) + '</span></button>').join('')
      : '<div class="search-empty">일치하는 문서가 없습니다. 숫자·파일명·도메인 이름으로 다시 검색해 보세요.</div>';
    results.classList.add('open');
  }

  function escapeForHtml(value) {
    return String(value).replace(/[&<>"']/g, (ch) => ({ '&':'&amp;', '<':'&lt;', '>':'&gt;', '"':'&quot;', "'":'&#39;' }[ch]));
  }

  document.addEventListener('click', (event) => {
    const nav = event.target.closest('[data-doc-id]');
    if (nav) {
      const id = nav.dataset.docId;
      id === 'home' ? showHome() : showDoc(id);
      return;
    }
    const searchResult = event.target.closest('[data-search-doc]');
    if (searchResult) {
      showDoc(searchResult.dataset.searchDoc);
      results.classList.remove('open');
      search.value = '';
      return;
    }
    const docLink = event.target.closest('a[data-doc-link]');
    if (docLink) {
      event.preventDefault();
      const hash = docLink.getAttribute('href').slice(1);
      showDoc(docLink.dataset.docLink, hash);
      return;
    }
    const printOne = event.target.closest('.print-one');
    if (printOne) {
      printWithMode('one');
      return;
    }
    const complete = event.target.closest('.complete');
    if (complete) {
      const id = complete.closest('.document').dataset.doc;
      progress[id] = !progress[id];
      saveJson(progressKey, progress);
      updateProgressUi();
      return;
    }
    if (!event.target.closest('.search-wrap')) results.classList.remove('open');
  });

  document.getElementById('menuToggle').addEventListener('click', () => document.body.classList.toggle('nav-open'));
  document.getElementById('backdrop').addEventListener('click', closeNav);
  document.getElementById('themeToggle').addEventListener('click', () => {
    prefs.theme = prefs.theme === 'dark' ? 'paper' : 'dark';
    saveJson(prefsKey, prefs);
    applyPrefs();
  });
  document.getElementById('fontDown').addEventListener('click', () => setFont(-.08));
  document.getElementById('fontUp').addEventListener('click', () => setFont(.08));
  document.getElementById('printAll').addEventListener('click', () => printWithMode('all'));
  document.getElementById('scrollTop').addEventListener('click', () => window.scrollTo({ top: 0, behavior: 'smooth' }));
  document.getElementById('prevDoc').addEventListener('click', () => {
    const index = config.docs.findIndex((doc) => doc.id === activeDocId);
    if (index > 0) showDoc(config.docs[index - 1].id);
  });
  document.getElementById('nextDoc').addEventListener('click', () => {
    const index = config.docs.findIndex((doc) => doc.id === activeDocId);
    if (index >= 0 && index < config.docs.length - 1) showDoc(config.docs[index + 1].id);
  });
  search.addEventListener('input', () => runSearch(search.value));
  search.addEventListener('keydown', (event) => {
    if (event.key === 'Escape') { search.value = ''; results.classList.remove('open'); search.blur(); }
    if (event.key === 'Enter') {
      const first = results.querySelector('[data-search-doc]');
      if (first) first.click();
    }
  });
  window.addEventListener('hashchange', navigateHash);
  window.addEventListener('afterprint', () => { delete document.body.dataset.printMode; });
  document.addEventListener('keydown', (event) => {
    if (event.key === '/' && document.activeElement !== search) { event.preventDefault(); search.focus(); }
    if (event.key === '[' && document.activeElement.tagName !== 'INPUT') document.getElementById('prevDoc').click();
    if (event.key === ']' && document.activeElement.tagName !== 'INPUT') document.getElementById('nextDoc').click();
  });

  applyPrefs();
  updateProgressUi();
  navigateHash();
})();
`;

function makeSidebar(groups, singleMode) {
  const groupHtml = groups.map((group, groupIndex) => `
    <details class="nav-group" ${groupIndex < 3 || singleMode ? 'open' : ''}>
      <summary>${escapeHtml(group.name)} · ${group.docs.length}</summary>
      <div class="nav-list">
        ${group.docs.map((doc) => `
          <button class="doc-nav" type="button" data-doc-id="${escapeAttribute(doc.id)}">
            <span class="doc-code">${escapeHtml(doc.label)}</span>
            <span class="doc-name">${escapeHtml(doc.title)}</span>
            <span class="doc-state" aria-hidden="true"></span>
          </button>`).join('')}
      </div>
    </details>`).join('');

  return `
    <aside class="sidebar" id="sidebar" aria-label="학습 문서 탐색">
      <section class="sidebar-section">
        <button class="home-link" type="button" data-doc-id="home">
          <span class="doc-code">HOME</span><span class="doc-name">학습 대시보드</span><span class="doc-state"></span>
        </button>
        <div class="progress-track" aria-label="학습 진척도"><span class="progress-value" data-progress-value></span></div>
        <div class="progress-copy"><span>LOCAL PROGRESS</span><span data-progress-count>0 / 0</span></div>
      </section>
      ${groupHtml}
      <section class="section-toc" id="sectionToc"><p class="sidebar-kicker">문서를 열면 이곳에 내부 목차가 표시됩니다.</p></section>
    </aside>`;
}

function makeHome(docs, groups, singleMode) {
  const totalBytes = docs.reduce((sum, doc) => sum + doc.bytes, 0);
  const totalLines = docs.reduce((sum, doc) => sum + doc.lines, 0);
  const totalWords = docs.reduce((sum, doc) => sum + doc.words, 0);
  const firstDoc = docs[0];
  const route = [
    ['01', '쉬운 말', '쉬운설명서에서 개념의 모양부터 잡는다.'],
    ['02', '실제 흐름', 'CONQUEST에서 Winters 코드와 owner를 연결한다.'],
    ['03', '수치와 경계', '분모·날짜·빌드 범위를 숫자와 함께 외운다.'],
    ['04', '구두 검증', '26문항을 본질·흐름·수치·경계로 답한다.'],
    ['05', '이력서 환전', '직무별 4불릿과 War Story로 압축한다.'],
  ];
  const numbers = [
    ['33.333ms', '30Hz Server Tick 예산', 'Client Frame 예산과 섞지 않기'],
    ['0.723→0.177ms', 'Lazy Pose Update median -75.5%', '전체 Frame/FPS 개선률 아님'],
    ['0.0675ms', 'Client Scheduler median', '개별 queue 비용 측정값 아님'],
    ['+25.4%', '합성 pure-worker jobs/s', 'LoL FPS 개선률 아님'],
    ['3.440/3.551ms', '10-bot 54,000-tick p99', 'historical v4 accelerated headless'],
    ['19,808B / 18개', '과거 Snapshot p95 / datagrams', '2026-07-13 replay v1 표본'],
    ['128 fields', '현재 EntitySnapshot schema', 'deltaBaseTick=0 full Snapshot'],
    ['48.1× / 3.15×', 'geometry-only / cooked set', 'texture 제외, 분모 다름'],
    ['5cm / 3초', 'Retreat progress / Recall 복구', '보존 A/B threshold, 최적값 아님'],
  ];

  return `
    <section class="home" id="home">
      <div class="hero">
        <div class="eyebrow">SOURCE-AUDITED EDITION · 2026-07-19 · LOCAL STUDY PORTAL</div>
        <h1>${singleMode ? 'Winters Engine<br>쉬운설명서' : 'Winters Engine<br>정복 학습 포털'}</h1>
        <p>${singleMode
          ? '쉬운 말로 본질을 잡고, 실제 프로젝트 흐름과 수치, 과장하면 안 되는 경계, 이력서 문장까지 한 권으로 연결합니다.'
          : '쉬운 설명부터 13개 기술 도메인, 면접 답안, 이력서 문장까지 흩어진 모든 정리본을 한 곳에서 검색하고 학습합니다.'}</p>
        <div class="hero-actions">
          <button class="route-btn primary" type="button" data-doc-id="${escapeAttribute(firstDoc.id)}">첫 문서부터 읽기</button>
          ${docs.some((doc) => doc.id === 'oral-scorecard') ? '<button class="route-btn" type="button" data-doc-id="oral-scorecard">26문항 시험 열기</button>' : ''}
          ${docs.some((doc) => doc.id === 'resume-bank') ? '<button class="route-btn" type="button" data-doc-id="resume-bank">이력서 문장 보기</button>' : ''}
        </div>
        <div class="hero-rule">CONCEPT → CODE FLOW → MEASUREMENT → BOUNDARY → RESUME → ORAL DEFENSE</div>
      </div>

      <div class="metric-grid">
        <div class="metric"><strong>${docs.length}</strong><span>EMBEDDED DOCUMENTS</span></div>
        <div class="metric"><strong>${totalLines.toLocaleString('ko-KR')}</strong><span>SOURCE LINES</span></div>
        <div class="metric"><strong>${totalWords.toLocaleString('ko-KR')}</strong><span>APPROX. WORDS</span></div>
        <div class="metric"><strong>${(totalBytes / 1024 / 1024).toFixed(2)}MB</strong><span>MARKDOWN SOURCE</span></div>
      </div>

      <div class="panel notice">
        <b>읽기 우선순위</b>
        <p>현재 사실과 최신 수치는 <code>CONQUEST 01~13</code>을 우선합니다. <code>REFERENCE 심화 각론</code>은 더 깊은 배경 설명이며, 같은 내용이 다르면 최신 정복 문서의 정정을 따릅니다.</p>
      </div>

      <div class="section-head"><h2>학습 루프</h2><span>이해를 면접과 이력서로 환전하는 순서</span></div>
      <div class="route-grid">
        ${route.map(([index, title, copy]) => `<div class="route-card"><span class="route-index">${index}</span><b>${title}</b><small>${copy}</small></div>`).join('')}
      </div>

      ${singleMode ? '' : `
      <div class="section-head"><h2>문서 묶음</h2><span>왼쪽 탐색에서도 같은 분류로 열 수 있습니다.</span></div>
      <div class="group-grid">
        ${groups.map((group) => `<div class="group-card"><b>${escapeHtml(group.name)}</b><small>${group.docs.length}개 문서 · ${group.docs.map((doc) => escapeHtml(doc.label)).join(' / ')}</small></div>`).join('')}
      </div>`}

      <div class="section-head"><h2>핵심 숫자 덱</h2><span>숫자와 과장 금지선을 한 쌍으로 외우기</span></div>
      <div class="number-grid">
        ${numbers.map(([number, meaning, boundary]) => `<div class="number-card"><b>${number}</b><small>${meaning}</small><em>${boundary}</em></div>`).join('')}
      </div>
    </section>`;
}

function makeDocument(doc, rendered) {
  const sourceHref = encodeURI(path.relative(OUTPUT_DIR, doc.abs).replaceAll('\\', '/'));
  return `
    <article class="document" data-doc="${escapeAttribute(doc.id)}" id="doc-${escapeAttribute(doc.id)}" hidden>
      <header class="doc-header">
        <div class="doc-header-top">
          <div>
            <span class="doc-label">${escapeHtml(doc.label)} · ${escapeHtml(doc.group)}</span>
            <h1>${escapeHtml(doc.title)}</h1>
            <p class="doc-note">${escapeHtml(doc.note)}</p>
          </div>
          <div class="doc-actions">
            <button class="doc-action print-one" type="button">이 문서만 인쇄</button>
            <button class="doc-action complete" type="button">학습 완료</button>
            <a class="doc-action" href="${escapeAttribute(sourceHref)}">원문 MD</a>
          </div>
        </div>
        <div class="doc-meta">
          <span>LINES <b>${doc.lines.toLocaleString('ko-KR')}</b></span>
          <span>BYTES <b>${doc.bytes.toLocaleString('ko-KR')}</b></span>
          <span>SHA-256 <code>${doc.sha}</code></span>
          <span>SOURCE <code>${escapeHtml(doc.sourcePath)}</code></span>
        </div>
      </header>
      <div class="doc-body">${rendered.html}</div>
      <footer class="doc-footer"><span>원문 SHA-256 · ${doc.sha}</span><span>Rendered locally from Markdown · 2026-07-19</span></footer>
    </article>`;
}

function makeHtml(docs, options) {
  const docsByPath = new Map(docs.map((doc) => [normalizePath(doc.abs), doc]));
  const rendered = new Map(docs.map((doc) => [doc.id, createMarkdownRenderer(doc, docsByPath)]));
  const groups = groupDocs(docs);
  const config = {
    siteTitle: options.siteTitle,
    mode: options.mode,
    docs: docs.map((doc) => ({ id: doc.id, label: doc.label, title: doc.title, group: doc.group, sha: doc.sha })),
  };

  return `<!doctype html>
<html lang="ko" data-theme="paper">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta name="description" content="Winters Engine 기술 도메인 학습·면접·이력서 통합 HTML">
  <title>${escapeHtml(options.siteTitle)}</title>
  <style>${STYLES}</style>
</head>
<body>
  <header class="topbar">
    <div class="brand">
      <button class="menu-toggle" id="menuToggle" type="button" aria-label="문서 메뉴 열기">MENU</button>
      <span class="brand-mark">W/E</span>
      <span class="brand-copy"><strong>${escapeHtml(options.siteTitle)}</strong><small>CODE · MEASURE · EXPLAIN</small></span>
    </div>
    <div class="search-wrap">
      <span class="search-icon">⌕</span>
      <input id="globalSearch" type="search" autocomplete="off" placeholder="전체 본문 검색 · 예: JobSystem, 0.0675ms, Snapshot">
      <span class="search-kbd">/</span>
      <div class="search-results" id="searchResults" role="listbox"></div>
    </div>
    <div class="toolbar">
      <button class="tool-btn font-btn" id="fontDown" type="button" title="글자 작게">A−</button>
      <button class="tool-btn font-btn" id="fontUp" type="button" title="글자 크게">A+</button>
      <button class="tool-btn" id="themeToggle" type="button" title="테마 전환">DARK</button>
      <button class="tool-btn print-btn" id="printAll" type="button" title="${docs.length}개 문서 전체 인쇄">PRINT ALL</button>
    </div>
  </header>
  ${makeSidebar(groups, options.singleMode)}
  <div class="backdrop" id="backdrop"></div>
  <div class="shell"><main class="main">
    ${makeHome(docs, groups, options.singleMode)}
    ${docs.map((doc) => makeDocument(doc, rendered.get(doc.id))).join('\n')}
  </main></div>
  <div class="floating" aria-label="문서 이동">
    <button type="button" id="prevDoc" title="이전 문서 · [">PREV</button>
    <button type="button" id="nextDoc" title="다음 문서 · ]">NEXT</button>
    <button type="button" id="scrollTop" title="맨 위로">TOP</button>
  </div>
  <script id="portal-config" type="application/json">${safeJson(config)}</script>
  <script>${SCRIPTS}</script>
</body>
</html>`;
}

function writeOutput(filename, html) {
  fs.mkdirSync(OUTPUT_DIR, { recursive: true });
  const target = path.join(OUTPUT_DIR, filename);
  fs.writeFileSync(target, html, 'utf8');
  return target;
}

function main() {
  const portalDocs = loadDocs(portalManifest);
  const easyDocs = portalDocs.filter((doc) => doc.id === 'easy-guide');

  const easyHtml = makeHtml(easyDocs, {
    mode: 'easy-card-comparison',
    singleMode: true,
    siteTitle: 'Winters Engine · 쉬운설명서 · 카드형 비교본',
  });
  const portalHtml = makeHtml(portalDocs, {
    mode: 'portal-card-comparison',
    singleMode: false,
    siteTitle: 'Winters Engine · 통합 학습 포털 · 카드형 비교본',
  });

  const outputs = [
    writeOutput('2026-07-19_Winters_Engine_쉬운설명서_카드형_비교본.html', easyHtml),
    writeOutput('2026-07-19_Winters_Engine_통합학습포털_카드형_비교본.html', portalHtml),
  ];

  const report = {
    generatedAt: new Date().toISOString(),
    sourceDocuments: portalDocs.length,
    sourceBytes: portalDocs.reduce((sum, doc) => sum + doc.bytes, 0),
    sourceLines: portalDocs.reduce((sum, doc) => sum + doc.lines, 0),
    outputs: outputs.map((target) => ({
      path: path.relative(ROOT, target).replaceAll('\\', '/'),
      bytes: fs.statSync(target).size,
      sha256: sha256(fs.readFileSync(target, 'utf8')),
    })),
    sources: portalDocs.map((doc) => ({
      id: doc.id,
      path: doc.sourcePath.replaceAll('\\', '/'),
      bytes: doc.bytes,
      lines: doc.lines,
      sha256: doc.sha,
    })),
  };
  const reportPath = path.join(OUTPUT_DIR, '2026-07-19_Winters_Engine_CARD_HTML_BUILD_REPORT.json');
  fs.writeFileSync(reportPath, `${JSON.stringify(report, null, 2)}\n`, 'utf8');

  process.stdout.write(`${JSON.stringify(report, null, 2)}\n`);
}

main();
