# PR-0024 Final Verification Report

검증일: 2026-07-20 (Asia/Seoul)

기준 커밋: `5dfa184 feat: add progress tracking and reporting`

## 결론

PR-0024의 canonical output, SHA-256, known digits, BBP 교차 검증,
verification manifest, CLI 및 progress 연결은 계획된 정상·손상·경계·대용량
시나리오를 통과했다. 확인된 결함 두 건은 검증 중 수정했으며, 최종 GCC
sanitizer 전체 회귀, Clang 교차 검증 및 Clang Static Analyzer에서 남은 오류는
발견되지 않았다.

## 검증 환경

- OS: Linux 7.1.3-2-cachyos x86_64
- GCC: 16.1.1
- Clang: 22.1.8
- C++: C++20
- Sanitizer: AddressSanitizer, UndefinedBehaviorSanitizer
- 정적 분석: Clang scan-build (`No bugs found`)

## 기능 및 정확성 검증

- 정상 1, 10, 50, 100, 1,000자리 실제 Chudnovsky 결과 검증
- canonical `3.<digits>` 구조와 정확한 단일 LF 파일 framing 검증
- 잘못된 prefix, 길이, non-digit, trailing data 거부
- 요청 정밀도에 맞춘 known decimal 반올림 비교
- SHA-256 표준 vector, multi-block, 100만 `a`, 임의 chunk 분할 검증
- 메모리와 파일 canonical SHA-256 일치
- 저장 manifest SHA-256과 파일 중간·마지막 digit 변조 교차 검증
- 알려진 초기 64개 및 1001번째 BBP hexadecimal digit 검증
- decimal-to-hex GMP 추출과 known hexadecimal prefix 비교
- deterministic BBP sample의 expected/output digit 교차 검증
- BBP mismatch 및 수치 경계 `inconclusive` 처리 검증
- BBP가 적용 불가능한 작은 출력에서 skipped 단계의 중립 집계 검증
- verification manifest canonical round-trip, CRC32C 손상 탐지 검증
- atomic rename 이전 장애에서 기존 manifest 보존 검증

## CLI 및 progress 검증

- 실제 1,000자리 CLI 계산·출력·검증·manifest 생성 성공
- 성공 출력에 manifest 경로와 lowercase SHA-256 표시
- progress 활성/비활성 결과 파일 및 SHA-256 동일
- JSON progress에서 schema v2 `verifying_output` 관측
- text/JSON reporter의 `verifying_output` 직렬화 검증
- 실패 report의 stage, status, structured reason 출력 경로 검증
- 계산기 standalone progress lifecycle과 CLI 소유 lifecycle 호환 검증

## 대용량 및 메모리 검증

128 MiB fractional-digit synthetic 파일을 생성하여 다음 두 streaming pass를
수행했다.

1. canonical output 구조 검사
2. canonical prefix SHA-256

두 경로 모두 고정 64 KiB buffer를 사용했다. ASan/UBSan 실행에서 output 크기
비례 할당 없이 RSS 증가 허용치 16 MiB 이내 조건을 통과했다. 전체 대용량
테스트 시간은 약 8.6~8.9초였다.

## 전체 회귀 결과

- GCC Debug + ASan + UBSan: 49/49 통과
- Clang 관련 통합·대용량·reporter 검증: 통과
- Clang scan-build: `No bugs found`
- `git diff --check`: 통과

## Release 성능

조건: memory final verification, deterministic BBP sample 8개, warm-up 1회
제외 5회 median.

| Decimal digits | Calculation | Verification | Verification / Calculation |
|---:|---:|---:|---:|
| 1,000 | 61 us | 314 us | 5.15x |
| 10,000 | 1,018 us | 5,539 us | 5.44x |
| 100,000 | 26,020 us | 62,121 us | 2.39x |

100,000자리에서 검증 절대 시간은 약 62 ms다. 상대 비용의 대부분은 서로 다른
위치를 독립 계산하는 BBP sample에서 발생한다. SHA-256과 known-prefix 검사는
선형 streaming이며, decimal GMP 값은 한 번만 구성해 sample 사이에서 재사용한다.

## 검증 중 수정한 결함

1. Known digits가 요청 자릿수의 반올림 결과 대신 단순 절단 prefix를 비교하던
   문제를 수정했다.
2. 적용 불가능한 BBP `skipped`가 다른 필수 단계의 pass를 덮어 전체 결과를
   skipped로 만들던 집계 정책을 수정했다. 모든 단계가 skipped일 때만 overall
   skipped를 유지한다.

## 남은 관찰점

기능·정확성 측면의 release blocker는 발견되지 않았다. 향후 수백만~수십억 자리
검증 latency를 더 줄이려면 독립 BBP sample 병렬 실행이 우선 최적화 대상이다.
현재 정책은 sample 수를 최대 32개로 제한하고 기본 8개를 사용해 비용을
명시적으로 제어한다.
