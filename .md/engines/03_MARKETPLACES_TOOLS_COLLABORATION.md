# 마켓플레이스·툴 생태계·협업 — Fab과 Unity Asset Store의 원리와 전략

작성일: 2026-07-10. Fab과 Unity Asset Store의 원리·본질, 툴 개발자 관점의 규칙 전체, 협업 구조와 업데이트 사이클을 고정하는 참조 문서다. Winters의 자산 수용 방향은 `.md/architecture/WINTERS_UE_FAB_TOOL_ADOPTION.md`를, 툴 면접 대비는 `.md/interview/tool-development.md`를 따르며, 이 문서는 그 두 문서의 "시장 쪽 절반"을 채운다. 웹 검증 사실은 출처 도메인을 괄호로 표기하고, 일반 공학 지식은 (일반 지식), 검증 실패 항목은 (미검증)으로 구분한다.

---

## §1 마켓플레이스의 본질 — 원료 공급망이자 유통 채널

### 1.1 이중 관점

마켓플레이스는 Winters 입장에서 두 개의 서로 다른 물건이다.

| 관점 | 역할 | Winters에서의 실체 |
|---|---|---|
| 소비자 (팀) | 외부 자산 공급망 — 메시/애님/텍스처/오디오라는 "원료" | `.md/architecture/WINTERS_UE_FAB_TOOL_ADOPTION.md` §3의 ingestion 파이프라인 (§5에서 상술) |
| 판매자 (개인) | 툴 개발자의 유통 채널 — 어빌리티 타임라인 에디터의 출구 | `.md/interview/tool-development.md` §12의 로드맵 "에디터 코어 → 타임라인 툴 → UE 이식 → Fab 출시" |

같은 스토어를 쓰더라도 두 관점의 규율은 완전히 다르다. 소비자 관점의 핵심은 "무엇을 사도 되는가"와 "산 것을 어떻게 검증된 계약으로만 게임에 넣는가"이고, 판매자 관점의 핵심은 "심사·버전·업데이트라는 계약을 지속적으로 이행할 수 있는가"다. 이 문서는 §2~§4에서 판매자 규칙을, §5에서 소비자 규율을, §6에서 판매 전략을 다룬다.

### 1.2 왜 엔진 회사가 마켓을 운영하는가

(일반 지식) 엔진 회사가 마켓을 직접 운영하는 이유는 수수료 수익이 아니라 생태계 락인과 콘텐츠 플라이휠이다.

- **콘텐츠 플라이휠**: 자산이 많을수록 개발자가 그 엔진을 선택하고, 개발자가 많을수록 구매자가 늘고, 구매자가 늘수록 판매자가 몰려 자산이 더 많아진다. 스토어는 이 순환의 축이며, 엔진 전환 비용(이미 구매한 자산·플러그인이 다른 엔진에서 무용지물)을 키우는 락인 장치다.
- **수수료 구조가 전략을 드러낸다**: Epic은 88/12(dev.epicgames.com), Unity는 70/30(assetstore.unity.com)이다. Epic의 낮은 수수료는 스토어 자체 수익보다 UE 생태계 확장(그리고 앱스토어 30% 관행에 대한 공세)을 우선한다는 신호다. (일반 지식) 12%는 Epic Games Store의 게임 판매 수수료와 같은 숫자로, Epic의 일관된 플랫폼 전략이다.
- **품질 게이트로서의 심사**: 두 스토어 모두 인간 심사를 유지한다. 스토어의 신뢰가 무너지면 플라이휠 전체가 죽기 때문에, 심사는 판매자에게는 마찰이지만 플랫폼에게는 생존 조건이다.

### 1.3 두 스토어 한눈 비교

| 항목 | Fab (Epic) | Unity Asset Store |
|---|---|---|
| 출범/성격 | 2024-10 통합 출범, 멀티 엔진·멀티 포맷 (unrealengine.com) | 단일 엔진 전용, 에디터 내장 스토어 |
| 수익 배분 | 88/12 고정 (dev.epicgames.com) | 70/30 고정 (assetstore.unity.com) |
| 최소 가격 | 무료 또는 .99로 끝나는 가격 (dev.epicgames.com) | $4.99 (assetstore.unity.com) |
| 코드 제출 | 소스 제출 → Epic이 엔진 버전별 컴파일 (forums.unrealengine.com) | 패키지 그대로 배포 (빌드는 구매자 몫) |
| 신규 심사 | 공식 SLA 없음, 수일~수개월 보고 (forums.unrealengine.com) | 약 10영업일 (support.unity.com) |
| 업데이트 심사 | 대개 ~24시간 (커뮤니티 보고) | 약 2영업일 (support.unity.com) |
| 지급 | 월간, 최소 $100, 월말 후 ~30일 (dev.epicgames.com) | PayPal 월간 또는 은행 송금 분기($250 최소) (docs.unity3d.com) |
| 2025 시장 | 창작자 지급 $24M+, 리스팅 42만+ (unrealengine.com) | 카테고리별 공식 수치 미공개 |

---

## §2 Fab 상세

### 2.1 출범 배경과 구조

- Fab(fab.com)은 2024년 10월 22일 출범한 Epic의 단일 통합 마켓플레이스로, 기존 **Unreal Engine Marketplace + Sketchfab Store + Quixel Megascans + ArtStation Marketplace** 4개를 흡수·대체했다 (unrealengine.com, cgchannel.com).
- 서비스 대상은 UE 전용이 아니다: Unreal Engine, Unity, UEFN, DCC 툴(Blender/Maya/Substance 3D)용 포맷과 원시 자산 포맷을 모두 취급하며, UE와 UEFN에는 인-에디터 통합이 있다 (unrealengine.com).
- **멀티포맷 리스팅**: 하나의 상품 페이지에 여러 파일 포맷을 얹을 수 있다 — 공식 문서가 "Add new format... 각 파일마다 반복"이라고 안내하며, 구 마켓 이주 판매자에게도 "추가 포맷·라이선스 옵션·실시간 3D 프리뷰로 리스팅을 업그레이드하라"고 안내했다 (dev.epicgames.com). 즉 UE + Unity + 단독 다운로드를 한 페이지에서 팔 수 있다. 단, 이 기능이 실질적으로 이득인 것은 콘텐츠(모델/오디오)이고, 에디터 코드 툴은 엔진별 구현이 달라 §6.4의 이중 스토어 전략이 필요하다.

### 2.2 퍼블리셔 온보딩 절차

fab.com에서 셀프서브로 진행된다 (dev.epicgames.com):

1. Epic Games 계정으로 로그인 → 상단 **Publish** 클릭
2. **Fab Distribution Agreement** 동의
3. **Publisher Profile** 작성
4. 고유 **Creator Code** 생성 (퍼블리셔 URL로 예약되는 사용자명)
5. **Trader Verification** (EU DSA형 판매자 신원 검증)
6. 서드파티 결제사 **Hyperwallet**으로 지급 정보 설정 (support.fab.com), 지역별 세금 인터뷰

- 지급: 최소 임계 $100 USD, 매월 말 기준 약 30일 후 지급 (dev.epicgames.com).
- 퍼블리셔 프로필 자체가 Epic 심사를 거친다 — 포털 고지는 "최대 36시간"이지만 2025-01 포럼에는 72시간 초과 사례가 보고됐다 (forums.unrealengine.com). 리스팅 심사와는 별개의 게이트다.

### 2.3 코드 플러그인 — 소스 제출과 Epic 버전별 컴파일

Fab 코드 플러그인의 가장 특징적인 구조는 **판매자가 바이너리를 만들지 않는다**는 것이다.

- 제출물은 소스 zip이다. Epic 스태프가 명시했다: "Unreal Engine-generated Binaries and Intermediate files are not required" — Epic의 툴체인이 엔진 버전별 바이너리를 빌드해 Launcher로 배포한다 (forums.unrealengine.com).
- 기대 구조: `MyPlugin.uplugin` + `Source/<Module>/{Public,Private,*.Build.cs}` + `Config/` + `Content/` + `Resources/` + 선택적 `ThirdParty/`. Code Plugins는 최소 1개의 코드 모듈을 포함해야 하고, 반대로 "complete project" 카테고리 상품에는 C++ 코드가 있으면 안 된다(C++는 Code Plugins 카테고리로) (dev.epicgames.com).
- **엔진 버전 락 해제**: 구 UE Marketplace는 최신 3개 엔진 버전만 허용했으나, Fab에서는 Epic 스태프가 "최신 3개 버전에 락되지 않는다"고 확인했고 4.27까지 구버전 지원이 가능하다 (forums.unrealengine.com).
- **버전별 재제출 의무**: 공식 문서 — "Code Plugins: Always require an updated project to be submitted... You must upload a new .uplugin project to your listing per engine version." 엔진 신버전이 나올 때마다 해당 버전용 제출이 필요하다 (dev.epicgames.com).
- 제출 전 로컬 검증의 표준은 RunUAT다:

```text
Engine\Build\BatchFiles\RunUAT.bat BuildPlugin -Plugin=<path\to\MyPlugin.uplugin> -Package=<출력 디렉터리> -Rocket
```

지원하는 엔진 버전마다 이 명령을 돌려 zero-error를 확인한다. 커뮤니티 CI 노트에 따르면 Fab 심사 컴파일러는 clang 계열이라 MSVC가 놓치는 경고(`-Wshadow` 등)를 잡는다 — "에러 0 + 유의미한 경고 0"이 통과 조건이다 (github.com/MuddyTerrain, dev.epicgames.com).

- 기타 규칙: zip 15GB 상한(초과 시 Fab 팀 검토), 추가 파일 최대 3개 각 6GB; 문서는 "thorough documentation" 의무(형식 미지정); Blueprint는 에러/유의미 경고 없이 깨끗해야 하고 튜토리얼 외 loose node 금지; 외부 라이선스/구독으로 잠긴 유료 기능은 base 상품 단독 기능이 충분할 때만 허용; Python 코드는 `Content/Python`에 배치 (dev.epicgames.com).

### 2.4 수익 배분·라이선스·가격 규칙

- **88/12 고정**: "Publishers receive 88% of the revenue generated by your product sales." 볼륨 티어 없음 (dev.epicgames.com). 구 마켓 이주 판매자에게 2024년 말까지 한시적 100% 혜택이 있었으나 종료됐다 (unrealengine.com). 실수령 참고치: $49.99 판매 1건이 결제 수수료·세금·환전 후 약 $40.50~42.80 (strayspark.studio, 2차 출처).
- **라이선스 3종**: Creative Commons Attribution(무료 전용), Standard(무료/유료), 이주 상품용 legacy UE Marketplace 라이선스. **Standard 유료 상품은 Personal/Professional 이중 티어가 의무다** — Personal은 최근 12개월 상업 총매출 $100,000 미만 구매자용, Professional은 그 이상. 두 티어 가격은 같아도 되고 달라도 된다 (dev.epicgames.com).
- **가격 규칙**: USD 기준, 모든 가격은 **.99로 끝나야 한다**($0.00 무료 제외). 증분 규칙: $100 미만 $1 단위, $100~150 $5 단위 등 구간별 스텝, $1,500 초과는 Fab Support 문의 (dev.epicgames.com). 2025년 중반부터 판매자 개인 세일/할인 운영 가능 (forums.unrealengine.com).
- NoAI 플래그를 켠 자산은 CC 라이선스를 쓸 수 없고 Standard만 가능하다 (dev.epicgames.com).

### 2.5 심사 프로세스와 2025-26의 현실

- 상태 흐름: `Draft → Pending approval → (Changes needed | Approved) → Pending Publication → Live` 또는 `Declined`. 결과는 이메일 통보 (dev.epicgames.com).
- 라이브 이후 대부분의 업데이트는 재심사되지만, **심사 중에도 현재 버전은 라이브로 유지된다** ("Update - Pending approval" 상태). 가격·태그·라이선스 선택·지원 플랫폼 같은 "safe edits"는 2025-06-03 개편으로 무심사 반영된다 (dev.epicgames.com, forums.unrealengine.com).
- **공식 SLA는 없다.** 커뮤니티 보고 기준: 업데이트는 대개 ~24시간, 신규 상품 심사는 예측 불가 — 2025년 포럼 보고가 수일에서 "때로는 수개월"까지 걸쳐 있고 반려 가능성도 있다 (forums.unrealengine.com). 첫 코드 플러그인 제출은 수 주 버퍼를 잡고 계획해야 한다.

### 2.6 2025 정책 변화 묶음

| 변화 | 내용 | 출처 |
|---|---|---|
| AI 신고 의무 | AI로 생성한 콘텐츠는 "Created with AI" 자기신고 플래그 의무 | (support.fab.com) |
| NoAI 메타태그 | "Disallow use by generative AI" 체크 → HTML NoAI 메타태그로 학습 스크레이퍼 거부 | (support.fab.com) |
| 개인 세일 | 판매자 자체 할인/프로모션 운영 가능 | (forums.unrealengine.com) |
| FAQ/체인지로그 | 상품당 FAQ 최대 20개 + 릴리스 노트 체인지로그 | (forums.unrealengine.com) |
| 구매검증 리뷰 | 구매자만 별점+텍스트 리뷰 가능 (리뷰 폭격 차단), 구 마켓 리뷰 이관 | (unrealengine.com) |
| 런처 통합 | Epic Games Launcher 안에 Fab, 멀티 DCC export (UE/Unity/C4D/Blender/3ds Max/Maya) | (forums.unrealengine.com) |
| MetaHuman | .mhpkg 포맷 판매 허용 (UE 5.6+) | (forums.unrealengine.com) |
| 검색성 | 택소노미 개편, 태그/퍼블리셔/스타일/기술 특성 필터, "Exclude publisher" 필터 | (unrealengine.com) |

### 2.7 시장 규모와 잘 팔리는 것

- **공식 2025 결산**: 창작자 지급 $24M 초과, 라이브 리스팅 3배 증가로 42만+, 퍼블리셔 2배 증가로 2만+. 전문 구매자로 WWE TV 프로덕션, Sandfall Interactive(Clair Obscur: Expedition 33)가 언급됐다 (unrealengine.com).
- Tools & Plugins는 전용 카테고리(Dialog Systems 등 서브카테고리 포함)가 있다 (fab.com). 가시적으로 성공한 툴 계열: 대화/퀘스트 에디터(Narrative Tales, SUDS Pro, Dialogue Builder), 인벤토리 프레임워크(Inventory Framework Plugin), 어빌리티/전투 프레임워크(Narrative Pro, Ascent Combat Framework), 에디터 생산성(Blueprint Assist), 런타임 임포터(Runtime Audio Importer), 플러그인 빌드 자동화(PluginSmith, Plugin Builder) (fab.com).
- 12개월 판매 회고(2차 출처): "구체적으로 명시된 문제(자동화된 자산 작업, 배치 워크플로, 외부 시스템 연동)를 푸는 툴"이 범용 툴보다 구매자를 잘 유지하며, 신뢰 레버는 상세 기술 문서 + 임베디드 비디오 + 인터랙티브 프리뷰, 출시 후 90일 내 2~3회 계획된 업데이트, Discord/뉴스레터/YouTube 직접 채널 구축이다 (strayspark.studio, 2차 출처). Epic은 카테고리별 판매 순위 데이터를 공개하지 않으므로 "무엇이 팔리는가"는 카테고리 노출과 판매자 보고로부터의 추론이다.

### 2.8 미검증 항목

- 신규 코드 플러그인의 **최소 엔진 버전 지원 규칙**(구 마켓의 "최신 3개 중 1개" 류) 존재 여부 (미검증 — 근거 페이지 support.fab.com의 FAB TECHNICAL REQUIREMENTS가 JS 렌더링이라 자동 수집 불가, 브라우저로 직접 확인 필요).
- Win64 등 **의무 플랫폼 매트릭스** (미검증 — 공식 문서에서 확인 불가, "지원 플랫폼"은 판매자가 설정하는 리스팅 필드).
- **데모 비디오 의무** (미검증 — 문서는 썸네일+복수 스크린샷만 요구, 비디오는 권장 관행).
- 썸네일 등 정확한 미디어 규격 (미검증).

---

## §3 Unity Asset Store 상세

### 3.1 온보딩·지급·세금

- 퍼블리셔 계정은 Publisher Portal(publisher.unity.com)에서 Unity ID로 만들고, 프로필·포트폴리오 작성 후 Asset Store Tools로 첫 패키지를 업로드해 큐레이션 심사에 제출한다. **퍼블리싱 수수료는 없다** (assetstore.unity.com). 별도의 계정 승인 게이트는 없고, 첫 패키지가 심사를 통과하는 것이 사실상의 승인이다.
- **2FA 의무**: 이중 인증을 켜야 payout profile을 만들 수 있다. 지급은 USD 전용 2종 — PayPal(월간, 임계값을 $0까지 설정 가능) 또는 은행 송금(분기, **최소 $250 의무** — 미달분은 다음 분기로 이월) (support.unity.com, docs.unity3d.com).
- **세금 인터뷰**: 지급 전 온라인 세금 인터뷰 필수 — 미국 퍼블리셔는 W-9, 그 외는 W-8BEN. 거주 국가, 개인/법인 구분, TIN(SSN/EIN/ITIN 또는 현지 등가물)이 필요하다 (support.unity.com).

### 3.2 수익 배분과 가격 규칙

- **70/30 고정**: "You retain 70% of all revenue earned." 판매량·연차·카테고리와 무관한 단일 비율이다 (assetstore.unity.com, docs.unity3d.com).
- **최소 가격 $4.99**, 그 위로는 자유 책정 (assetstore.unity.com).
- **New Release 할인**: 최초 출간 시에만 쓸 수 있는 런칭 할인 — $15 이상 에셋 대상, 10%/30%/50%를 1주 또는 2주. Unity는 참여 시 평균 매출이 더 높다고 안내한다 (docs.unity.com, discussions.unity.com).
- **가격 인하 잠금**: 어떤 세일이든 판매가 발생한 후 6주간 가격을 내릴 수 없다 (docs.unity.com).
- **바우처**: 에셋당 연 16개 무료 바우처 코드 (2025-03-18에 12→16 상향, 출간 기념일 기준 리셋) (assetstore.unity.com).
- Unity 주최 세일(Summer Sale, Publisher of the Week 등)은 초청제 — 세일 테마에 맞는 패키지의 퍼블리셔에게 1~2개월 전 연락이 온다 (docs.unity.com).

### 3.3 심사와 기술 요건

- **신규 제출 약 10영업일, 업데이트 약 2영업일** (2026-05 기준 공식 지원 문서). 가속 불가, 큐 위치는 Publisher Portal에서 1일 1회 갱신 표시 (support.unity.com).
- 반려 시 지적 사항을 수정해 재제출한다. **수정 없이 반려 콘텐츠를 반복 재제출하면 퍼블리셔 계정 해지 사유**다 (가이드라인 2.1.h) (assetstore.unity.com).
- 제출 전 검증은 Unity 공식 Asset Store Tools의 pre-submission validator로 한다 (github.com/Unity-Technologies).
- **최소 Unity 버전 2022.3**: 신규 에셋과 기존 에셋의 업데이트 모두 2022.3 이상에서 제출해야 한다 (2023-06에 정한 2021.3에서 상향) (assetstore.unity.com).

### 3.4 패키지 포맷 — .unitypackage vs UPM

| 항목 | .unitypackage | UPM 패키지 |
|---|---|---|
| 크기 상한 | 6GB | **700MB** |
| 전달 | Asset Store Tools 업로더 | Package Manager 통합 |
| 버전 | 스토어 리스팅 버전 | manifest의 **semver 의무** |
| 필수 메타 | 루트 폴더 정리, 경로 150자 미만 | `name/displayName/version/unity/unityRelease/author/description` |
| 코드 요건 | — | **asmdef 최소 1개 의무** |

(assetstore.unity.com). (일반 지식) UPM 패키지의 `Editor/` 폴더는 `Assets/Editor` 같은 매직 폴더가 아니므로, 에디터 전용 스트리핑은 `*.Editor.asmdef`의 includePlatforms=Editor로만 이뤄진다 — Asset Store 제출 이전에 패키지 구조 단계에서 걸리는 고전 함정이다.

### 3.5 에디터 툴 전용 규칙 (조항 번호까지 기억할 것)

에디터 확장을 파는 사람에게 직접 적용되는 가이드라인 조항들 (assetstore.unity.com):

- **2.5.a** 모든 코드는 커스텀 네임스페이스 사용 — Unity 네임스페이스 금지.
- **2.5.d** 코드는 편집/열람 가능해야 한다.
- **2.5.g** 리플렉션으로 Unity Editor internal API에 접근 금지.
- **2.5.h** Editor 6.6부터 스크립트 포함 패키지는 Domain Reload 비활성 상태의 Fast Enter Playmode 지원 의무.
- **2.5.i** 제출 시점 최신 지원 에디터 버전에서 deprecated/obsolete API 경고 금지.
- **2.5.1.a** 메뉴는 `Window/<PackageName>` 같은 기존 메뉴 아래 또는 커스텀 `Tools` 메뉴에 배치.
- **2.5.1.d** 사용자 동의 없이 에디터 밖으로 리다이렉트 금지.
- **2.5.1.e** 사용자 프로젝트에 패키지를 프로그래매틱하게 추가/갱신/제거 금지 (자기 콘텐츠 제외).
- **1.1.f** 외부 자산을 조작하는 툴은 시연용 샘플 자산 동봉 의무.

### 3.6 문서 의무와 신뢰 요소

- **문서는 의무다**: 코드가 있거나 설정이 필요한 패키지는 포괄적 문서 필수. 허용 형식 `.txt/.md/.pdf/.html/.rtf`, 로컬 오프라인 문서 강력 권장, 튜토리얼 비디오는 외부 호스팅(YouTube/Vimeo)만 허용. 스토어 설명에는 핵심 기능·의존성·제약·지원 렌더 파이프라인을 정확히 기재 (3.1.a/3.1.b) (assetstore.unity.com).
- 스토어 메커니즘: 리뷰는 3개 이상 쌓여야 공개 표시되고, 출시 시 14일 "new release" 노출 부스트가 있다 (assetstore.unity.com 관련 커뮤니티 검증). 판매자 모범 관행(VR 툴 퍼블리셔 MindPort): 텍스트 적은 마케팅형 이미지, 긴 영상 하나보다 짧은 자막 영상 여러 개, 설명에서 바로 접근되는 문서, 상품 페이지에 보이는 지원 채널, **업데이트 날짜가 2개월만 지나도 방치된 에셋으로 읽힌다**, $50 이상 에셋이 오히려 더 잘 팔릴 수 있다(Unity가 고마진 상품을 더 밀어준다) (mindport.co).
- 톱 툴 셀러(Odin/Opsive/Pixel Crushers)의 공통 패턴: 독립 문서 사이트 + 공개 포럼 + Discord + 다운로드 가능한 데모. 스토어 요건이 아니라 관측된 패턴이다 (odininspector.com, opsive.com).

### 3.7 가격 벤치마크 (2026-07 검증)

| 툴 | 퍼블리셔 | 가격 |
|---|---|---|
| DOTween Pro | Demigiant | $15 |
| Odin Inspector and Serializer | Sirenix | $55 (785 리뷰, 12,202 즐겨찾기) |
| Amplify Shader Editor | Amplify Creations | 약 $80 |
| Behavior Designer | Opsive | $95 |
| Dialogue System for Unity | Pixel Crushers | 약 $104.50 |
| A* Pathfinding Project Pro | Aron Granberg | $140 |

(assetstore.unity.com). 진지한 에디터 툴은 $15~$140 구간에 몰리고, 최강 셀러는 대부분 $50~$100+ 구간이다. "에디터 툴/유틸리티가 최고 매출 카테고리이고 톱 툴이 월 수만 달러를 번다"는 커뮤니티 주장은 (미검증) — Unity는 카테고리 매출을 공개하지 않으며, 리뷰 수와 장수 셀러의 존재가 방향만 지지한다.

### 3.8 AI 정책과 기타

- **AI 콘텐츠 신고 의무** (2023-06-30부터, 현행): 1.6.a — AI로 전부/일부 생성한 콘텐츠는 전용 "AI description" 필드에 사용 툴을 명시해 투명하게 신고. 1.6.b — 패키지 본질이 정확히 표현되는 한 로고/보조 마케팅 비주얼의 AI 사용 허용. 1.6.c — AI 콘텐츠에 'hand drawn'/'painted' 같은 인간 노력 암시 키워드 금지. AI 사용 여부는 구매자에게 필터 가능한 속성으로 노출된다 (assetstore.unity.com).
- **Unity 6 배지 프로그램은 없다** (미검증 — 공식 페이지에서 발견 실패, 부재로 추정): 호환성은 업로드한 패키지 버전별로 퍼블리셔가 선언하는 방식이며, Unity 6 에디터에서 패키지 버전을 올리는 것이 Unity 6 호환 표시를 얻는 방법이다 (assetstore.unity.com, support.unity.com).

---

## §4 업데이트와 버전 관리의 계약

### 4.1 두 스토어의 버전 계약은 종류가 다르다

| 축 | Fab 코드 플러그인 | Unity Asset Store |
|---|---|---|
| 버전 단위 | **엔진 버전별 재제출** — UE 신버전마다 .uplugin 프로젝트 업로드 의무 (dev.epicgames.com) | **semver** — UPM manifest의 version 필드가 계약 (assetstore.unity.com) |
| 빌드 책임 | Epic이 버전별 컴파일 (소스가 모든 지원 버전에서 무결 컴파일되어야) | 판매자 패키지가 그대로 배포 (구매자 프로젝트에서 컴파일) |
| 검증 도구 | `RunUAT BuildPlugin -Rocket` 버전 매트릭스 (github.com/MuddyTerrain) | Asset Store Tools validator (github.com/Unity-Technologies) |
| 바닥 상승 | 구버전 지원은 허용 (4.27까지) — 락 없음 (forums.unrealengine.com) | 최소 버전이 시간에 따라 올라감 (2021.3 → 2022.3) (assetstore.unity.com) |

(일반 지식) 결과적으로 Fab은 "N개 엔진 버전 × 1개 소스"의 컴파일 매트릭스 관리 문제이고, Unity는 "1개 패키지 × 에디터 진화 추적"의 API 수명 관리 문제다. 전자는 CI가, 후자는 deprecation 대응 규율이 본체다.

### 4.2 업데이트가 곧 신뢰다

- Unity 쪽 관측: **업데이트 날짜가 2개월만 지나도 구매자는 방치(유기)로 읽는다** (mindport.co). 리뷰·문의 응답과 함께 업데이트 주기 자체가 상품 페이지의 신뢰 신호다.
- Fab 쪽 관측: 출시 시점에 **첫 90일 내 2~3회 업데이트를 계획**하고 들어가는 것이 판매자 회고의 권장 사항이다 (strayspark.studio, 2차 출처).
- (일반 지식) 이는 마켓 툴이 "완성품 판매"가 아니라 "유지보수 구독을 선불로 파는 것"에 가깝다는 뜻이다. 출시 결정 = 지속 유지보수 결정이며, 방치할 툴은 애초에 올리지 않는 것이 평판 관리상 낫다.

### 4.3 라이브 버전 유지 중 심사

- Fab: 업데이트가 심사 중("Update - Pending approval")인 동안 기존 버전이 계속 판매된다. 가격/태그/라이선스/지원 플랫폼 변경은 무심사 즉시 반영 (dev.epicgames.com).
- Unity: 업데이트 심사 약 2영업일 동안 기존 버전 판매 유지 (support.unity.com).
- (일반 지식) 따라서 "심사 중 공백" 걱정 없이 업데이트를 밀어도 되지만, 역으로 치명 버그 핫픽스조차 심사 시간(Fab ~24h, Unity ~2영업일)을 우회할 수 없다 — 출시 전 검증 게이트가 유일한 방어선이다.

### 4.4 deprecation 대응은 판매자의 법적 의무 수준이다

Unity 가이드라인은 에디터의 진화를 판매자에게 소급 적용한다:

- **2.5.i**: 제출 시점의 최신 지원 에디터에서 obsolete API 경고가 하나라도 나면 반려 대상. 즉 Unity가 API를 deprecate하면 다음 업데이트 제출 전에 반드시 마이그레이션해야 한다 (assetstore.unity.com).
- **2.5.h**: Editor 6.6부터 스크립트 패키지는 Domain Reload 꺼진 Fast Enter Playmode를 지원해야 한다 — static 상태에 의존하는 에디터 툴 구현 관행 전체를 금지하는 조항이다 (assetstore.unity.com).
- 최소 버전 바닥 상승(→2022.3)은 업데이트에도 적용되므로, 방치했던 에셋을 다시 만지는 순간 현행 규칙 전체가 소급된다 (assetstore.unity.com).

### 4.5 Winters 관점 — 이미 하고 있는 규율의 외부화

이 계약 구조는 Winters가 엔진 내부에서 이미 실행하는 버전 규율과 동형이다:

- `WintersFileHeader`의 `version_major/version_minor` 분리(호환 파괴 vs 하위 호환 추가)와 `STAGE_VERSION_MIN_COMPAT=3`(v3~v5 로드 허용)은 semver 및 "라이브 버전 유지"와 같은 문제의 해법이다 (`.md/interview/tool-development.md` §4, `Shared/GameSim/Definitions/MapDataFormats.h`).
- `Tools/Harness/Check-SharedBoundary.ps1`이 빌드에서 경계 위반을 기계 강제하듯, `RunUAT BuildPlugin` 버전 매트릭스는 Fab 판매자의 기계 게이트다. "규칙은 있는데 기계 강제가 없다"는 UE5.7 감사의 메타패턴 진단은 마켓 판매에서도 그대로 적용된다 — 심사 반려는 사람이 늦게 발견한 규칙 위반이다.
- 면접 서사로도 쓸 수 있다: "마켓플레이스의 버전 계약(엔진 버전별 재컴파일, semver, deprecation 의무)은 제가 `.w*` 포맷에서 겪은 magic/major/minor/MIN_COMPAT 문제의 산업 스케일 버전이다."

---

## §5 협업 관점의 구조 — 팀이 마켓 자산을 수용하는 규율

### 5.1 Winters ingestion 파이프라인

방향은 `.md/architecture/WINTERS_UE_FAB_TOOL_ADOPTION.md`에 고정되어 있다. §0의 한 줄 결론: UE에서 가져올 것은 개별 툴이 아니라 **"자산이 검증된 런타임 계약으로만 게임에 들어간다"는 파이프라인 규율**이다. Fab 자산은 전부 "외부 원료"로 취급하며 단일 경로만 허용한다 (§3 공통 뼈대):

```text
Fab 다운로드 (원본: FBX/glTF/PNG/EXR/WAV/uasset)
  → Tools/AssetStaging/<vendor>/<pack>/   (원본 보존 — 라이선스 메타 필수)
  → import manifest 작성 (무엇을, 어떤 설정으로, 어디로)
  → WintersAssetConverter / 전용 컨버터 cook → .w* 바이너리
  → Validator (포맷/본 수/텍스처 채널/네이밍) → Asset Catalog 등록
  → 런타임은 카탈로그/정의 팩 경유로만 참조
```

실행 주체는 실물로 존재한다: 오프라인 쿠커 `Tools/WintersAssetConverter/WintersAssetConverter.vcxproj`(엔트리는 `Engine/Private/Tools/AssetConverter/main.cpp`), 일괄 변환 `Tools/convert_all_assets.bat`, 검증 게이트의 패턴은 `Engine/Public/FX/Graph/FxGraphValidator.h`(이슈 목록 + topoOrder 반환)가 보여준다. 핵심 규율은 두 가지다 — **Validator가 게이트다**(조용한 기본값 폴백 금지, 불합격 자산은 사라지지 않고 '불합격' 상태+사유로 카탈로그에 남는다), 그리고 **런타임 프레임 코드는 원본 포맷(JSON/FBX)을 절대 읽지 않는다** (같은 문서 §2).

### 5.2 uasset 전용 팩 회피

- uasset 전용 팩은 구매하지 않는다 — UE 에디터 없이 추출이 불가능하고, uasset 파이프라인 호환은 "유지비가 이득을 압도"하기 때문이다. **Fab에서 "Source files included"(FBX 등 소스 포맷 포함) 표기 팩만 채택한다** (`WINTERS_UE_FAB_TOOL_ADOPTION.md` §3-1, §5).
- (일반 지식) 이 규칙은 자체 엔진 팀뿐 아니라 Unity 팀에도 그대로 적용된다 — Fab의 멀티포맷 리스팅에서 자기 파이프라인이 소비 가능한 소스 포맷이 있는지가 구매 기준이다.

### 5.3 애님팩 리타겟 선결

- Fab 애님팩(로코모션/전투/리액션)은 대부분 UE 표준 스켈레톤(UE4/UE5 Mannequin) 기준이라 **리타겟 없이는 무용지물**이다. Winters의 설계: `.wskel`에 리타겟 프로필(본 매핑 테이블 + 리스트 포즈 보정)을 추가하고 AssetConverter에 `--retarget=<profile>` cook 단계를 넣는다 — 런타임 IK가 아니라 **cook 시점 오프라인 베이크**가 결정론/성능 원칙에 맞는다 (`WINTERS_UE_FAB_TOOL_ADOPTION.md` §3-2).
- 도입 순서상의 결정: **리타겟 프로필 설계 완료 전까지 애님팩 구매는 보류**다 (같은 문서 §6). "지금 사면 쓸 수 있는가"를 파이프라인 상태로 판정하는 것 자체가 팀 규율이다.

### 5.4 라이선스/출처 관리 — 협업 필수 필드

staging manifest의 필수 필드 (같은 문서 §3-7):

| 필드 | 이유 |
|---|---|
| 출처 URL | 재구매/업데이트 추적, 분쟁 시 증빙 |
| 라이선스 종류 (Fab Standard/Personal 등) | Personal 티어는 팀 매출 $100k 기준 — 팀이 커지면 Professional 재구매 의무 발생 (dev.epicgames.com) |
| 구매 계정 | 라이선스 귀속 주체 확인 (개인 계정 구매 자산이 팀 프로젝트에 섞이는 사고 방지) |
| 재배포 가능 여부 | 공개 빌드/포트폴리오/레포 커밋 가부 판정 |

원본 추출 자산(Elden 등)은 로컬 검증 전용이라는 기존 경계를 유지하고, 공개 빌드/포트폴리오는 대체 자산으로 간다 (같은 문서 §3-7). (일반 지식) 출처 메타데이터 없는 구매 자산은 시간이 지나면 "이거 어디서 왔고 팀 빌드에 넣어도 되는 물건인가"를 아무도 답할 수 없는 법적·프로덕션 리스크가 된다 — manifest가 팀의 답이다.

### 5.5 툴/플러그인은 이식 대상이 아니라 사양서다

- UE 코드 플러그인은 Winters로 이식하지 않는다. 잘 팔리는 툴(스탯 관리, 대화 시스템, 인벤토리)은 "디자이너가 원하는 워크플로"의 시장 증거이므로, 기능 목록을 참고해 Winters 에디터 패널/정의 팩 스키마를 설계하는 **사양서**로 소비한다 (`WINTERS_UE_FAB_TOOL_ADOPTION.md` §3-6). §2.7의 성공 카테고리 목록은 그래서 구매 목록이 아니라 요구사항 조사 자료다.
- 범용 라이브러리(포맷 파서, 압축 등)만 `.md/build/THIRDPARTY_INTEGRATION_GUIDE.md` 절차로 수용한다.
- 각 자산 클래스의 수용 완료 게이트: 빌드 PASS + 실제 팩 1개를 F5 런타임까지 관통 + Validator가 의도적 불량 입력을 잡는 네거티브 테스트 1개 (같은 문서 §6).

---

## §6 툴 판매자 전략 종합

### 6.1 잘 팔리는 카테고리 — 양쪽 스토어

| 계열 | Fab 사례 | Unity 사례 |
|---|---|---|
| 대화/퀘스트 | Narrative Tales, SUDS Pro, Dialogue Builder (fab.com) | Dialogue System for Unity ~$104.50 (assetstore.unity.com) |
| 어빌리티/전투/AI | Narrative Pro, Ascent Combat Framework (fab.com) | Behavior Designer $95, A* Pro $140 (assetstore.unity.com) |
| 에디터 생산성/인스펙터 | Blueprint Assist (fab.com) | Odin Inspector $55 (assetstore.unity.com) |
| 워크플로/파이프라인 | Runtime Audio Importer, PluginSmith (fab.com) | DOTween Pro $15, Amplify $80 (assetstore.unity.com) |

공통 패턴: **특정 워크플로 문제를 좁고 깊게 푸는 툴이 범용 툴을 이긴다** (strayspark.studio, 2차 출처). 어빌리티 타임라인 에디터는 이 표의 "어빌리티/전투 + 에디터 생산성" 교집합에 정확히 앉는다.

### 6.2 신뢰 요소 체크리스트

두 스토어의 검증 사실을 합치면 출시 체크리스트가 나온다:

- **문서**: Fab "thorough documentation" 의무 (dev.epicgames.com), Unity 코드 패키지 문서 의무 + 오프라인 문서 권장 (assetstore.unity.com). 톱셀러는 독립 문서 사이트까지 간다.
- **영상**: 의무는 아니지만(Fab 기준 미검증, Unity는 외부 호스팅만 허용) 짧은 자막 영상 여러 개가 관행적 정답 (mindport.co).
- **FAQ/체인지로그**: Fab은 상품당 FAQ 20개 + 릴리스 노트를 네이티브 지원 — 채워진 FAQ/체인지로그 자체가 신뢰 신호 (forums.unrealengine.com).
- **커뮤니티**: Discord/포럼/YouTube 직접 채널 (strayspark.studio 2차, opsive.com 패턴).
- **데모**: 다운로드 가능한 데모/샘플 — Unity는 외부 자산 조작 툴에 샘플 동봉이 아예 의무(1.1.f).
- **리뷰**: 두 스토어 모두 구매검증 리뷰 체제 — Unity는 3개+부터 공개, Fab은 2025년 별점+텍스트 도입. 초기 리뷰 확보가 발견성의 관문이다.
- **업데이트 리듬**: §4.2 — 2개월 공백이면 방치로 읽힌다.

### 6.3 가격 존

- Unity 에디터 툴 실측 존: **$15(단품 유틸) ~ $140(전문가 시스템)**, 최강 셀러 $50~$100+ (assetstore.unity.com). $50 이상이 오히려 유리할 수 있다는 관측 (mindport.co).
- Fab: .99 규칙 + Personal/Professional 이중 티어 의무 (dev.epicgames.com). (일반 지식) 이중 티어는 가격 차별화 도구로 쓸 수 있다 — Personal을 낮게, Professional을 높게 설정해 인디 침투와 스튜디오 마진을 동시에 잡는 패턴이 일반적이다.
- 실수령 비교: 명목 88% vs 70%에 더해, Fab은 결제 수수료·환전 후 $49.99 기준 약 81~86% 실수령 보고 (strayspark.studio, 2차 출처). 같은 가격이면 Fab 마진이 유의미하게 높다.

### 6.4 이중 스토어 전략 — "1 콘셉트, 2 구현, 2 스토어"

연구의 종합 분석(단일 출처 인용이 아닌 이 세션의 검증 사실 기반 합성):

- **에디터 툴은 멀티포맷 리스팅으로 커버되지 않는다.** Fab의 멀티포맷은 콘텐츠(모델/오디오)용이고, 깊은 에디터 UI 툴은 엔진 종속(Unity C# UI Toolkit+ScriptableObject vs UE C++/Slate+FAssetEditorToolkit)이라 **하나의 설계 코어 + 두 개의 별도 구현**이 현실이다 (docs.unity3d.com / dev.epicgames.com의 각 툴 표면 검증 사실로부터의 분석).
- **발견성은 비대칭이다.** Unity 에디터 툴의 전체 고객은 Unity 생태계 안에 있고 Asset Store가 에디터에 통합된 기본 검색지다. Fab의 에디터 통합은 UE/UEFN을 향하므로 Unity 툴을 Fab에서 파는 것은 발견성이 약하다 — **에디터 툴 발견성은 Asset Store 우위, 마진은 Fab 우위** (분석).
- **순서**: 시장 검증 관점의 분석은 "툴 구매 문화가 크고 가격 벤치마크가 뚜렷한 Unity에서 콘셉트를 증명한 뒤, 88% 마진과 덜 포화된 Fab 툴 카테고리로 UE판을 낸다"이다 (분석). Winters의 고정 로드맵은 이와 접합된다: **① 에디터 코어 선행(CEditorTransaction 공용 승격 + AtomicWriteFile + CFxGraphValidator 저장 배선 + 격리 프리뷰 월드) → ② 타임라인 툴 → ③ 언리얼 이식(UDataAsset + 커스텀 에셋 에디터) → ④ Fab 출시** (`.md/interview/tool-development.md` §12, Q31). 데이터 모델을 엔진 불가지론적(프레임 인덱스 기반 순수 데이터)으로 유지하면 ③의 이식 비용이 데이터가 아니라 UI 계층에만 발생한다 — Unity 쪽에서 Timeline 커스텀 트랙 확장 대신 순수 EditorWindow+ScriptableObject 노선(Route B)이 검토된 이유가 이것이다 (deepwiki.com의 GAS-for-Unity 분석 + 이 세션 분석).

### 6.5 어빌리티 타임라인 에디터의 시장 위치

- **편집 대상 데이터가 이미 실물이다**: `Shared/GameSim/Definitions/SkillDef.h`의 `visualCastFrame/visualRecoveryFrame`(+stage2 변형)/`lockDurationSec`/`visualPlaySpeed`/훅 id 스키마가 타임라인 에디터가 편집할 바로 그 데이터이고, 14개 챔피언의 authored 값이 `Client/Private/GameObject/Champion/*/​*_Registration.cpp`에 있으며, `Client/Private/UI/SkillTimingPanel.cpp`(SliderFloat로 g_SkillTable을 라이브 튜닝하고 값을 소스로 역기록)가 이 툴의 살아 있는 조상이다.
- **차별화 서사는 서버 권위 분리다**: Winters는 시각 프레임(클라이언트 `SkillDef`)과 판정 틱(서버 권위 — `Shared/GameSim/Registries/ChampionGameData/ChampionGameDataDB.h`의 `ResolveBasicAttackWindupTicks`)을 구조적으로 분리해 두었다. 시판 어빌리티 툴(Ascent Combat Framework, ABC Ability & Combat Toolkit 등)이 대부분 클라이언트 단일 세계관인 것과 달리, "타임라인이 어디까지 진실을 만들 수 있는가"를 아는 저자라는 포지셔닝이 가능하다 — "툴은 gameplay truth를 만들 수 없다"(`WINTERS_UE_FAB_TOOL_ADOPTION.md` §2)가 그대로 제품 철학이 된다.
- **경쟁 구도에서의 자리**: 기존 성공작들은 런타임 프레임워크(전투 시스템 전체)를 팔고, 순수 authoring 툴은 상대적으로 비어 있다. 타임라인 에디터는 "런타임을 강요하지 않는 authoring 툴 + 엔진별 얇은 어댑터"로 좁게 정의할 때 §6.1의 교집합에서 경쟁을 피한다 (분석).
- **가격 위치**: Unity 기준 에디터 툴 존($15~140)에서, 문서·데모·업데이트 리듬을 갖춘 전문 툴로 $50 전후 진입이 벤치마크와 정합한다 (assetstore.unity.com 벤치마크로부터의 분석). Fab판은 Personal/Professional 이중 티어로 같은 콘셉트를 상향 가격에 낼 수 있다.
- **일정 감각**: 신규 심사 리드타임(Unity ~10영업일, Fab 수일~수개월)과 첫 90일 2~3회 업데이트 관행을 로드맵 ④에 미리 산입한다. 출시일이 아니라 "제출일 + 심사 버퍼 + 초기 업데이트 3회"가 실제 커밋이다.

---

## 부록 — 이 문서가 의존하는 검증 경계

- 웹 사실의 검증 시점은 2026-07-10이다. 심사 기간·정책·가격은 변동 가능성이 크므로, 실제 제출 전 재확인 대상: Fab 최소 엔진 버전 규칙(support.fab.com의 FAB TECHNICAL REQUIREMENTS 문서를 브라우저로 직접 확인), Unity 제출 가이드라인 현행판, 두 스토어의 심사 큐 현황.
- (미검증) 표기 항목 요약: Fab 최소 엔진 버전 규칙 / Win64 의무 / 비디오 의무 / 미디어 규격, Unity 카테고리별 매출 주장, Unity 6 공식 배지 프로그램 부재(발견 실패 기반 추정), Fab의 AI 라벨링이 Unity 1.6과 등가인지 여부.
- 2차 출처(공식 아님) 의존 항목: 실수령률(~81–86%), 90일 업데이트 관행, Fab 셀러 지원 응답 시간 — 모두 strayspark.studio 판매자 회고 기반이다.
