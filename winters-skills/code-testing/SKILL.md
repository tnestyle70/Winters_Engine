---
name: code-testing
description: >
  WintersEngine 코드 assertion 기반 검증.
  "테스트 해줘", "검증해줘", "빌드 전 점검" 등에 트리거.
  code-scaffolding 이후 자동 트리거.
---

# Skill: Code Testing — WintersEngine

## 실행 순서
1. `references/test-patterns.md` 읽기
2. 대상 분류: `[Engine]` / `[ECS]` / `[Renderer]` / `[PublicAPI]` / `[Shader]`
3. 해당 assertion 전부 실행 → FAIL 시 수정 후 재실행 (최대 3회)
4. 전 항목 PASS 후 → code-review 연계 → 최종 코드 + vcxproj 등록 안내

## 출력 형식
```
[Test Run #N — ClassName / 분류]
✅ ASSERT-E01  설명
❌ ASSERT-R03  설명 → 수정 필요
결과: N/M PASS
```

## 핵심 규칙
- **통과 전 코드 제시 금지** (생성→실패→수정→통과→제시)
- 3회 FAIL 시 중단, 설계 재검토 안내
