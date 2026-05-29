---
name: code-review
description: >
  WintersEngine 아키텍처 컨벤션과 GOTCHAS 기준으로 C++20 코드 검토.
  "리뷰해줘", "문제 없는지 봐줘", "아키텍처 위반 있어?" 등에 트리거.
  code-scaffolding 스킬 이후 자동 트리거.
---

# Skill: Code Review — WintersEngine

## 실행 순서
1. `references/gotchas.md` 읽기
2. **[생각 흐름]** 출력 → **[Review]** 출력
3. FAIL 항목 있으면 수정 코드 제안

## 생각 흐름 (반드시 출력)
```
1. 클래스 역할 / 2. 엔진 레이어 / 3. 소유권 모델
4. 스레드 안전성 / 5. 데이터 설계 / 6. 외부 의존성
7. 의심 지점 / 8. GOTCHAS 매핑 번호
```

## Review 체크리스트 (✅/❌/⚠️)
```
── 네이밍 ──  #21 파일 C금지/클래스 C필수, #22 멤버 접두사 m_/m_p/m_b/m_f/m_v, #23 타입 alias(u32_t 등)
── 메모리 ──  #1 smart ptr, #12 멤버 초기화
── RHI ──     #2 DX11 타입 미노출, #11 Win32 미노출
── ECS ──     #3 Component 순수데이터, #4 CommandBuffer, #5 순회중 Remove 금지, #15 SystemAccess
── C++20 ──   #6 싱글톤 금지, #7 using namespace, #8 noexcept, #9 nodiscard, #10 enum class, #17 constexpr
── DX11 ──    #13 셰이더 에러처리, #18 GPU호출 위치, #19 경로 하드코딩, #20 HRESULT
── 구조 ──    #14 ImGui 적극 사용(하드코딩 금지, Debug/Editor에 튜닝 UI 의무), #16 Query 성능
```

## 빈번한 컴파일 에러 패턴
- 헤더 블록 뒤 `;` 누락 (`class Foo { ... }` → `};`)
- `operator=` 에서 `=` 누락 (`operator(...)`)
- 동일 스코프 변수 이중 선언 (`D3D11_BUFFER_DESC desc = {}; ... D3D11_BUFFER_DESC desc = {};`)
- include path 미갱신 (파일 rename 후 기존 include 잔존)
- ISystem override 시 시그니처 불일치 (`Update` vs `Execute`, `World` vs `CWorld`)
